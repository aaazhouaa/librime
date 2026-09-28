//
// Copyright RIME Developers
// Distributed under the BSD License
//
// Created by nameoverflow on 2018/11/14.
//

#include "corrector.h"
#include <algorithm>
#include <numeric>
#include <queue>
#include <rime/schema.h>
#include <rime/service.h>
#include <rime/ticket.h>

using namespace rime;
using namespace corrector;

static hash_map<char, hash_set<char>> keyboard_map = {
    {'1', {'2', 'q', 'w'}},
    {'2', {'1', '3', 'q', 'w', 'e'}},
    {'3', {'2', '4', 'w', 'e', 'r'}},
    {'4', {'3', '5', 'e', 'r', 't'}},
    {'5', {'4', '6', 'r', 't', 'y'}},
    {'6', {'5', '7', 't', 'y', 'u'}},
    {'7', {'6', '8', 'y', 'u', 'i'}},
    {'8', {'7', '9', 'u', 'i', 'o'}},
    {'9', {'8', '0', 'i', 'o', 'p'}},
    {'0', {'9', '-', 'o', 'p', '['}},
    {'-', {'0', '=', 'p', '[', ']'}},
    {'=', {'-', '[', ']', '\\'}},
    // ── 字母行：按触屏 QWERTY 真实交错坐标（row0 偏移 0 / row1 偏移 0.5 / row2 偏移 1.5）
    //    与欧氏距离阈值 1.5 生成，覆盖同行紧邻 + 相邻行斜向。
    //    上游仅收录同行水平邻键，手机端最高发的上下/斜向误按因而完全无法纠错。
    //    坐标与阈值同本项目历史 handslide_filter 实现（commit 6d8e7aca），已经真机调校；
    //    生成脚本 scripts/neighbor/gen_keyboard_map.py（带双向对称性自检）。
    //    注：数字行与标点行（[ ] \ ; ' - = , . /）未纳入坐标模型，仍沿用上游原状。
    //    因此字母行重写后，上游两条「字母→标点」邻接被移除：p→[ 与 l→; 。
    //    影响可忽略：真机字母键盘上无这些标点键，且它们不在拼音 alphabet 内，
    //    音节图本就不会经过；移除后邻键表 = 坐标规则的纯函数，可被脚本逐条核验。
    //    （scripts/neighbor/check_keyboard_map.py 会输出该差异，便于评审）
    {'q', {'a', 'w'}},
    {'w', {'a', 'e', 'q', 's'}},
    {'e', {'d', 'r', 's', 'w'}},
    {'r', {'d', 'e', 'f', 't'}},
    {'t', {'f', 'g', 'r', 'y'}},
    {'y', {'g', 'h', 't', 'u'}},
    {'u', {'h', 'i', 'j', 'y'}},
    {'i', {'j', 'k', 'o', 'u'}},
    {'o', {'i', 'k', 'l', 'p'}},
    {'p', {'l', 'o'}},
    {'[', {'p', ']'}},
    {']', {'[', '\\'}},
    {'\\', {']'}},
    {'a', {'q', 's', 'w', 'z'}},
    {'s', {'a', 'd', 'e', 'w', 'x', 'z'}},
    {'d', {'c', 'e', 'f', 'r', 's', 'x', 'z'}},
    {'f', {'c', 'd', 'g', 'r', 't', 'v', 'x'}},
    {'g', {'b', 'c', 'f', 'h', 't', 'v', 'y'}},
    {'h', {'b', 'g', 'j', 'n', 'u', 'v', 'y'}},
    {'j', {'b', 'h', 'i', 'k', 'm', 'n', 'u'}},
    {'k', {'i', 'j', 'l', 'm', 'n', 'o'}},
    {'l', {'k', 'm', 'o', 'p'}},
    {';', {'l', '\''}},
    {'\'', {';'}},
    {'z', {'a', 'd', 's', 'x'}},
    {'x', {'c', 'd', 'f', 's', 'z'}},
    {'c', {'d', 'f', 'g', 'v', 'x'}},
    {'v', {'b', 'c', 'f', 'g', 'h'}},
    {'b', {'g', 'h', 'j', 'n', 'v'}},
    {'n', {'b', 'h', 'j', 'k', 'm'}},
    {'m', {'j', 'k', 'l', 'n'}},
    {',', {'m', '.'}},
    {'.', {',', '/'}},
    {'/', {'.'}},
};

