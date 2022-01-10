#include "modernizer/path_pattern.h"

#include "gtest/gtest.h"

TEST(PathPatternTest, Empty) {
  std::optional<modernizer::PathPattern> pattern =
      modernizer::PathPattern::Create("");
  ASSERT_FALSE(pattern);
}

TEST(PathPatternTest, Simple) {
  std::optional<modernizer::PathPattern> pattern =
      modernizer::PathPattern::Create("/api");
  ASSERT_TRUE(pattern);

  ASSERT_TRUE(pattern->Match("api/array_view.h"));
  ASSERT_FALSE(pattern->Match("call/audio_state.h"));
}

TEST(PathPatternTest, Complex) {
  std::optional<modernizer::PathPattern> pattern =
      modernizer::PathPattern::Create("/api:include/");
  ASSERT_TRUE(pattern);

  ASSERT_TRUE(pattern->Match("api/array_view.h"));
  ASSERT_TRUE(pattern->Match("modules/include/module.h"));
  ASSERT_FALSE(pattern->Match("call/audio_state.h"));
}

TEST(PathPatternTest, Negate) {
  std::optional<modernizer::PathPattern> pattern =
      modernizer::PathPattern::Create("/:!/third_party");
  ASSERT_TRUE(pattern);

  ASSERT_TRUE(pattern->Match("api/array_view.h"));
  ASSERT_FALSE(pattern->Match("third_party/libyuv/include/libyuv.h"));
}

TEST(PathPatternTest, Negate2) {
  std::optional<modernizer::PathPattern> pattern =
      modernizer::PathPattern::Create("!/:/rtc_base:/api");
  ASSERT_TRUE(pattern);

  ASSERT_TRUE(pattern->Match("api/array_view.h"));
  ASSERT_FALSE(pattern->Match("third_party/libyuv/include/libyuv.h"));
  ASSERT_FALSE(
      pattern->Match("sdk/objc/api/peerconnection/RTCPeerConnection.h"));
}
