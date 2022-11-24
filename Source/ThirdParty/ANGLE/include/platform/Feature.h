//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Feature.h: Definition of structs to hold feature/workaround information.
//

#ifndef ANGLE_PLATFORM_FEATURE_H_
#define ANGLE_PLATFORM_FEATURE_H_

#include <map>
#include <string>
#include <vector>

#define ANGLE_FEATURE_CONDITION(set, feature, cond)       \
    do                                                    \
    {                                                     \
        (set)->feature.enabled   = cond;                  \
        (set)->feature.condition = ANGLE_STRINGIFY(cond); \
    } while (0)

namespace angle
{

enum class FeatureCategory
{
    FrontendFeatures,
    FrontendWorkarounds,
    OpenGLWorkarounds,
    OpenGLFeatures,
    D3DWorkarounds,
    VulkanFeatures,
    VulkanWorkarounds,
    VulkanAppWorkarounds,
    MetalFeatures,
    MetalWorkarounds,
};

constexpr char kFeatureCategoryFrontendWorkarounds[]  = "Frontend workarounds";
constexpr char kFeatureCategoryFrontendFeatures[]     = "Frontend features";
constexpr char kFeatureCategoryOpenGLWorkarounds[]    = "OpenGL workarounds";
constexpr char kFeatureCategoryOpenGLFeatures[]       = "OpenGL features";
constexpr char kFeatureCategoryD3DWorkarounds[]       = "D3D workarounds";
constexpr char kFeatureCategoryVulkanAppWorkarounds[] = "Vulkan app workarounds";
constexpr char kFeatureCategoryVulkanWorkarounds[]    = "Vulkan workarounds";
constexpr char kFeatureCategoryVulkanFeatures[]       = "Vulkan features";
constexpr char kFeatureCategoryMetalFeatures[]        = "Metal features";
constexpr char kFeatureCategoryMetalWorkarounds[]     = "Metal workarounds";
constexpr char kFeatureCategoryUnknown[]              = "Unknown";

inline const char *FeatureCategoryToString(const FeatureCategory &fc)
{
    switch (fc)
    {
        case FeatureCategory::FrontendFeatures:
            return kFeatureCategoryFrontendFeatures;
            break;

        case FeatureCategory::FrontendWorkarounds:
            return kFeatureCategoryFrontendWorkarounds;
            break;

        case FeatureCategory::OpenGLWorkarounds:
            return kFeatureCategoryOpenGLWorkarounds;
            break;

        case FeatureCategory::OpenGLFeatures:
            return kFeatureCategoryOpenGLFeatures;
            break;

        case FeatureCategory::D3DWorkarounds:
            return kFeatureCategoryD3DWorkarounds;
            break;

        case FeatureCategory::VulkanFeatures:
            return kFeatureCategoryVulkanFeatures;
            break;

        case FeatureCategory::VulkanWorkarounds:
            return kFeatureCategoryVulkanWorkarounds;
            break;

        case FeatureCategory::VulkanAppWorkarounds:
            return kFeatureCategoryVulkanAppWorkarounds;
            break;

        case FeatureCategory::MetalFeatures:
            return kFeatureCategoryMetalFeatures;
            break;

        case FeatureCategory::MetalWorkarounds:
            return kFeatureCategoryMetalWorkarounds;
            break;

        default:
            return kFeatureCategoryUnknown;
            break;
    }
}

constexpr char kFeatureStatusEnabled[]  = "enabled";
constexpr char kFeatureStatusDisabled[] = "disabled";

inline const char *FeatureStatusToString(const bool &status)
{
    if (status)
    {
        return kFeatureStatusEnabled;
    }
    return kFeatureStatusDisabled;
}

struct FeatureInfo;

using FeatureMap  = std::map<std::string, FeatureInfo *>;
using FeatureList = std::vector<const FeatureInfo *>;

struct FeatureInfo
{
    FeatureInfo(const FeatureInfo &other);
    FeatureInfo(const char *name,
                const FeatureCategory &category,
                const char *description,
                FeatureMap *const mapPtr,
                const char *bug);
    ~FeatureInfo();

    // The name of the workaround, lowercase, camel_case
    const char *const name;

    // The category that the workaround belongs to. Eg. "Vulkan workarounds"
    const FeatureCategory category;

    // A short description to be read by the user.
    const char *const description;

    // A link to the bug, if any
    const char *const bug;

    // Whether the workaround is enabled or not. Determined by heuristics like vendor ID and
    // version, but may be overriden to any value.
    bool enabled = false;

    // A stringified version of the condition used to set 'enabled'. ie "IsNvidia() && IsApple()"
    const char *condition;
};

inline FeatureInfo::FeatureInfo(const FeatureInfo &other) = default;
inline FeatureInfo::FeatureInfo(const char *name,
                                const FeatureCategory &category,
                                const char *description,
                                FeatureMap *const mapPtr,
                                const char *bug = "")
    : name(name),
      category(category),
      description(description),
      bug(bug),
      enabled(false),
      condition("")
{
    if (mapPtr != nullptr)
    {
        (*mapPtr)[std::string(name)] = this;
    }
}

inline FeatureInfo::~FeatureInfo() = default;

struct FeatureSetBase
{
  public:
    FeatureSetBase();
    ~FeatureSetBase();

  private:
    // Non-copyable
    FeatureSetBase(const FeatureSetBase &other)            = delete;
    FeatureSetBase &operator=(const FeatureSetBase &other) = delete;

  protected:
    FeatureMap members = FeatureMap();

  public:
    void overrideFeatures(const std::vector<std::string> &featureNames, bool enabled);
    void populateFeatureList(FeatureList *features) const;

    const FeatureMap &getFeatures() const { return members; }
};

inline FeatureSetBase::FeatureSetBase()  = default;
inline FeatureSetBase::~FeatureSetBase() = default;

}  // namespace angle

#endif  // ANGLE_PLATFORM_WORKAROUND_H_
