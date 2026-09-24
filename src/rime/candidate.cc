//
// Copyright RIME Developers
// Distributed under the BSD License
//
// 2013-01-06 GONG Chen <chen.sst@gmail.com>
//
#include <rime/candidate.h>

namespace rime {

static an<Candidate> UnpackShadowCandidate(const an<Candidate>& cand) {
  auto shadow = As<ShadowCandidate>(cand);
  return shadow ? shadow->item() : cand;
}

// 对实现 genuine() 虚协议的装饰型包装继续解包（仅外层追加，逐层透传）。
// 注意：Shadow/Uniquified 的单层解包语义保持原样，不递归展开，
// 确保既有输入方案下 GetGenuineCandidate 的结果与历史行为完全一致。
// 空候选按历史行为原样返回：无候选段（如非法编码）的 GetSelectedCandidate
// 为 null，调用方依赖判空兜底，此处不得解引用。
static an<Candidate> UnwrapGenuineProtocol(const an<Candidate>& cand) {
  if (!cand)
    return cand;
  auto result = cand;
  while (auto wrapped = result->genuine()) {
    result = wrapped;
  }
  return result;
}

an<Candidate> Candidate::GetGenuineCandidate(const an<Candidate>& cand) {
  auto uniquified = As<UniquifiedCandidate>(cand);
  return UnwrapGenuineProtocol(
      UnpackShadowCandidate(uniquified ? uniquified->items().front() : cand));
}

vector<of<Candidate>> Candidate::GetGenuineCandidates(
    const an<Candidate>& cand) {
  vector<of<Candidate>> result;
  if (auto uniquified = As<UniquifiedCandidate>(cand)) {
    for (const auto& item : uniquified->items()) {
      result.push_back(UnwrapGenuineProtocol(UnpackShadowCandidate(item)));
    }
  } else {
    result.push_back(UnwrapGenuineProtocol(UnpackShadowCandidate(cand)));
  }
  return result;
}

int Candidate::compare(const Candidate& other) {
  // the one nearer to the beginning of segment comes first
  int k = start_ - other.start_;
  if (k != 0)
    return k;
  // then the longer comes first
  k = end_ - other.end_;
  if (k != 0)
    return -k;
  // compare quality
  double qdiff = quality_ - other.quality_;
  if (qdiff != 0.)
    return (qdiff > 0.) ? -1 : 1;
  // draw
  return 0;
}

}  // namespace rime
