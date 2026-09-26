//
// Copyright RIME Developers
// Distributed under the BSD License
//
// 2026-09-27 QWERTY Handslide Error Correction Filter
//

#ifndef RIME_HANDSLIDE_FILTER_H_
#define RIME_HANDSLIDE_FILTER_H_

#include <vector>
#include <string>
#include <unordered_set>
#include <rime/candidate.h>
#include <rime/common.h>
#include <rime/filter.h>
#include <rime/translation.h>

namespace rime {

class Translator;

struct GuessCandidate {
  an<Candidate> cand;
  std::string text;
  double score;
  bool is_original;

  GuessCandidate(an<Candidate> c, const std::string& t, double s, bool orig)
      : cand(c), text(t), score(s), is_original(orig) {}
};

class HandslideFilter : public Filter {
 public:
  explicit HandslideFilter(const Ticket& ticket);
  ~HandslideFilter() override;

  an<Translation> Apply(an<Translation> translation,
                        CandidateList* candidates) override;

  bool AppliesToSegment(Segment* segment) override;

 protected:
  void LoadConfig();
  void InitTranslator();
  void QueryCandidates(const std::string& guess_code,
                       float distance,
                       std::vector<GuessCandidate>& final_candidates,
                       std::unordered_set<std::string>& seen_texts);

 private:
  bool enable_ = true;
  double distance_threshold_ = 2.0;
  double weight_k_ = 10.0;
  double original_bonus_ = 2000.0;
  int max_input_len_ = 8;
  int max_guess_count_ = 20;

  // 纯净复用型 Translator，零开销获取候选词频，杜绝反复初始化重型万象 Schema
  the<Translator> guess_translator_;
  string last_schema_id_;
};

}  // namespace rime

#endif  // RIME_HANDSLIDE_FILTER_H_