void DFSCollect(const string& origin,
                const string& current,
                size_t ed,
                Script& result);

Script SymDeleteCollector::Collect(size_t edit_distance) {
  // TODO: specifically for 1 length str
  Script script;

  for (auto& v : syllabary_) {
    DFSCollect(v, v, edit_distance, script);
  }

  return script;
}

void DFSCollect(const string& origin,
                const string& current,
                size_t ed,
                Script& result) {
  if (ed <= 0)
    return;
  for (size_t i = 0; i < current.size(); i++) {
    string temp = current;
    temp.erase(i, 1);
    Spelling spelling(origin);
    spelling.properties.tips = origin;
    result[temp].push_back(spelling);
    DFSCollect(origin, temp, ed - 1, result);
  }
}

void EditDistanceCorrector::ToleranceSearch(const Prism& prism,
                                            const string& key,
                                            Corrections* results,
                                            size_t threshold) {
  if (key.empty())
    return;
  size_t key_len = key.length();

  vector<size_t> jump_pos(key_len);

  auto match_next = [&](size_t& node, size_t& point) -> bool {
    auto res_val = trie_->traverse(key.c_str(), node, point, point + 1);
    if (res_val == -2)
      return false;
    if (res_val >= 0) {
      for (auto accessor = QuerySpelling(res_val); !accessor.exhausted();
           accessor.Next()) {
        auto origin = accessor.properties().tips;
        auto current_input = key.substr(0, point);
        if (origin == current_input) {
          continue;  // early termination: this comparison is O(n)
        }
        auto distance = RestrictedDistance(origin, current_input, threshold);
        if (distance <= threshold) {  // only trace near words
          SyllableId corrected;
          if (prism.GetValue(origin, &corrected)) {
            results->Alter(corrected, {distance, corrected, point});
          }
        }
      }
    }
    return true;
  };

  // pass through origin key, cache trie nodes
  size_t max_match = 0;
  for (size_t next_node = 0; max_match < key_len;) {
    jump_pos[max_match] = next_node;
    if (!match_next(next_node, max_match))
      break;
  }

  // start at the next position of deleted char
  for (size_t del_pos = 0; del_pos <= max_match; del_pos++) {
    size_t next_node = jump_pos[del_pos];
    for (size_t key_point = del_pos + 1; key_point < key_len;) {
      if (!match_next(next_node, key_point))
        break;
    }
  }
}

inline uint8_t SubstCost(char left, char right) {
  if (left == right)
    return 0;
  if (keyboard_map[left].find(right) != keyboard_map[left].end()) {
    return 1;
  }
  return 4;
}

// This nice O(min(m, n)) implementation is from
// https://en.wikibooks.org/wiki/Algorithm_Implementation/Strings/Levenshtein_distance#C++
Distance EditDistanceCorrector::LevenshteinDistance(const std::string& s1,
                                                    const std::string& s2) {
  // To change the type this function manipulates and returns, change
  // the return type and the types of the two variables below.
  auto s1len = (size_t)s1.size();
  auto s2len = (size_t)s2.size();

  auto column_start = (decltype(s1len))1;

  auto column = new decltype(s1len)[s1len + 1];
  std::iota(column + column_start - 1, column + s1len + 1, column_start - 1);

  for (auto x = column_start; x <= s2len; x++) {
    column[0] = x;
    auto last_diagonal = x - column_start;
    for (auto y = column_start; y <= s1len; y++) {
      auto old_diagonal = column[y];
      auto possibilities = {column[y] + 1, column[y - 1] + 1,
                            last_diagonal + SubstCost(s1[y - 1], s2[x - 1])};

      column[y] = (std::min)(possibilities);
      last_diagonal = old_diagonal;
    }
  }
  auto result = column[s1len];
  delete[] column;
  return result;
}

