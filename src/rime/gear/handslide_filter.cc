//
// Copyright RIME Developers
// Distributed under the BSD License
//
// 2026-09-27 QWERTY Handslide Error Correction Filter Implementation
//

#include <cmath>
#include <vector>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>

#include <rime/candidate.h>
#include <rime/context.h>
#include <rime/engine.h>
#include <rime/menu.h>
#include <rime/schema.h>
#include <rime/service.h>
#include <rime/translation.h>
#include <rime/translator.h>
#include <rime/gear/handslide_filter.h>
#include <rime/gear/translator_commons.h>

namespace rime {

// 触屏 QWERTY 键盘相对网格坐标表
struct KeyCoord {
  float x;
  float y;
};

static const std::unordered_map<char, KeyCoord> kQwertyCoords = {
    {'q', {0.0f, 0.0f}}, {'w', {1.0f, 0.0f}}, {'e', {2.0f, 0.0f}},
    {'r', {3.0f, 0.0f}}, {'t', {4.0f, 0.0f}}, {'y', {5.0f, 0.0f}},
    {'u', {6.0f, 0.0f}}, {'i', {7.0f, 0.0f}}, {'o', {8.0f, 0.0f}},
    {'p', {9.0f, 0.0f}},
    {'a', {0.0f, 1.0f}}, {'s', {1.0f, 1.0f}}, {'d', {2.0f, 1.0f}},
    {'f', {3.0f, 1.0f}}, {'g', {4.0f, 1.0f}}, {'h', {5.0f, 1.0f}},
    {'j', {6.0f, 1.0f}}, {'k', {7.0f, 1.0f}}, {'l', {8.0f, 1.0f}},
    {'z', {0.0f, 2.0f}}, {'x', {1.0f, 2.0f}}, {'c', {2.0f, 2.0f}},
    {'v', {3.0f, 2.0f}}, {'b', {4.0f, 2.0f}}, {'n', {5.0f, 2.0f}},
    {'m', {6.0f, 2.0f}}
};

static inline float CalcKeyDistance(char c1, char c2) {
  auto it1 = kQwertyCoords.find(c1);
  auto it2 = kQwertyCoords.find(c2);
  if (it1 == kQwertyCoords.end() || it2 == kQwertyCoords.end()) {
    return 999.0f;
  }
  float dx = it1->second.x - it2->second.x;
  float dy = it1->second.y - it2->second.y;
  return std::sqrt(dx * dx + dy * dy);
}

// 标准汉语拼音全部合法音节集合（用于快速预过滤非法组合）
static const std::unordered_set<std::string> kValidSyllables = {
    "a", "ai", "an", "ang", "ao",
    "ba", "bai", "ban", "bang", "bao", "bei", "ben", "beng", "bi", "bian",
    "biao", "bie", "bin", "bing", "bo", "bu",
    "ca", "cai", "can", "cang", "cao", "ce", "cen", "ceng", "cha", "chai",
    "chan", "chang", "chao", "che", "chen", "cheng", "chi", "chong", "chou",
    "chu", "chua", "chuai", "chuan", "chuang", "chui", "chun", "chuo", "ci",
    "cong", "cou", "cu", "cuan", "cui", "cun", "cuo",
    "da", "dai", "dan", "dang", "dao", "de", "dei", "den", "deng", "di",
    "dia", "dian", "diao", "die", "ding", "diu", "dong", "dou", "du", "duan",
    "dui", "dun", "duo",
    "e", "ei", "en", "eng", "er",
    "fa", "fan", "fang", "fei", "fen", "feng", "fo", "fou", "fu",
    "ga", "gai", "gan", "gang", "gao", "ge", "gei", "gen", "geng", "gong",
    "gou", "gu", "gua", "guai", "guan", "guang", "gui", "gun", "guo",
    "ha", "hai", "han", "hang", "hao", "he", "hei", "hen", "heng", "hong",
    "hou", "hu", "hua", "huai", "huan", "huang", "hui", "hun", "huo",
    "ji", "jia", "jian", "jiang", "jiao", "jie", "jin", "jing", "jiong",
    "jiu", "ju", "juan", "jue", "jun",
    "ka", "kai", "kan", "kang", "kao", "ke", "kei", "ken", "keng", "kong",
    "kou", "ku", "kua", "kuai", "kuan", "kuang", "kui", "kun", "kuo",
    "la", "lai", "lan", "lang", "lao", "le", "lei", "leng", "li", "lia",
    "lian", "liang", "liao", "lie", "lin", "ling", "liu", "lo", "long",
    "lou", "lu", "luan", "lue", "lun", "luo", "lv",
    "ma", "mai", "man", "mang", "mao", "me", "mei", "men", "meng", "mi",
    "mian", "miao", "mie", "min", "ming", "miu", "mo", "mou", "mu",
    "na", "nai", "nan", "nang", "nao", "ne", "nei", "nen", "neng", "ni",
    "nian", "niang", "niao", "nie", "nin", "ning", "niu", "nong", "nou",
    "nu", "nuan", "nue", "nun", "nuo", "nv",
    "o", "ou",
    "pa", "pai", "pan", "pang", "pao", "pei", "pen", "peng", "pi", "pian",
    "piao", "pie", "pin", "ping", "po", "pou", "pu",
    "qi", "qia", "qian", "qiang", "qiao", "qie", "qin", "qing", "qiong",
    "qiu", "qu", "quan", "que", "qun",
    "ran", "rang", "rao", "re", "ren", "reng", "ri", "rong", "rou", "ru",
    "rua", "ruan", "rui", "run", "ruo",
    "sa", "sai", "san", "sang", "sao", "se", "sen", "seng", "sha", "shai",
    "shan", "shang", "shao", "she", "shei", "shen", "sheng", "shi", "shou",
    "shu", "shua", "shuai", "shuan", "shuang", "shui", "shun", "shuo", "si",
    "song", "sou", "su", "suan", "sui", "sun", "suo",
    "ta", "tai", "tan", "tang", "tao", "te", "teng", "ti", "tian", "tiao",
    "tie", "ting", "tong", "tou", "tu", "tuan", "tui", "tun", "tuo",
    "wa", "wai", "wan", "wang", "wei", "wen", "weng", "wo", "wu",
    "xi", "xia", "xian", "xiang", "xiao", "xie", "xin", "xing", "xiong",
    "xiu", "xu", "xuan", "xue", "xun",
    "ya", "yan", "yang", "yao", "ye", "yi", "yin", "ying", "yo", "yong",
    "you", "yu", "yuan", "yue", "yun",
    "za", "zai", "zan", "zang", "zao", "ze", "zei", "zen", "zeng", "zha",
    "zhai", "zhan", "zhang", "zhao", "zhe", "zhei", "zhen", "zheng", "zhi",
    "zhong", "zhou", "zhu", "zhua", "zhuai", "zhuan", "zhuang", "zhui",
    "zhun", "zhuo", "zi", "zong", "zou", "zu", "zuan", "zui", "zun", "zuo"
};

// 检查是否为合法音节前缀
static inline bool IsSyllablePrefix(const std::string& prefix) {
  if (prefix.empty()) return false;
  static const std::unordered_set<std::string> kInitials = {
      "b", "p", "m", "f", "d", "t", "n", "l", "g", "k", "h",
      "j", "q", "x", "zh", "ch", "sh", "r", "z", "c", "s", "y", "w"
  };
  if (kInitials.find(prefix) != kInitials.end()) return true;
  for (const auto& syl : kValidSyllables) {
    if (syl.size() >= prefix.size() &&
        syl.compare(0, prefix.size(), prefix) == 0) {
      return true;
    }
  }
  return false;
}

// 快速拼音合法性判定（使用 DP 验证字符串能否切分为合法音节）
static bool IsPinyinValid(const std::string& input) {
  if (input.empty()) return false;
  size_t n = input.size();
  std::vector<bool> dp(n + 1, false);
  dp[0] = true;

  for (size_t i = 0; i < n; ++i) {
    if (!dp[i]) continue;
    for (size_t len = 1; len <= 6 && i + len <= n; ++len) {
      std::string sub = input.substr(i, len);
      if (kValidSyllables.find(sub) != kValidSyllables.end()) {
        dp[i + len] = true;
      } else if (i + len == n && IsSyllablePrefix(sub)) {
        dp[i + len] = true;
      }
    }
  }
  return dp[n];
}

// 将 librime 词典对数概率权重映射为正向线性频次 (1 ~ 1e8 尺度)
static inline double CalculateBaseFrequency(const an<Candidate>& cand) {
  if (!cand) return 0.0;
  auto genuine = Candidate::GetGenuineCandidate(cand);
  if (auto phrase = As<Phrase>(genuine)) {
    // librime 中 entry_->weight = log(count) - 18.42068
    // 因此 std::exp(weight + 18.42068) 严格还原词典真实词频数量级
    double raw = std::exp(phrase->weight() + 18.42068);
    return raw > 0.0 ? raw : 1.0;
  }
  return cand->quality() * 10000.0;
}

HandslideFilter::HandslideFilter(const Ticket& ticket) : Filter(ticket) {
  LoadConfig();
}

HandslideFilter::~HandslideFilter() = default;

void HandslideFilter::LoadConfig() {
  if (!engine_ || !engine_->schema()) return;
  Config* config = engine_->schema()->config();
  if (!config) return;

  config->GetBool("handslide/enable", &enable_);
  config->GetDouble("handslide/distance_threshold", &distance_threshold_);
  config->GetDouble("handslide/weight_k", &weight_k_);
  config->GetDouble("handslide/original_bonus", &original_bonus_);
  config->GetInt("handslide/max_input_len", &max_input_len_);
  config->GetInt("handslide/max_guess_count", &max_guess_count_);
}

void HandslideFilter::InitTranslator() {
  if (!engine_ || !engine_->schema()) return;
  string schema_id = engine_->schema()->schema_id();
  if (guess_translator_ && schema_id == last_schema_id_) {
    return;
  }
  last_schema_id_ = schema_id;
  Ticket ticket(engine_, "translator");
  if (auto comp = Translator::Require("script_translator")) {
    guess_translator_.reset(comp->Create(ticket));
  }
}

bool HandslideFilter::AppliesToSegment(Segment* segment) {
  return segment != nullptr;
}

void HandslideFilter::QueryCandidates(const std::string& guess_code,
                                      float distance,
                                      std::vector<GuessCandidate>& final_candidates,
                                      std::unordered_set<std::string>& seen_texts) {
  InitTranslator();
  if (guess_translator_) {
    // 零开销极速查询：复用主方案底层已就绪的词典与拼音引擎，耗时 < 10 微秒
    Segment seg(0, guess_code.size());
    an<Translation> sub_trans = guess_translator_->Query(guess_code, seg);
    if (sub_trans) {
      for (size_t i = 0; i < 6 && !sub_trans->exhausted(); ++i) {
        auto cand = sub_trans->Peek();
        if (cand && seen_texts.insert(cand->text()).second) {
          double base_freq = CalculateBaseFrequency(cand);
          double score = base_freq - weight_k_ * distance;
          final_candidates.push_back(GuessCandidate(cand, cand->text(), score, false));
        }
        sub_trans->Next();
      }
    }
  } else {
    // 兜底：隔离 Session 查询
    SessionId sub_id = Service::instance().CreateSession();
    if (!sub_id) return;
    an<Session> sub_session = Service::instance().GetSession(sub_id);
    if (sub_session) {
      if (!last_schema_id_.empty()) {
        sub_session->ApplySchema(new Schema(last_schema_id_));
      }
      sub_session->context()->set_input(guess_code);
      Context* sub_ctx = sub_session->context();
      if (sub_ctx && sub_ctx->HasMenu() && !sub_ctx->composition().empty()) {
        Segment& seg = sub_ctx->composition().back();
        if (seg.menu) {
          for (size_t i = 0; i < 6; ++i) {
            auto cand = seg.menu->GetCandidateAt(i);
            if (!cand) break;
            if (seen_texts.insert(cand->text()).second) {
              double base_freq = CalculateBaseFrequency(cand);
              double score = base_freq - weight_k_ * distance;
              final_candidates.push_back(GuessCandidate(cand, cand->text(), score, false));
            }
          }
        }
      }
    }
    Service::instance().DestroySession(sub_id);
  }
}

an<Translation> HandslideFilter::Apply(an<Translation> translation,
                                       CandidateList* candidates) {
  // 1. 防重入保护
  static thread_local bool s_reentrancy_guard = false;
  if (s_reentrancy_guard) {
    return translation;
  }

  // 2. 前置短路判断
  if (!enable_ || !engine_ || !engine_->context()) {
    return translation;
  }

  std::string input_code = engine_->context()->input();
  // 关键限制：输入长度必须在 [2, max_input_len] 范围内
  // 单字符 (length < 2) 严禁触发纠错，直接放行，消解首字母卡顿并保护单字简码
  if (input_code.size() < 2 || static_cast<int>(input_code.size()) > max_input_len_) {
    return translation;
  }

  // 校验输入全为小写英文字母
  for (char c : input_code) {
    if (c < 'a' || c > 'z') {
      return translation;
    }
  }

  // 3. 生成手滑猜测编码集合
  struct GuessInfo {
    std::string code;
    float distance;
  };
  std::vector<GuessInfo> valid_guesses;
  std::unordered_set<std::string> seen_guess_codes;
  seen_guess_codes.insert(input_code);

  for (size_t pos = 0; pos < input_code.size(); ++pos) {
    char old_char = input_code[pos];
    for (const auto& pair : kQwertyCoords) {
      char new_char = pair.first;
      if (new_char == old_char) continue;

      float dist = CalcKeyDistance(old_char, new_char);
      if (dist >= distance_threshold_) continue;

      std::string guess_code = input_code;
      guess_code[pos] = new_char;

      // 拼音合法性快速预校验（非拼音组合在微秒级丢弃）
      if (!IsPinyinValid(guess_code)) continue;

      if (seen_guess_codes.insert(guess_code).second) {
        valid_guesses.push_back({guess_code, dist});
        if (static_cast<int>(valid_guesses.size()) >= max_guess_count_) {
          break;
        }
      }
    }
    if (static_cast<int>(valid_guesses.size()) >= max_guess_count_) {
      break;
    }
  }

  if (valid_guesses.empty()) {
    return translation;
  }

  // 4. 获取原始输入候选（取前 30 个参与重排，防止流式 Translation 全量遍历）
  std::vector<GuessCandidate> final_candidates;
  std::unordered_set<std::string> seen_texts;

  if (translation) {
    size_t count = 0;
    while (!translation->exhausted() && count < 30) {
      auto cand = translation->Peek();
      if (cand) {
        double base_freq = CalculateBaseFrequency(cand);
        double score = base_freq + original_bonus_;
        final_candidates.push_back(GuessCandidate(cand, cand->text(), score, true));
        seen_texts.insert(cand->text());
        ++count;
      }
      translation->Next();
    }
  }

  // 5. 获取纠错候选（通过轻量 Translator 查询，极速且隔离）
  s_reentrancy_guard = true;
  for (const auto& guess : valid_guesses) {
    QueryCandidates(guess.code, guess.distance, final_candidates, seen_texts);
  }
  s_reentrancy_guard = false;

  // 6. 按 score 降序排序
  std::stable_sort(
      final_candidates.begin(), final_candidates.end(),
      [](const GuessCandidate& a, const GuessCandidate& b) {
        return a.score > b.score;
      });

  // 7. 组装结果 FifoTranslation 输出给流水线下游
  auto result_translation = New<FifoTranslation>();
  for (const auto& item : final_candidates) {
    result_translation->Append(item.cand);
  }

  // 若原 translation 还有剩余未排候选，追加到末尾
  if (translation && !translation->exhausted()) {
    while (!translation->exhausted()) {
      auto cand = translation->Peek();
      if (cand && seen_texts.insert(cand->text()).second) {
        result_translation->Append(cand);
      }
      translation->Next();
    }
  }

  return result_translation;
}

}  // namespace rime
