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

#if defined(__ANDROID__)
#include <android/log.h>
#define LOG_TAG "XimeHandslide"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#else
#define LOGI(...)
#define LOGD(...)
#define LOGE(...)
#endif

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

// 触屏 QWERTY 键盘真实交错相对网格坐标表（第二行向右错位0.5，第三行向右错位1.5）
struct KeyCoord {
  float x;
  float y;
};

static const std::unordered_map<char, KeyCoord> kQwertyCoords = {
    {'q', {0.0f, 0.0f}}, {'w', {1.0f, 0.0f}}, {'e', {2.0f, 0.0f}},
    {'r', {3.0f, 0.0f}}, {'t', {4.0f, 0.0f}}, {'y', {5.0f, 0.0f}},
    {'u', {6.0f, 0.0f}}, {'i', {7.0f, 0.0f}}, {'o', {8.0f, 0.0f}},
    {'p', {9.0f, 0.0f}},
    {'a', {0.5f, 1.0f}}, {'s', {1.5f, 1.0f}}, {'d', {2.5f, 1.0f}},
    {'f', {3.5f, 1.0f}}, {'g', {4.5f, 1.0f}}, {'h', {5.5f, 1.0f}},
    {'j', {6.5f, 1.0f}}, {'k', {7.5f, 1.0f}}, {'l', {8.5f, 1.0f}},
    {'z', {1.5f, 2.0f}}, {'x', {2.5f, 2.0f}}, {'c', {3.5f, 2.0f}},
    {'v', {4.5f, 2.0f}}, {'b', {5.5f, 2.0f}}, {'n', {6.5f, 2.0f}},
    {'m', {7.5f, 2.0f}}
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

// 标准汉语拼音全部合法音节集合
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

// 完整全拼切分判定：检查字符串能否【完整切分为合法汉语拼音音节】
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
      }
    }
  }
  return dp[n];
}

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

// 检查字符串能否完整切分为合法拼音，或为「完整音节 + 一个合法前缀」。
// 例如 "ni"、"nih" 均视为合法中间态，应直接放行（不触发纠错）。
static bool IsPinyinValidOrPrefix(const std::string& input) {
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
      }
    }
  }
  if (dp[n]) return true;
  for (size_t i = 0; i <= n; ++i) {
    if (dp[i] && IsSyllablePrefix(input.substr(i))) {
      return true;
    }
  }
  return false;
}

// 获取原生对数词频
static inline double CalculateBaseFrequency(const an<Candidate>& cand) {
  if (!cand) return -20.0;
  auto genuine = Candidate::GetGenuineCandidate(cand);
  if (auto phrase = As<Phrase>(genuine)) {
    return phrase->weight();
  }
  return cand->quality() - 20.0;
}

HandslideFilter::HandslideFilter(const Ticket& ticket) : Filter(ticket) {
  LoadConfig();
  InitNeighborMap();
  LOGI("HandslideFilter initialized: enable=%d, dist_th=%.2f, weight_k=%.2f",
       enable_, distance_threshold_, weight_k_);
}

HandslideFilter::~HandslideFilter() = default;

void HandslideFilter::LoadConfig() {
  if (!engine_ || !engine_->schema()) return;
  Config* config = engine_->schema()->config();
  if (!config) return;

  config->GetBool("handslide/enable", &enable_);
  config->GetDouble("handslide/distance_threshold", &distance_threshold_);
  config->GetDouble("handslide/weight_k", &weight_k_);
  config->GetInt("handslide/max_input_len", &max_input_len_);
  config->GetInt("handslide/max_guess_count", &max_guess_count_);
}

