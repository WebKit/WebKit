/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>
#include <utility>

#include "modules/desktop_capture/desktop_frame.h"
#include "modules/desktop_capture/desktop_geometry.h"
#include "modules/desktop_capture/frame_texture.h"
#include "modules/desktop_capture/shared_desktop_frame.h"
#include "test/gtest.h"

namespace webrtc {

namespace {

// Returns a non-null, non-invalid sentinel FrameTexture::Handle for testing.
// On Windows Handle is void*, on other platforms it is int.
FrameTexture::Handle MakeHandle() {
  constexpr uintptr_t kValue = 0x1234;
#if defined(WEBRTC_WIN)
  return reinterpret_cast<FrameTexture::Handle>(kValue);
#else
  return static_cast<FrameTexture::Handle>(kValue);
#endif
}

// A concrete FrameTexture subclass for testing purposes. Unlike the
// platform-specific DXGIFrameTexture, this doesn't own any real GPU resources.
class FakeFrameTexture : public FrameTexture {
 public:
  explicit FakeFrameTexture(Handle handle) : FrameTexture(handle) {}
  ~FakeFrameTexture() override = default;
};

// A texture-backed DesktopFrame with nullptr data, mimicking DXGIDesktopFrame.
class TextureDesktopFrame : public DesktopFrame {
 public:
  TextureDesktopFrame(DesktopSize size,
                      std::unique_ptr<FakeFrameTexture> texture)
      : DesktopFrame(size,
                     /*stride=*/0,
                     FOURCC_ARGB,
                     /*data=*/nullptr,
                     /*shared_memory=*/nullptr,
                     texture.get()),
        owned_texture_(std::move(texture)) {}

  ~TextureDesktopFrame() override = default;

 private:
  std::unique_ptr<FakeFrameTexture> owned_texture_;
};

}  // namespace

TEST(FrameTextureTest, StoresHandle) {
  const FrameTexture::Handle handle = MakeHandle();
  FakeFrameTexture texture(handle);
  EXPECT_EQ(texture.handle(), handle);
}

TEST(DesktopFrameTest, DefaultTextureIsNull) {
  auto frame = std::make_unique<BasicDesktopFrame>(DesktopSize(10, 10));
  EXPECT_EQ(frame->texture(), nullptr);
}

TEST(DesktopFrameTest, TextureFrameHasValidTexture) {
  const FrameTexture::Handle handle = MakeHandle();
  auto texture = std::make_unique<FakeFrameTexture>(handle);
  FrameTexture* raw_texture = texture.get();

  auto frame = std::make_unique<TextureDesktopFrame>(DesktopSize(10, 10),
                                                     std::move(texture));
  EXPECT_EQ(frame->data(), nullptr);
  EXPECT_NE(frame->texture(), nullptr);
  EXPECT_EQ(frame->texture(), raw_texture);
  EXPECT_EQ(frame->texture()->handle(), handle);
}

TEST(DesktopFrameTest, TextureFrameHasNullData) {
  // Verify that a texture-backed frame with nullptr data() still works for
  // metadata access (size, texture handle, etc.).
  const FrameTexture::Handle handle = MakeHandle();
  auto texture = std::make_unique<FakeFrameTexture>(handle);

  auto frame = std::make_unique<TextureDesktopFrame>(DesktopSize(10, 10),
                                                     std::move(texture));
  EXPECT_EQ(frame->data(), nullptr);
  EXPECT_NE(frame->texture(), nullptr);
  EXPECT_EQ(frame->texture()->handle(), handle);
  EXPECT_EQ(frame->size().width(), 10);
  EXPECT_EQ(frame->size().height(), 10);

  // Metadata operations should work on texture-only frames.
  frame->set_dpi(DesktopVector(96, 96));
  EXPECT_EQ(frame->dpi().x(), 96);
  frame->set_capture_time_ms(123);
  EXPECT_EQ(frame->capture_time_ms(), 123);
  frame->mutable_updated_region()->SetRect(DesktopRect::MakeWH(10, 10));
  EXPECT_FALSE(frame->updated_region().is_empty());
}

TEST(SharedDesktopFrameTest, TexturePropagatedToSharedFrame) {
  const FrameTexture::Handle handle = MakeHandle();
  auto texture = std::make_unique<FakeFrameTexture>(handle);
  FrameTexture* raw_texture = texture.get();

  auto frame = std::make_unique<TextureDesktopFrame>(DesktopSize(10, 10),
                                                     std::move(texture));
  auto shared_frame = SharedDesktopFrame::Wrap(std::move(frame));
  ASSERT_NE(shared_frame, nullptr);
  EXPECT_EQ(shared_frame->texture(), raw_texture);
  EXPECT_EQ(shared_frame->texture()->handle(), handle);
}

TEST(SharedDesktopFrameTest, TexturePropagatedToClone) {
  const FrameTexture::Handle handle = MakeHandle();
  auto texture = std::make_unique<FakeFrameTexture>(handle);
  FrameTexture* raw_texture = texture.get();

  auto frame = std::make_unique<TextureDesktopFrame>(DesktopSize(10, 10),
                                                     std::move(texture));
  auto shared_frame = SharedDesktopFrame::Wrap(std::move(frame));
  auto clone = shared_frame->Share();
  ASSERT_NE(clone, nullptr);
  EXPECT_EQ(clone->texture(), raw_texture);
  EXPECT_EQ(clone->texture()->handle(), handle);
  EXPECT_TRUE(shared_frame->ShareFrameWith(*clone));
}

TEST(SharedDesktopFrameTest, NullTextureForNormalFrame) {
  auto frame = std::make_unique<BasicDesktopFrame>(DesktopSize(10, 10));
  auto shared_frame = SharedDesktopFrame::Wrap(std::move(frame));
  ASSERT_NE(shared_frame, nullptr);
  EXPECT_EQ(shared_frame->texture(), nullptr);
}

}  // namespace webrtc
