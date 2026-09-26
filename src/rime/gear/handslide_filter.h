//
// Copyright RIME Developers
// Distributed under the BSD License
//
// 2026-09-27 QWERTY Handslide Error Correction Filter
//

#ifndef RIME_HANDSLIDE_FILTER_H_
#define RIME_HANDSLIDE_FILTER_H_

#include <rime/candidate.h>
#include <rime/common.h>
#include <rime/filter.h>

namespace rime {

class HandslideFilter : public Filter {
 public:
  explicit HandslideFilter(const Ticket& ticket);

  an<Translation> Apply(an<Translation> translation,
                        CandidateList* candidates) override;

  bool AppliesToSegment(Segment* segment) override;

 protected:
  void LoadConfig();

 private:
  bool enable_ = true;
  double distance_threshold_ = 2.0;
  double weight_k_ = 10.0;
  double original_bonus_ = 2000.0;
  int max_input_len_ = 8;
  int max_guess_count_ = 20;
};

}  // namespace rime

#endif  // RIME_HANDSLIDE_FILTER_H_