// L's distance with transposition allowed
Distance EditDistanceCorrector::RestrictedDistance(const std::string& s1,
                                                   const std::string& s2,
                                                   Distance threshold) {
  auto len1 = s1.size(), len2 = s2.size();
  vector<size_t> d((len1 + 1) * (len2 + 1));

  auto index = [len1, len2](size_t i, size_t j) { return i * (len2 + 1) + j; };

  d[0] = 0;
  for (size_t i = 1; i <= len1; ++i)
    d[index(i, 0)] = i * 2;
  for (size_t i = 1; i <= len2; ++i)
    d[index(0, i)] = i * 2;

  for (size_t i = 1; i <= len1; ++i) {
    auto min_d = threshold + 1;
    for (size_t j = 1; j <= len2; ++j) {
      d[index(i, j)] =
          (std::min)({d[index(i - 1, j)] + 2, d[index(i, j - 1)] + 2,
                      d[index(i - 1, j - 1)] +
                          SubstCost(s1[i - 1], s2[j - 1])});
      if (i > 1 && j > 1 && s1[i - 2] == s2[j - 1] && s1[i - 1] == s2[j - 2]) {
        d[index(i, j)] = (std::min)(d[index(i, j)], d[index(i - 2, j - 2)] + 2);
      }
      min_d = (std::min)(min_d, d[index(i, j)]);
    }
    // early termination: do not continue if too far
    if (min_d > threshold)
      return min_d;
  }
  return (uint8_t)d[index(len1, len2)];
}
bool EditDistanceCorrector::Build(const Syllabary& syllabary,
                                  const Script* script,
                                  uint32_t dict_file_checksum,
                                  uint32_t schema_file_checksum) {
  Syllabary correct_syllabary;
  if (script && !script->empty()) {
    for (auto& v : *script) {
      correct_syllabary.insert(v.first);
    }
  } else {
    correct_syllabary = syllabary;
  }

  SymDeleteCollector collector(correct_syllabary);
  auto correction_script = collector.Collect((size_t)1);

  return Prism::Build(syllabary, &correction_script, dict_file_checksum,
                      schema_file_checksum);
}
EditDistanceCorrector::EditDistanceCorrector(const path& file_path)
    : Prism(file_path) {}

void NearSearchCorrector::ToleranceSearch(const Prism& prism,
                                          const string& key,
                                          Corrections* results,
                                          size_t threshold) {
  if (key.empty())
    return;

  using record = struct {
    size_t node_pos;
    size_t idx;
    size_t distance;
    char ch;
  };

  std::queue<record> queue;
  queue.push({0, 0, 0, key[0]});
  for (auto subst : keyboard_map[key[0]]) {
    queue.push({0, 0, 1, subst});
  }
  for (; !queue.empty(); queue.pop()) {
    auto& rec = queue.front();
    char ch = rec.ch;
    char& exchange(const_cast<char*>(key.c_str())[rec.idx]);
    std::swap(ch, exchange);
    auto val =
        prism.trie().traverse(key.c_str(), rec.node_pos, rec.idx, rec.idx + 1);
    std::swap(ch, exchange);

    if (val == -2)
      continue;
    if (val >= 0) {
      results->Alter(val, {rec.distance, val, rec.idx});
    }
    if (rec.idx < key.size()) {
      queue.push({rec.node_pos, rec.idx, rec.distance, key[rec.idx]});
      if (rec.distance < threshold) {
        for (auto subst : keyboard_map[key[rec.idx]]) {
          queue.push({rec.node_pos, rec.idx, rec.distance + 1, subst});
        }
      }
    }
  }
}
void CorrectorComponent::Unified::ToleranceSearch(const Prism& prism,
                                                  const string& key,
                                                  Corrections* results,
                                                  size_t tolerance) {
  for (auto& c : contents) {
    c->ToleranceSearch(prism, key, results, tolerance);
  }
}
CorrectorComponent::CorrectorComponent()
    : resolver_(Service::instance().CreateDeployedResourceResolver(
          {"corrector", "", ".correction.bin"})) {}

Corrector* CorrectorComponent::Create(const Ticket& ticket) noexcept {
  // Don't use edit distance based correction for now.
#if 0
  if (!ticket.schema) return nullptr;
  Config* config = ticket.schema->config();
  string prism_name;
  if (!config->GetString(ticket.name_space + "/prism", &prism_name)) {
    config->GetString(ticket.name_space + "/dictionary", &prism_name);
  }

  auto file_path = resolver_->ResolvePath(prism_name);

  auto ed_corrector = New<EditDistanceCorrector>(file_path);
  if (ed_corrector->Load()) {
    return Combine(New<NearSearchCorrector>(), ed_corrector);
  } else {
    return new NearSearchCorrector();
  }
#endif
  return new NearSearchCorrector();
}