void HandslideFilter::InitNeighborMap() {
  neighbor_map_.clear();
  for (const auto& p1 : kQwertyCoords) {
    char c1 = p1.first;
    auto& list = neighbor_map_[c1];
    for (const auto& p2 : kQwertyCoords) {
      char c2 = p2.first;
      if (c1 == c2) continue;
      float d = CalcKeyDistance(c1, c2);
      if (d < distance_threshold_) {
        list.push_back({c2, d});
      }
    }
  }
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

// 核心查询逻辑：严格保证 Preedit 干净隔离，杜绝幽灵拼音污染与残缺单字冲撞
void HandslideFilter::QueryCandidates(const std::string& guess_code,
                                      float distance,
                                      std::vector<GuessCandidate>& final_candidates,
                                      std::unordered_set<std::string>& seen_texts) {
  InitTranslator();
  if (!guess_translator_ || !engine_ || !engine_->context()) return;

  std::string raw_input = engine_->context()->input();
  Segment seg(0, guess_code.size());
  seg.tags.insert("abc");
  an<Translation> sub_trans = guess_translator_->Query(guess_code, seg);
  if (!sub_trans) return;

  for (size_t i = 0; i < 3 && !sub_trans->exhausted(); ++i) {
    auto cand = sub_trans->Peek();
    if (cand) {
      size_t span = cand->end() - cand->start();

      // 规则 1：【全码完整匹配硬性约束】
      // 纠错必须是覆盖全部或主要输入长度的完整词组！
      // 绝对禁止残缺单字（如输入 fansile 纠出单字“但”，或者 nihso 纠出单字“你”）！
      if (span >= guess_code.size() && seen_texts.insert(cand->text()).second) {
        double base_freq = CalculateBaseFrequency(cand);
        // 全码词组给予精确匹配加分，抗衡简拼缩写句
        double score = base_freq - weight_k_ * distance + 4.0;

        // 规则 2：【彻底隔离幽灵 Preedit 污染】
        // 用 SimpleCandidate 包装，preedit 严格继承用户当前真实输入的字符 raw_input！
        // 绝对不向外部流水线暴露任何猜测代码（如 dansile），气泡永远稳固显示用户按键！
        auto safe_cand = New<SimpleCandidate>(
            cand->type(),
            0,
            raw_input.size(),
            cand->text(),
            cand->comment(),
            raw_input);

        final_candidates.push_back(
            GuessCandidate(safe_cand, cand->text(), score, false));
        LOGI("Guess [%s]: accepted full-span candidate '%s' (span=%zu), score=%.2f",
             guess_code.c_str(), cand->text().c_str(), span, score);
      }
    }
    sub_trans->Next();
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
  // 单字母直接放行防首键延迟；超出 max_input_len_ 保护长句性能
  if (input_code.size() < 2 || static_cast<int>(input_code.size()) > max_input_len_) {
    return translation;
  }

  for (char c : input_code) {
    if (c < 'a' || c > 'z') {
      return translation;
    }
  }

  // 合法拼音或合法中间态（完整音节 + 合法前缀）直接放行，
  // 避免正常输入与打字中间态走完整纠错流水线造成卡顿与无谓重排。
  if (IsPinyinValidOrPrefix(input_code)) {
    return translation;
  }

  // 3. 生成单字符替换猜测
  struct GuessInfo {
    std::string code;
    float distance;
  };
  std::vector<GuessInfo> valid_guesses;
  std::unordered_set<std::string> seen_guess_codes;
  seen_guess_codes.insert(input_code);

  for (size_t pos = 0; pos < input_code.size(); ++pos) {
    char old_char = input_code[pos];
    auto it = neighbor_map_.find(old_char);
    if (it == neighbor_map_.end()) continue;

    for (const auto& nb : it->second) {
      char new_char = nb.first;
      float dist = nb.second;

      std::string guess_code = input_code;
      guess_code[pos] = new_char;

      // 快速拼音合法性预校验（必须能够完整切分为标准拼音）
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

  LOGI("Input [%s]: generated %zu valid guesses",
       input_code.c_str(), valid_guesses.size());

  // 4. 获取原始输入候选
  std::vector<GuessCandidate> final_candidates;
  std::unordered_set<std::string> seen_texts;

  if (translation) {
    size_t count = 0;
    while (!translation->exhausted() && count < 12) {
      auto cand = translation->Peek();
      if (cand) {
        double base_freq = CalculateBaseFrequency(cand);
        final_candidates.push_back(GuessCandidate(cand, cand->text(), base_freq, true));
        seen_texts.insert(cand->text());
        ++count;
      }
      translation->Next();
    }
  }

  // 5. 获取纠错候选（纯 C++ 极速查询，微秒级无感）
  s_reentrancy_guard = true;
  for (const auto& guess : valid_guesses) {
    QueryCandidates(guess.code, guess.distance, final_candidates, seen_texts);
  }
  s_reentrancy_guard = false;

  // 6. 分离纠错候选与原始候选：纠错候选按得分降序，原始候选保持原相对顺序
  std::vector<GuessCandidate> guess_candidates;
  std::vector<GuessCandidate> original_candidates;
  guess_candidates.reserve(final_candidates.size());
  original_candidates.reserve(final_candidates.size());
  for (const auto& item : final_candidates) {
    if (item.is_original) {
      original_candidates.push_back(item);
    } else {
      guess_candidates.push_back(item);
    }
  }
  std::stable_sort(
      guess_candidates.begin(), guess_candidates.end(),
      [](const GuessCandidate& a, const GuessCandidate& b) {
        return a.score > b.score;
      });

  // 7. 组装结果 FifoTranslation：纠错候选置顶，原始候选保持原序随后
  auto result_translation = New<FifoTranslation>();
  for (const auto& item : guess_candidates) {
    result_translation->Append(item.cand);
  }
  for (const auto& item : original_candidates) {
    result_translation->Append(item.cand);
  }

  if (!guess_candidates.empty()) {
    LOGI("Top 1 after Handslide sort: '%s' (score=%.2f, is_original=%d)",
         guess_candidates[0].text.c_str(), guess_candidates[0].score,
         guess_candidates[0].is_original);
  }

  return result_translation;
}

}  // namespace rime
