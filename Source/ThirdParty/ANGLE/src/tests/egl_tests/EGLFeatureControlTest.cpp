//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Tests the eglQueryStringiANGLE and eglQueryDisplayAttribANGLE functions exposed by the
// extension EGL_ANGLE_feature_control.

#include <gtest/gtest.h>

#include "libANGLE/Display.h"
#include "test_utils/ANGLETest.h"

using namespace angle;

class EGLFeatureControlTest : public ANGLETest
{
  public:
    void testSetUp() override
    {
        // All tests are skipped on old AMD Linux Vulkan driver. See http://crbug.com/1097750
        if (IsLinux() && IsAMD() && IsVulkan())
        {
            GTEST_SKIP();
        }

        mDisplay = EGL_NO_DISPLAY;
    }

    void testTearDown() override
    {
        if (mDisplay != EGL_NO_DISPLAY)
        {
            eglTerminate(mDisplay);
        }
    }

  protected:
    EGLDisplay mDisplay;

    bool initTest()
    {
        // http://anglebug.com/3629 This test sporadically times out on Win10/Intel
        if (IsWindows() && IsIntel())
            return false;

        EGLAttrib dispattrs[] = {EGL_PLATFORM_ANGLE_TYPE_ANGLE, GetParam().getRenderer(), EGL_NONE};
        mDisplay              = eglGetPlatformDisplay(EGL_PLATFORM_ANGLE_ANGLE,
                                         reinterpret_cast<void *>(EGL_DEFAULT_DISPLAY), dispattrs);
        EXPECT_NE(mDisplay, EGL_NO_DISPLAY);

        EXPECT_EQ(eglInitialize(mDisplay, nullptr, nullptr), static_cast<EGLBoolean>(EGL_TRUE));

        EXPECT_TRUE(IsEGLClientExtensionEnabled("EGL_ANGLE_feature_control"));

        return true;
    }
};

// Ensure eglQueryStringiANGLE generates EGL_BAD_DISPLAY if the display passed in is invalid.
TEST_P(EGLFeatureControlTest, InvalidDisplay)
{
    ANGLE_SKIP_TEST_IF(!initTest());
    EXPECT_EQ(nullptr, eglQueryStringiANGLE(EGL_NO_DISPLAY, EGL_FEATURE_NAME_ANGLE, 0));
    EXPECT_EGL_ERROR(EGL_BAD_DISPLAY);
}

// Ensure eglQueryStringiANGLE generates EGL_BAD_PARAMETER if the index is negative.
TEST_P(EGLFeatureControlTest, NegativeIndex)
{
    ANGLE_SKIP_TEST_IF(!initTest());
    EXPECT_EQ(nullptr, eglQueryStringiANGLE(mDisplay, EGL_FEATURE_NAME_ANGLE, -1));
    EXPECT_EGL_ERROR(EGL_BAD_PARAMETER);
}

// Ensure eglQueryStringiANGLE generates EGL_BAD_PARAMETER if the index is out of bounds.
TEST_P(EGLFeatureControlTest, IndexOutOfBounds)
{
    ANGLE_SKIP_TEST_IF(!initTest());
    egl::Display *display = static_cast<egl::Display *>(mDisplay);
    EXPECT_EQ(nullptr, eglQueryStringiANGLE(mDisplay, EGL_FEATURE_NAME_ANGLE,
                                            display->getFeatures().size()));
    EXPECT_EGL_ERROR(EGL_BAD_PARAMETER);
}

// Ensure eglQueryStringiANGLE generates EGL_BAD_PARAMETER if the name is not one of the valid
// options specified in EGL_ANGLE_feature_control.
TEST_P(EGLFeatureControlTest, InvalidName)
{
    ANGLE_SKIP_TEST_IF(!initTest());
    EXPECT_EQ(nullptr, eglQueryStringiANGLE(mDisplay, 100, 0));
    EXPECT_EGL_ERROR(EGL_BAD_PARAMETER);
}

// For each valid name and index in the feature description arrays, query the values and ensure
// that no error is generated, and that the values match the correct values frim ANGLE's display's
// FeatureList.
TEST_P(EGLFeatureControlTest, QueryAll)
{
    ANGLE_SKIP_TEST_IF(!initTest());
    egl::Display *display       = static_cast<egl::Display *>(mDisplay);
    angle::FeatureList features = display->getFeatures();
    for (size_t i = 0; i < features.size(); i++)
    {
        EXPECT_STREQ(features[i]->name, eglQueryStringiANGLE(mDisplay, EGL_FEATURE_NAME_ANGLE, i));
        EXPECT_STREQ(FeatureCategoryToString(features[i]->category),
                     eglQueryStringiANGLE(mDisplay, EGL_FEATURE_CATEGORY_ANGLE, i));
        EXPECT_STREQ(features[i]->description,
                     eglQueryStringiANGLE(mDisplay, EGL_FEATURE_DESCRIPTION_ANGLE, i));
        EXPECT_STREQ(features[i]->bug, eglQueryStringiANGLE(mDisplay, EGL_FEATURE_BUG_ANGLE, i));
        EXPECT_STREQ(FeatureStatusToString(features[i]->enabled),
                     eglQueryStringiANGLE(mDisplay, EGL_FEATURE_STATUS_ANGLE, i));
        EXPECT_STREQ(features[i]->condition,
                     eglQueryStringiANGLE(mDisplay, EGL_FEATURE_CONDITION_ANGLE, i));
        ASSERT_EGL_SUCCESS();
    }
}

