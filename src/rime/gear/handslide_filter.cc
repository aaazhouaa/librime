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
  // 单声母允许作为音节前缀
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
    // 汉语拼音单音节长度范围通常为 1 到 6
    for (size_t len = 1; len <= 6 && i + len <= n; ++len) {
      std::string sub = input.substr(i, len);
      if (kValidSyllables.find(sub) != kValidSyllables.end()) {
        dp[i + len] = true;
      } else if (i + len == n && IsSyllablePrefix(sub)) {
        // 末尾允许不完整前缀
        dp[i + len] = true;
      }
    }
  }
  return dp[n];
}

HandslideFilter::HandslideFilter(const Ticket& ticket) : Filter(ticket) {
  LoadConfig();
}

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

bool HandslideFilter::AppliesToSegment(Segment* segment) {
  return segment != nullptr;
}

struct GuessCandidate {
  an<Candidate> cand;
  std::string text;
  double score;
  bool is_original;

  GuessCandidate(an<Candidate> c, const std::string& t, double s, bool orig)
      : cand(c), text(t), score(s), is_original(orig) {}
};

an<Translation> HandslideFilter::Apply(an<Translation> translation,
                                       CandidateList* candidates) {
  // 1. 防重入保护（临时子 Session 内部翻译时直接透传放行）
  static thread_local bool s_reentrancy_guard = false;
  if (s_reentrancy_guard) {
    return translation;
  }

  // 2. 前置短路判断
  if (!enable_ || !engine_ || !engine_->context()) {
    return translation;
  }

  std::string input_code = engine_->context()->input();
  if (input_code.empty() || static_cast<int>(input_code.size()) > max_input_len_) {
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

      // 拼音合法性快速预校验
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

  // 如果没有合法的猜测编码，直接返回原始翻译
  if (valid_guesses.empty()) {
    return translation;
  }

  // 4. 获取原始输入候选并计算 score = base_freq + original_bonus
  std::vector<GuessCandidate> final_candidates;
  std::unordered_set<std::string> seen_texts;

  if (translation) {
    while (!translation->exhausted()) {
      auto cand = translation->Peek();
      if (cand) {
        double base_freq = cand->quality();
        auto genuine = Candidate::GetGenuineCandidate(cand);
        if (auto phrase = As<Phrase>(genuine)) {
          base_freq = phrase->weight();
        }
        double score = base_freq + original_bonus_;
        final_candidates.push_back({cand, cand->text(), score, true});
        seen_texts.insert(cand->text());
      }
      translation->Next();
    }
  }

  // 5. 获取纠错候选（通过临时隔离 Session 翻译，严格防止内存泄漏）
  s_reentrancy_guard = true;
  std::string current_schema_id = engine_->schema() ? engine_->schema()->schema_id() : "";

  for (const auto& guess : valid_guesses) {
    SessionId sub_id = Service::instance().CreateSession();
    if (!sub_id) continue;

    an<Session> sub_session = Service::instance().GetSession(sub_id);
    if (sub_session) {
      if (!current_schema_id.empty()) {
        sub_session->ApplySchema(new Schema(current_schema_id));
      }
      sub_session->context()->set_input(guess.code);
      Context* sub_ctx = sub_session->context();
      if (sub_ctx && sub_ctx->HasMenu() && !sub_ctx->composition().empty()) {
        Segment& seg = sub_ctx->composition().back();
        if (seg.menu) {
          // 每个纠错猜测最多提取前 6 个有效候选
          for (size_t i = 0; i < 6; ++i) {
            auto cand = seg.menu->GetCandidateAt(i);
            if (!cand) break;

            if (seen_texts.find(cand->text()) == seen_texts.end()) {
              double base_freq = cand->quality();
              auto genuine = Candidate::GetGenuineCandidate(cand);
              if (auto phrase = As<Phrase>(genuine)) {
                base_freq = phrase->weight();
              }
              // 打分公式：score = base_freq - weight_k * d
              double score = base_freq - weight_k_ * guess.distance;
              final_candidates.push_back({cand, cand->text(), score, false});
              seen_texts.insert(cand->text());
            }
          }
        }
      }
    }
    // 立即销毁临时会话释放全部资源
    Service::instance().DestroySession(sub_id);
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

  return result_translation;
}

}  // namespace rime
