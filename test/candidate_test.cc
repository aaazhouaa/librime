//
// Copyright RIME Developers
// Distributed under the BSD License
//
// Candidate 解包协议测试：GetGenuineCandidate 对 Shadow/Uniquified 的
// 既有语义保持不变；实现 genuine() 虚协议的装饰型包装被透传解包，
// 使 Memory::OnDeleteEntry（长按删词）能拿到底层 Phrase。
//
#include <gtest/gtest.h>
#include <rime/candidate.h>
#include <rime/gear/translator_commons.h>

using namespace rime;

namespace {

class DecoratedCandidate : public SimpleCandidate {
 public:
  DecoratedCandidate(an<Candidate> item, const string& preedit)
      : SimpleCandidate(item->type() + "'deco",
                        item->start(), item->end(),
                        item->text(), item->comment(), preedit),
        item_(item) {}

  an<Candidate> genuine() const override { return item_; }

 private:
  an<Candidate> item_;
};

class CandidateGenuineTest : public ::testing::Test {};

TEST_F(CandidateGenuineTest, PlainCandidateIsItsOwnGenuine) {
  auto c = New<SimpleCandidate>("word", 0, 1, "test");
  EXPECT_EQ(c, Candidate::GetGenuineCandidate(c));
}

TEST_F(CandidateGenuineTest, ShadowCandidateUnwrappedToItem) {
  auto phrase = New<SimpleCandidate>("phrase", 0, 2, "测试");
  auto shadow = New<ShadowCandidate>(phrase, "simplified");
  EXPECT_EQ(phrase, Candidate::GetGenuineCandidate(shadow));
}

TEST_F(CandidateGenuineTest, UniquifiedCandidateUnwrappedToItem) {
  auto phrase = New<SimpleCandidate>("phrase", 0, 2, "测试");
  auto unique = New<UniquifiedCandidate>(phrase, "unique");
  EXPECT_EQ(phrase, Candidate::GetGenuineCandidate(unique));
}

TEST_F(CandidateGenuineTest, GenuineProtocolUnwrapsDecoratedWrapper) {
  // 既非 Shadow 也非 Uniquified 的装饰型包装（如 T9 的 preedit 转换包装）
  // 经 genuine() 协议透传到内层候选。
  auto phrase = New<SimpleCandidate>("phrase", 0, 2, "测试");
  auto decorated = New<DecoratedCandidate>(phrase, "ce shi");
  EXPECT_EQ(phrase, Candidate::GetGenuineCandidate(decorated));
  EXPECT_EQ(1u, Candidate::GetGenuineCandidates(decorated).size());
  EXPECT_EQ(phrase, Candidate::GetGenuineCandidates(decorated).front());
}

TEST_F(CandidateGenuineTest, GenuineProtocolAfterUniquifier) {
  // 真实链路：t9_filter 包装在外，uniquifier 在外层再包一次。
  auto phrase = New<SimpleCandidate>("phrase", 0, 2, "测试");
  auto decorated = New<DecoratedCandidate>(phrase, "ce shi");
  auto unique = New<UniquifiedCandidate>(decorated, "unique");
  EXPECT_EQ(phrase, Candidate::GetGenuineCandidate(unique));
}

TEST_F(CandidateGenuineTest, ShadowSemanticsUnchangedForOtherSchemas) {
  // 既有语义回归锚点：多层 Shadow 只解一层（与历史行为一致），
  // 保证其他输入方案下解包结果不变。
  auto phrase = New<SimpleCandidate>("phrase", 0, 2, "测试");
  auto inner = New<ShadowCandidate>(phrase, "a");
  auto outer = New<ShadowCandidate>(inner, "b");
  EXPECT_EQ(inner, Candidate::GetGenuineCandidate(outer));
}

TEST_F(CandidateGenuineTest, NullCandidateToleratedLikeLegacy) {
  // 回归锚点（真机崩溃：无候选段输入 vii/vvv 触发 SIGSEGV）：历史上
  // GetGenuineCandidate 对空候选原样返回、不崩溃，调用方（引擎、Lua 绑定、
  // 万象等方案脚本）依赖判空兜底。genuine 协议解包必须保持这一行为。
  an<Candidate> nil;
  EXPECT_EQ(nullptr, Candidate::GetGenuineCandidate(nil).get());
  auto genuines = Candidate::GetGenuineCandidates(nil);
  ASSERT_EQ(1u, genuines.size());
  EXPECT_EQ(nullptr, genuines.front().get());
}

}  // namespace