// Ensure eglQueryDisplayAttribANGLE returns the correct number of features when queried with
// attribute EGL_FEATURE_COUNT_ANGLE
TEST_P(EGLFeatureControlTest, FeatureCount)
{
    ANGLE_SKIP_TEST_IF(!initTest());
    egl::Display *display = static_cast<egl::Display *>(mDisplay);
    EGLAttrib value       = -1;
    EXPECT_EQ(static_cast<EGLBoolean>(EGL_TRUE),
              eglQueryDisplayAttribANGLE(mDisplay, EGL_FEATURE_COUNT_ANGLE, &value));
    EXPECT_EQ(display->getFeatures().size(), static_cast<size_t>(value));
    ASSERT_EGL_SUCCESS();
}

// Submit a list of features to override when creating the display with eglGetPlatformDisplay, and
// ensure that the features are correctly overridden.
TEST_P(EGLFeatureControlTest, OverrideFeatures)
{
    ANGLE_SKIP_TEST_IF(!initTest());
    egl::Display *display       = static_cast<egl::Display *>(mDisplay);
    angle::FeatureList features = display->getFeatures();

    // Build lists of features to enable/disabled. Toggle features we know are ok to toggle based
    // from this list.
    std::vector<const char *> enabled       = std::vector<const char *>();
    std::vector<const char *> disabled      = std::vector<const char *>();
    std::vector<bool> shouldBe              = std::vector<bool>();
    std::vector<std::string> testedFeatures = {
        "add_and_true_to_loop_condition",  // Safe to toggle GL
        "clamp_frag_depth",                // Safe to toggle GL
        "clamp_point_size",                // Safe to toggle GL and Vulkan
        "flip_viewport_y",                 // Safe to toggle on Vulkan
        "zero_max_lod",                    // Safe to toggle on D3D
        "expand_integer_pow_expressions",  // Safe to toggle on D3D
        "rewrite_unary_minus_operator",    // Safe to toggle on D3D
    };
    for (size_t i = 0; i < features.size(); i++)
    {
        bool toggle = std::find(testedFeatures.begin(), testedFeatures.end(),
                                std::string(features[i]->name)) != testedFeatures.end();
        if (features[i]->enabled ^ toggle)
        {
            enabled.push_back(features[i]->name);
        }
        else
        {
            disabled.push_back(features[i]->name);
        }
        // Save what we expect the feature status will be when checking later.
        shouldBe.push_back(features[i]->enabled ^ toggle);
    }
    disabled.push_back(0);
    enabled.push_back(0);

    // Terminate the old display (we just used it to collect features)
    eglTerminate(mDisplay);

    // Create a new display with these overridden features.
    EGLAttrib dispattrs[]   = {EGL_PLATFORM_ANGLE_TYPE_ANGLE,
                             GetParam().getRenderer(),
                             EGL_FEATURE_OVERRIDES_ENABLED_ANGLE,
                             reinterpret_cast<EGLAttrib>(enabled.data()),
                             EGL_FEATURE_OVERRIDES_DISABLED_ANGLE,
                             reinterpret_cast<EGLAttrib>(disabled.data()),
                             EGL_NONE};
    EGLDisplay dpy_override = eglGetPlatformDisplay(
        EGL_PLATFORM_ANGLE_ANGLE, reinterpret_cast<void *>(EGL_DEFAULT_DISPLAY), dispattrs);
    ASSERT_EGL_SUCCESS();
    ASSERT_TRUE(dpy_override != EGL_NO_DISPLAY);
    ASSERT_TRUE(eglInitialize(dpy_override, nullptr, nullptr) == EGL_TRUE);

    // Check that all features have the correct status (even the ones we toggled).
    for (size_t i = 0; i < features.size(); i++)
    {
        EXPECT_STREQ(FeatureStatusToString(shouldBe[i]),
                     eglQueryStringiANGLE(dpy_override, EGL_FEATURE_STATUS_ANGLE, i));
    }
}

ANGLE_INSTANTIATE_TEST(EGLFeatureControlTest,
                       WithNoFixture(ES2_D3D9()),
                       WithNoFixture(ES2_D3D11()),
                       WithNoFixture(ES2_OPENGL()),
                       WithNoFixture(ES2_VULKAN()),
                       WithNoFixture(ES3_D3D11()),
                       WithNoFixture(ES3_OPENGL()));
