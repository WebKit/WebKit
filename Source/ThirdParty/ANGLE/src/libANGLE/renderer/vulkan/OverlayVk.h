//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// OverlayVk.h:
//    Defines the OverlayVk class that does the actual rendering of the overlay.
//

#ifndef LIBANGLE_RENDERER_VULKAN_OVERLAYVK_H_
#define LIBANGLE_RENDERER_VULKAN_OVERLAYVK_H_

#include "common/angleutils.h"
#include "libANGLE/renderer/OverlayImpl.h"
#include "libANGLE/renderer/vulkan/vk_helpers.h"

namespace rx
{
class ContextVk;

class OverlayVk : public OverlayImpl
{
  public:
    OverlayVk(const gl::OverlayState &state);
    ~OverlayVk() override;

    angle::Result init(const gl::Context *context) override;
    void onDestroy(const gl::Context *context) override;

    angle::Result onPresent(ContextVk *contextVk,
                            vk::ImageHelper *imageToPresent,
                            const vk::ImageView *imageToPresentView);

  private:
    angle::Result createFont(ContextVk *contextVk);
    angle::Result cullWidgets(ContextVk *contextVk);

    bool mSupportsSubgroupBallot;
    bool mSupportsSubgroupArithmetic;
    bool mRefreshCulledWidgets;

    // Cached size of subgroup as accepted by UtilsVk, deduced from hardware subgroup size.
    uint32_t mSubgroupSize[2];

    // Cached size of last presented image.  If the size changes, culling is repeated.
    VkExtent2D mPresentImageExtent;

    vk::ImageHelper mFontImage;
    vk::ImageView mFontImageView;

    vk::ImageHelper mCulledWidgets;
    vk::ImageView mCulledWidgetsView;
};

}  // namespace rx

#endif  // LIBANGLE_RENDERER_VULKAN_OVERLAYVK_H_
