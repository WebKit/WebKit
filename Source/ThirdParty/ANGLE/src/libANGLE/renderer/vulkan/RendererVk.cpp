//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// RendererVk.cpp:
//    Implements the class methods for RendererVk.
//

#include "libANGLE/renderer/vulkan/RendererVk.h"

// Placing this first seems to solve an intellisense bug.
#include "libANGLE/renderer/vulkan/vk_utils.h"

#include <EGL/eglext.h>

#include "common/debug.h"
#include "common/platform.h"
#include "common/system_utils.h"
#include "common/vulkan/libvulkan_loader.h"
#include "common/vulkan/vk_google_filtering_precision.h"
#include "common/vulkan/vulkan_icd.h"
#include "gpu_info_util/SystemInfo.h"
#include "libANGLE/Context.h"
#include "libANGLE/Display.h"
#include "libANGLE/renderer/driver_utils.h"
#include "libANGLE/renderer/vulkan/CompilerVk.h"
#include "libANGLE/renderer/vulkan/ContextVk.h"
#include "libANGLE/renderer/vulkan/DisplayVk.h"
#include "libANGLE/renderer/vulkan/FramebufferVk.h"
#include "libANGLE/renderer/vulkan/ProgramVk.h"
#include "libANGLE/renderer/vulkan/ResourceVk.h"
#include "libANGLE/renderer/vulkan/VertexArrayVk.h"
#include "libANGLE/renderer/vulkan/vk_caps_utils.h"
#include "libANGLE/renderer/vulkan/vk_format_utils.h"
#include "libANGLE/trace.h"
#include "platform/PlatformMethods.h"

// Consts
namespace
{
constexpr VkFormatFeatureFlags kInvalidFormatFeatureFlags = static_cast<VkFormatFeatureFlags>(-1);

#if defined(ANGLE_EXPOSE_NON_CONFORMANT_EXTENSIONS_AND_VERSIONS)
constexpr bool kExposeNonConformantExtensionsAndVersions = true;
#else
constexpr bool kExposeNonConformantExtensionsAndVersions = false;
#endif

// This flag is used for memory allocation tracking using allocation size counters.
constexpr bool kTrackMemoryAllocationSizes = true;
#if defined(ANGLE_ENABLE_MEMORY_ALLOC_LOGGING)
// Flag used for logging memory allocations and deallocations.
constexpr bool kTrackMemoryAllocationDebug = true;
static_assert(kTrackMemoryAllocationSizes,
              "kTrackMemoryAllocationSizes must be enabled to use kTrackMemoryAllocationDebug.");
#else
// Only the allocation size counters are used (if enabled).
constexpr bool kTrackMemoryAllocationDebug = false;
#endif
}  // anonymous namespace

namespace rx
{

namespace
{
constexpr uint32_t kMinDefaultUniformBufferSize = 16 * 1024u;
// This size is picked based on experience. Majority of devices support 64K
// maxUniformBufferSize. Since this is per context buffer, a bigger buffer size reduces the
// number of descriptor set allocations, so we picked the maxUniformBufferSize that most
// devices supports. It may needs further tuning based on specific device needs and balance
// between performance and memory usage.
constexpr uint32_t kPreferredDefaultUniformBufferSize = 64 * 1024u;

// Update the pipeline cache every this many swaps.
constexpr uint32_t kPipelineCacheVkUpdatePeriod = 60;
// Per the Vulkan specification, as long as Vulkan 1.1+ is returned by vkEnumerateInstanceVersion,
// ANGLE must indicate the highest version of Vulkan functionality that it uses.  The Vulkan
// validation layers will issue messages for any core functionality that requires a higher version.
// This value must be increased whenever ANGLE starts using functionality from a newer core
// version of Vulkan.
constexpr uint32_t kPreferredVulkanAPIVersion = VK_API_VERSION_1_1;

bool IsQualcommOpenSource(uint32_t vendorId, uint32_t driverId, const char *deviceName)
{
    if (!IsQualcomm(vendorId))
    {
        return false;
    }

    // Where driver id is available, distinguish by driver id:
    if (driverId != 0)
    {
        return driverId != VK_DRIVER_ID_QUALCOMM_PROPRIETARY;
    }

    // Otherwise, look for Venus or Turnip in the device name.
    return strstr(deviceName, "Venus") != nullptr || strstr(deviceName, "Turnip") != nullptr;
}

angle::vk::ICD ChooseICDFromAttribs(const egl::AttributeMap &attribs)
{
#if !defined(ANGLE_PLATFORM_ANDROID)
    // Mock ICD does not currently run on Android
    EGLAttrib deviceType = attribs.get(EGL_PLATFORM_ANGLE_DEVICE_TYPE_ANGLE,
                                       EGL_PLATFORM_ANGLE_DEVICE_TYPE_HARDWARE_ANGLE);

    switch (deviceType)
    {
        case EGL_PLATFORM_ANGLE_DEVICE_TYPE_HARDWARE_ANGLE:
            break;
        case EGL_PLATFORM_ANGLE_DEVICE_TYPE_NULL_ANGLE:
            return angle::vk::ICD::Mock;
        case EGL_PLATFORM_ANGLE_DEVICE_TYPE_SWIFTSHADER_ANGLE:
            return angle::vk::ICD::SwiftShader;
        default:
            UNREACHABLE();
            break;
    }
#endif  // !defined(ANGLE_PLATFORM_ANDROID)

    return angle::vk::ICD::Default;
}

bool StrLess(const char *a, const char *b)
{
    return strcmp(a, b) < 0;
}

bool ExtensionFound(const char *needle, const vk::ExtensionNameList &haystack)
{
    // NOTE: The list must be sorted.
    return std::binary_search(haystack.begin(), haystack.end(), needle, StrLess);
}

VkResult VerifyExtensionsPresent(const vk::ExtensionNameList &haystack,
                                 const vk::ExtensionNameList &needles)
{
    // NOTE: The lists must be sorted.
    if (std::includes(haystack.begin(), haystack.end(), needles.begin(), needles.end(), StrLess))
    {
        return VK_SUCCESS;
    }
    for (const char *needle : needles)
    {
        if (!ExtensionFound(needle, haystack))
        {
            ERR() << "Extension not supported: " << needle;
        }
    }
    return VK_ERROR_EXTENSION_NOT_PRESENT;
}

// Array of Validation error/warning messages that will be ignored, should include bugID
constexpr const char *kSkippedMessages[] = {
    // http://anglebug.com/2866
    "UNASSIGNED-CoreValidation-Shader-OutputNotConsumed",
    // http://anglebug.com/4883
    "UNASSIGNED-CoreValidation-Shader-InputNotProduced",
    // http://anglebug.com/4928
    "VUID-vkMapMemory-memory-00683",
    // http://anglebug.com/5027
    "UNASSIGNED-CoreValidation-Shader-PushConstantOutOfRange",
    // http://anglebug.com/5304
    "VUID-vkCmdDraw-magFilter-04553",
    "VUID-vkCmdDrawIndexed-magFilter-04553",
    // http://anglebug.com/5309
    "VUID-VkImageViewCreateInfo-usage-02652",
    // http://issuetracker.google.com/175584609
    "VUID-vkCmdDraw-None-04584",
    "VUID-vkCmdDrawIndexed-None-04584",
    "VUID-vkCmdDrawIndirect-None-04584",
    "VUID-vkCmdDrawIndirectCount-None-04584",
    "VUID-vkCmdDrawIndexedIndirect-None-04584",
    "VUID-vkCmdDrawIndexedIndirectCount-None-04584",
    // http://anglebug.com/5912
    "VUID-VkImageViewCreateInfo-pNext-01585",
    // http://anglebug.com/6442
    "UNASSIGNED-CoreValidation-Shader-InterfaceTypeMismatch",
    // http://anglebug.com/6514
    "vkEnumeratePhysicalDevices: One or more layers modified physical devices",
    // When using Vulkan secondary command buffers, the command buffer is begun with the current
    // framebuffer specified in pInheritanceInfo::framebuffer.  If the framebuffer is multisampled
    // and is resolved, an optimization would change the framebuffer to add the resolve target and
    // use a subpass resolve operation instead.  The following error complains that the framebuffer
    // used to start the render pass and the one specified in pInheritanceInfo::framebuffer must be
    // equal, which is not true in that case.  In practice, this is benign, as the part of the
    // framebuffer that's accessed by the command buffer is identically laid out.
    // http://anglebug.com/6811
    "VUID-vkCmdExecuteCommands-pCommandBuffers-00099",
    // http://anglebug.com/7105
    "VUID-vkCmdDraw-None-06538",
    "VUID-vkCmdDrawIndexed-None-06538",
    // http://anglebug.com/7325
    "VUID-vkCmdBindVertexBuffers2-pStrides-06209",
    // http://anglebug.com/7729
    "VUID-vkDestroySemaphore-semaphore-01137",
    // http://anglebug.com/7843
    "VUID-VkGraphicsPipelineCreateInfo-Vertex-07722",
    // http://anglebug.com/7861
    "VUID-vkCmdDraw-None-06887",
    "VUID-vkCmdDrawIndexed-None-06887",
    // http://anglebug.com/7865
    "VUID-VkDescriptorImageInfo-imageView-06711",
};

// Validation messages that should be ignored only when VK_EXT_primitive_topology_list_restart is
// not present.
constexpr const char *kNoListRestartSkippedMessages[] = {
    // http://anglebug.com/3832
    "VUID-VkPipelineInputAssemblyStateCreateInfo-topology-00428",
};

// Some syncval errors are resolved in the presence of the NONE load or store render pass ops.  For
// those, ANGLE makes no further attempt to resolve them and expects vendor support for the
// extensions instead.  The list of skipped messages is split based on this support.
constexpr vk::SkippedSyncvalMessage kSkippedSyncvalMessages[] = {
    // http://anglebug.com/6416
    // http://anglebug.com/6421
    {
        "SYNC-HAZARD-WRITE-AFTER-WRITE",
        "Access info (usage: SYNC_IMAGE_LAYOUT_TRANSITION, prior_usage: "
        "SYNC_IMAGE_LAYOUT_TRANSITION, "
        "write_barriers: 0, command: vkCmdEndRenderPass",
    },
    // These errors are caused by a feedback loop tests that don't produce correct Vulkan to begin
    // with.
    // http://anglebug.com/6417
    // http://anglebug.com/7070
    //
    // Occassionally, this is due to VVL's lack of support for some extensions.  For example,
    // syncval doesn't properly account for VK_EXT_fragment_shader_interlock, which gives
    // synchronization guarantees without the need for an image barrier.
    // https://github.com/KhronosGroup/Vulkan-ValidationLayers/issues/4387
    {
        "SYNC-HAZARD-READ-AFTER-WRITE",
        "imageLayout: VK_IMAGE_LAYOUT_GENERAL",
        "usage: SYNC_FRAGMENT_SHADER_SHADER_",
    },
    // http://anglebug.com/6551
    {
        "SYNC-HAZARD-WRITE-AFTER-WRITE",
        "Access info (usage: SYNC_IMAGE_LAYOUT_TRANSITION, prior_usage: "
        "SYNC_COLOR_ATTACHMENT_OUTPUT_COLOR_ATTACHMENT_WRITE, write_barriers: "
        "SYNC_EARLY_FRAGMENT_TESTS_DEPTH_STENCIL_ATTACHMENT_READ|SYNC_EARLY_FRAGMENT_TESTS_DEPTH_"
        "STENCIL_ATTACHMENT_WRITE|SYNC_LATE_FRAGMENT_TESTS_DEPTH_STENCIL_ATTACHMENT_READ|SYNC_LATE_"
        "FRAGMENT_TESTS_DEPTH_STENCIL_ATTACHMENT_WRITE|SYNC_COLOR_ATTACHMENT_OUTPUT_COLOR_"
        "ATTACHMENT_"
        "READ|SYNC_COLOR_ATTACHMENT_OUTPUT_COLOR_ATTACHMENT_WRITE, command: vkCmdEndRenderPass",
    },
    {
        "SYNC-HAZARD-WRITE-AFTER-WRITE",
        "Access info (usage: SYNC_IMAGE_LAYOUT_TRANSITION, prior_usage: "
        "SYNC_COLOR_ATTACHMENT_OUTPUT_COLOR_ATTACHMENT_WRITE, write_barriers: "
        "SYNC_COLOR_ATTACHMENT_OUTPUT_COLOR_ATTACHMENT_READ|SYNC_COLOR_ATTACHMENT_OUTPUT_COLOR_"
        "ATTACHMENT_WRITE, command: vkCmdEndRenderPass",
    },
    // From: TraceTest.manhattan_31 with SwiftShader and
    // VulkanPerformanceCounterTest.NewTextureDoesNotBreakRenderPass for both depth and stencil
    // aspect. http://anglebug.com/6701
    {
        "SYNC-HAZARD-WRITE-AFTER-WRITE",
        "Hazard WRITE_AFTER_WRITE in subpass 0 for attachment 1 aspect ",
        "during load with loadOp VK_ATTACHMENT_LOAD_OP_DONT_CARE. Access info (usage: "
        "SYNC_EARLY_FRAGMENT_TESTS_DEPTH_STENCIL_ATTACHMENT_WRITE, prior_usage: "
        "SYNC_IMAGE_LAYOUT_TRANSITION",
    },
    // From various tests. The validation layer does not calculate the exact vertexCounts that's
    // being accessed. http://anglebug.com/6725
    {
        "SYNC-HAZARD-READ-AFTER-WRITE",
        "vkCmdDrawIndexed: Hazard READ_AFTER_WRITE for vertex",
        "usage: SYNC_VERTEX_ATTRIBUTE_INPUT_VERTEX_ATTRIBUTE_READ",
    },
    {
        "SYNC-HAZARD-READ-AFTER-WRITE",
        "vkCmdDrawIndexedIndirect: Hazard READ_AFTER_WRITE for vertex",
        "usage: SYNC_VERTEX_ATTRIBUTE_INPUT_VERTEX_ATTRIBUTE_READ",
    },
    {
        "SYNC-HAZARD-READ-AFTER-WRITE",
        "vkCmdDrawIndirect: Hazard READ_AFTER_WRITE for vertex",
        "usage: SYNC_VERTEX_ATTRIBUTE_INPUT_VERTEX_ATTRIBUTE_READ",
    },
    {
        "SYNC-HAZARD-READ-AFTER-WRITE",
        "vkCmdDrawIndexedIndirect: Hazard READ_AFTER_WRITE for index",
        "usage: SYNC_INDEX_INPUT_INDEX_READ",
    },
    {
        "SYNC-HAZARD-WRITE-AFTER-READ",
        "vkCmdDraw: Hazard WRITE_AFTER_READ for",
        "Access info (usage: SYNC_VERTEX_SHADER_SHADER_STORAGE_WRITE, prior_usage: "
        "SYNC_VERTEX_ATTRIBUTE_INPUT_VERTEX_ATTRIBUTE_READ",
    },
    {
        "SYNC-HAZARD-WRITE-AFTER-READ",
        "vkCmdCopyImageToBuffer: Hazard WRITE_AFTER_READ for dstBuffer VkBuffer",
        "Access info (usage: SYNC_COPY_TRANSFER_WRITE, prior_usage: "
        "SYNC_VERTEX_ATTRIBUTE_INPUT_VERTEX_ATTRIBUTE_READ",
    },
    {
        "SYNC-HAZARD-WRITE-AFTER-READ",
        "vkCmdCopyBuffer: Hazard WRITE_AFTER_READ for dstBuffer VkBuffer",
        "Access info (usage: SYNC_COPY_TRANSFER_WRITE, prior_usage: "
        "SYNC_VERTEX_ATTRIBUTE_INPUT_VERTEX_ATTRIBUTE_READ",
    },
    {
        "SYNC-HAZARD-WRITE-AFTER-READ",
        "vkCmdDispatch: Hazard WRITE_AFTER_READ for VkBuffer",
        "Access info (usage: SYNC_COMPUTE_SHADER_SHADER_STORAGE_WRITE, prior_usage: "
        "SYNC_VERTEX_ATTRIBUTE_INPUT_VERTEX_ATTRIBUTE_READ",
    },
    // From: MultisampledRenderToTextureES3Test.TransformFeedbackTest. http://anglebug.com/6725
    {
        "SYNC-HAZARD-WRITE-AFTER-WRITE",
        "vkCmdBeginRenderPass: Hazard WRITE_AFTER_WRITE in subpass",
        "write_barriers: "
        "SYNC_TRANSFORM_FEEDBACK_EXT_TRANSFORM_FEEDBACK_COUNTER_READ_EXT|SYNC_TRANSFORM_FEEDBACK_"
        "EXT_"
        "TRANSFORM_FEEDBACK_COUNTER_WRITE_EXT",
    },
    // From: TraceTest.manhattan_31 with SwiftShader. These failures appears related to
    // dynamic uniform buffers. The failures are gone if I force mUniformBufferDescriptorType to
    // VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER. My guess is that syncval is not doing a fine grain enough
    // range tracking with dynamic uniform buffers. http://anglebug.com/6725
    {
        "SYNC-HAZARD-WRITE-AFTER-READ",
        "usage: SYNC_VERTEX_SHADER_UNIFORM_READ",
    },
    {
        "SYNC-HAZARD-READ-AFTER-WRITE",
        "usage: SYNC_VERTEX_SHADER_UNIFORM_READ",
    },
    {
        "SYNC-HAZARD-WRITE-AFTER-READ",
        "type: VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC",
    },
    {
        "SYNC-HAZARD-READ-AFTER-WRITE",
        "type: VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC",
    },
    // Coherent framebuffer fetch is enabled on some platforms that are known a priori to have the
    // needed behavior, even though this is not specified in the Vulkan spec.  These generate
    // syncval errors that are benign on those platforms.
    // http://anglebug.com/6870
    // From: TraceTest.dead_by_daylight
    // From: TraceTest.genshin_impact
    {"SYNC-HAZARD-READ-AFTER-WRITE",
     "vkCmdBeginRenderPass: Hazard READ_AFTER_WRITE in subpass 0 for attachment ",
     "aspect color during load with loadOp VK_ATTACHMENT_LOAD_OP_LOAD. Access info (usage: "
     "SYNC_COLOR_ATTACHMENT_OUTPUT_COLOR_ATTACHMENT_READ, prior_usage: "
     "SYNC_IMAGE_LAYOUT_TRANSITION, write_barriers: 0, command: vkCmdEndRenderPass",
     true},
    {"SYNC-HAZARD-WRITE-AFTER-WRITE",
     "vkCmdBeginRenderPass: Hazard WRITE_AFTER_WRITE in subpass 0 for attachment ",
     "image layout transition (old_layout: VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, new_layout: "
     "VK_IMAGE_LAYOUT_GENERAL). Access info (usage: SYNC_IMAGE_LAYOUT_TRANSITION, prior_usage: "
     "SYNC_COLOR_ATTACHMENT_OUTPUT_COLOR_ATTACHMENT_WRITE, write_barriers:",
     true},
    // From: TraceTest.car_chase http://anglebug.com/7125
    {
        "SYNC-HAZARD-WRITE-AFTER-READ",
        "type: VK_DESCRIPTOR_TYPE_STORAGE_BUFFER",
    },
    // From: TraceTest.car_chase http://anglebug.com/7125#c6
    {
        "SYNC-HAZARD-WRITE-AFTER-READ",
        "Access info (usage: SYNC_COPY_TRANSFER_WRITE, "
        "prior_usage: SYNC_FRAGMENT_SHADER_UNIFORM_READ, "
        "read_barriers: VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT, command: vkCmdDrawIndexed",
    },
    // From: TraceTest.special_forces_group_2 http://anglebug.com/5592
    {
        "SYNC-HAZARD-WRITE-AFTER-READ",
        "Access info (usage: SYNC_IMAGE_LAYOUT_TRANSITION, prior_usage: "
        "SYNC_FRAGMENT_SHADER_SHADER_",
    },
    // http://anglebug.com/7031
    {"SYNC-HAZARD-READ-AFTER-WRITE",
     "type: VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, imageLayout: "
     "VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, binding #0, index 0. Access info (usage: "
     "SYNC_COMPUTE_SHADER_SHADER_",
     "", false},
    // http://anglebug.com/7456
    {
        "SYNC-HAZARD-READ-AFTER-WRITE",
        "type: VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, "
        "imageLayout: VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL",
        "Access info (usage: SYNC_FRAGMENT_SHADER_SHADER_",
    },
    // From: TraceTest.life_is_strange http://anglebug.com/7711
    {"SYNC-HAZARD-WRITE-AFTER-READ",
     "vkCmdEndRenderPass: Hazard WRITE_AFTER_READ in subpass 0 for attachment 1 "
     "depth aspect during store with storeOp VK_ATTACHMENT_STORE_OP_DONT_CARE. "
     "Access info (usage: SYNC_LATE_FRAGMENT_TESTS_DEPTH_STENCIL_ATTACHMENT_WRITE, "
     "prior_usage: SYNC_FRAGMENT_SHADER_SHADER_"},
    // From: TraceTest.life_is_strange http://anglebug.com/7711
    {"SYNC-HAZARD-READ-AFTER-WRITE",
     "type: VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, "
     "imageLayout: VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL",
     "usage: SYNC_FRAGMENT_SHADER_SHADER_"},
    // From: TraceTest.diablo_immortal http://anglebug.com/7837
    {"SYNC-HAZARD-WRITE-AFTER-WRITE", "vkCmdDrawIndexed: Hazard WRITE_AFTER_WRITE for VkImageView ",
     "Subpass #0, and pColorAttachments #0. Access info (usage: "
     "SYNC_COLOR_ATTACHMENT_OUTPUT_COLOR_ATTACHMENT_WRITE, prior_usage: "
     "SYNC_IMAGE_LAYOUT_TRANSITION, write_barriers: 0, command: vkCmdEndRenderPass"},
    // From: TraceTest.diablo_immortal http://anglebug.com/7837
    {"SYNC-HAZARD-WRITE-AFTER-READ",
     "load with loadOp VK_ATTACHMENT_LOAD_OP_DONT_CARE. Access info (usage: "
     "SYNC_EARLY_FRAGMENT_TESTS_DEPTH_STENCIL_ATTACHMENT_WRITE, prior_usage: "
     "SYNC_FRAGMENT_SHADER_SHADER_"},
    // From: TraceTest.catalyst_black http://anglebug.com/7924
    {"SYNC-HAZARD-WRITE-AFTER-READ",
     "store with storeOp VK_ATTACHMENT_STORE_OP_STORE. Access info (usage: "
     "SYNC_LATE_FRAGMENT_TESTS_DEPTH_STENCIL_ATTACHMENT_WRITE, prior_usage: "
     "SYNC_FRAGMENT_SHADER_SHADER_"},
};

// Messages that shouldn't be generated if storeOp=NONE is supported, otherwise they are expected.
constexpr vk::SkippedSyncvalMessage kSkippedSyncvalMessagesWithoutStoreOpNone[] = {
    // These errors are generated when simultaneously using a read-only depth/stencil attachment as
    // sampler.  This is valid Vulkan.
    //
    // When storeOp=NONE is not present, ANGLE uses storeOp=STORE, but considers the image read-only
    // and produces a hazard.  ANGLE relies on storeOp=NONE and so this is not expected to be worked
    // around.
    //
    // With storeOp=NONE, there is another bug where a depth/stencil attachment may use storeOp=NONE
    // for depth while storeOp=DONT_CARE for stencil, and the latter causes a synchronization error
    // (similarly to the previous case as DONT_CARE is also a write operation).
    // http://anglebug.com/5962
    {
        "SYNC-HAZARD-WRITE-AFTER-READ",
        "depth aspect during store with storeOp VK_ATTACHMENT_STORE_OP_STORE. Access info (usage: "
        "SYNC_LATE_FRAGMENT_TESTS_DEPTH_STENCIL_ATTACHMENT_WRITE",
        "usage: SYNC_FRAGMENT_SHADER_SHADER_",
    },
    {
        "SYNC-HAZARD-WRITE-AFTER-READ",
        "stencil aspect during store with stencilStoreOp VK_ATTACHMENT_STORE_OP_STORE. Access info "
        "(usage: SYNC_LATE_FRAGMENT_TESTS_DEPTH_STENCIL_ATTACHMENT_WRITE",
        "usage: SYNC_FRAGMENT_SHADER_SHADER_",
    },
    {
        "SYNC-HAZARD-READ-AFTER-WRITE",
        "imageLayout: VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL",
        "usage: SYNC_FRAGMENT_SHADER_SHADER_",
    },
    // From: TraceTest.antutu_refinery http://anglebug.com/6663
    {
        "SYNC-HAZARD-READ-AFTER-WRITE",
        "imageLayout: VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL",
        "usage: SYNC_COMPUTE_SHADER_SHADER_SAMPLED_READ",
    },
};

// Messages that shouldn't be generated if both loadOp=NONE and storeOp=NONE are supported,
// otherwise they are expected.
constexpr vk::SkippedSyncvalMessage kSkippedSyncvalMessagesWithoutLoadStoreOpNone[] = {
    // This error is generated for multiple reasons:
    //
    // - http://anglebug.com/6411
    // - http://anglebug.com/5371: This is resolved with storeOp=NONE
    {
        "SYNC-HAZARD-WRITE-AFTER-WRITE",
        "Access info (usage: SYNC_IMAGE_LAYOUT_TRANSITION, prior_usage: "
        "SYNC_LATE_FRAGMENT_TESTS_DEPTH_STENCIL_ATTACHMENT_WRITE, write_barriers: 0, command: "
        "vkCmdEndRenderPass",
    },
    // http://anglebug.com/6411
    // http://anglebug.com/6584
    {
        "SYNC-HAZARD-WRITE-AFTER-WRITE",
        "aspect depth during load with loadOp VK_ATTACHMENT_LOAD_OP_DONT_CARE. Access info (usage: "
        "SYNC_EARLY_FRAGMENT_TESTS_DEPTH_STENCIL_ATTACHMENT_WRITE, prior_usage: "
        "SYNC_IMAGE_LAYOUT_TRANSITION",
    },
    {
        "SYNC-HAZARD-WRITE-AFTER-WRITE",
        "aspect stencil during load with loadOp VK_ATTACHMENT_LOAD_OP_DONT_CARE. Access info "
        "(usage: "
        "SYNC_EARLY_FRAGMENT_TESTS_DEPTH_STENCIL_ATTACHMENT_WRITE",
    },
    // http://anglebug.com/5962
    {
        "SYNC-HAZARD-WRITE-AFTER-WRITE",
        "aspect stencil during load with loadOp VK_ATTACHMENT_LOAD_OP_DONT_CARE. Access info "
        "(usage: "
        "SYNC_EARLY_FRAGMENT_TESTS_DEPTH_STENCIL_ATTACHMENT_WRITE, prior_usage: "
        "SYNC_IMAGE_LAYOUT_TRANSITION",
    },
    {
        "SYNC-HAZARD-WRITE-AFTER-WRITE",
        "aspect stencil during load with loadOp VK_ATTACHMENT_LOAD_OP_DONT_CARE. Access info "
        "(usage: "
        "SYNC_EARLY_FRAGMENT_TESTS_DEPTH_STENCIL_ATTACHMENT_WRITE, prior_usage: "
        "SYNC_IMAGE_LAYOUT_TRANSITION",
    },
};

enum class DebugMessageReport
{
    Ignore,
    Print,
};

bool IsMessageInSkipList(const char *message,
                         const char *const skippedList[],
                         size_t skippedListSize)
{
    for (size_t index = 0; index < skippedListSize; ++index)
    {
        if (strstr(message, skippedList[index]) != nullptr)
        {
            return true;
        }
    }

    return false;
}

// Suppress validation errors that are known.  Returns DebugMessageReport::Ignore in that case.
DebugMessageReport ShouldReportDebugMessage(RendererVk *renderer,
                                            const char *messageId,
                                            const char *message)
{
    if (message == nullptr)
    {
        return DebugMessageReport::Print;
    }

    // Check with non-syncval messages:
    const std::vector<const char *> &skippedMessages = renderer->getSkippedValidationMessages();
    if (IsMessageInSkipList(message, skippedMessages.data(), skippedMessages.size()))
    {
        return DebugMessageReport::Ignore;
    }

    // Then check with syncval messages:
    const bool isFramebufferFetchUsed = renderer->isFramebufferFetchUsed();

    for (const vk::SkippedSyncvalMessage &msg : renderer->getSkippedSyncvalMessages())
    {
        if (strstr(messageId, msg.messageId) == nullptr ||
            strstr(message, msg.messageContents1) == nullptr ||
            strstr(message, msg.messageContents2) == nullptr)
        {
            continue;
        }

        // If the error is due to exposing coherent framebuffer fetch (without
        // VK_EXT_rasterization_order_attachment_access), but framebuffer fetch has not been used by
        // the application, report it.
        //
        // Note that currently syncval doesn't support the
        // VK_EXT_rasterization_order_attachment_access extension, so the syncval messages would
        // continue to be produced despite the extension.
        constexpr bool kSyncValSupportsRasterizationOrderExtension = false;
        const bool hasRasterizationOrderExtension =
            renderer->getFeatures().supportsRasterizationOrderAttachmentAccess.enabled &&
            kSyncValSupportsRasterizationOrderExtension;
        if (msg.isDueToNonConformantCoherentFramebufferFetch &&
            (!isFramebufferFetchUsed || hasRasterizationOrderExtension))
        {
            return DebugMessageReport::Print;
        }

        // Otherwise ignore the message
        return DebugMessageReport::Ignore;
    }

    return DebugMessageReport::Print;
}

const char *GetVkObjectTypeName(VkObjectType type)
{
    switch (type)
    {
        case VK_OBJECT_TYPE_UNKNOWN:
            return "Unknown";
        case VK_OBJECT_TYPE_INSTANCE:
            return "Instance";
        case VK_OBJECT_TYPE_PHYSICAL_DEVICE:
            return "Physical Device";
        case VK_OBJECT_TYPE_DEVICE:
            return "Device";
        case VK_OBJECT_TYPE_QUEUE:
            return "Queue";
        case VK_OBJECT_TYPE_SEMAPHORE:
            return "Semaphore";
        case VK_OBJECT_TYPE_COMMAND_BUFFER:
            return "Command Buffer";
        case VK_OBJECT_TYPE_FENCE:
            return "Fence";
        case VK_OBJECT_TYPE_DEVICE_MEMORY:
            return "Device Memory";
        case VK_OBJECT_TYPE_BUFFER:
            return "Buffer";
        case VK_OBJECT_TYPE_IMAGE:
            return "Image";
        case VK_OBJECT_TYPE_EVENT:
            return "Event";
        case VK_OBJECT_TYPE_QUERY_POOL:
            return "Query Pool";
        case VK_OBJECT_TYPE_BUFFER_VIEW:
            return "Buffer View";
        case VK_OBJECT_TYPE_IMAGE_VIEW:
            return "Image View";
        case VK_OBJECT_TYPE_SHADER_MODULE:
            return "Shader Module";
        case VK_OBJECT_TYPE_PIPELINE_CACHE:
            return "Pipeline Cache";
        case VK_OBJECT_TYPE_PIPELINE_LAYOUT:
            return "Pipeline Layout";
        case VK_OBJECT_TYPE_RENDER_PASS:
            return "Render Pass";
        case VK_OBJECT_TYPE_PIPELINE:
            return "Pipeline";
        case VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT:
            return "Descriptor Set Layout";
        case VK_OBJECT_TYPE_SAMPLER:
            return "Sampler";
        case VK_OBJECT_TYPE_DESCRIPTOR_POOL:
            return "Descriptor Pool";
        case VK_OBJECT_TYPE_DESCRIPTOR_SET:
            return "Descriptor Set";
        case VK_OBJECT_TYPE_FRAMEBUFFER:
            return "Framebuffer";
        case VK_OBJECT_TYPE_COMMAND_POOL:
            return "Command Pool";
        case VK_OBJECT_TYPE_SAMPLER_YCBCR_CONVERSION:
            return "Sampler YCbCr Conversion";
        case VK_OBJECT_TYPE_DESCRIPTOR_UPDATE_TEMPLATE:
            return "Descriptor Update Template";
        case VK_OBJECT_TYPE_SURFACE_KHR:
            return "Surface";
        case VK_OBJECT_TYPE_SWAPCHAIN_KHR:
            return "Swapchain";
        case VK_OBJECT_TYPE_DISPLAY_KHR:
            return "Display";
        case VK_OBJECT_TYPE_DISPLAY_MODE_KHR:
            return "Display Mode";
        case VK_OBJECT_TYPE_INDIRECT_COMMANDS_LAYOUT_NV:
            return "Indirect Commands Layout";
        case VK_OBJECT_TYPE_DEBUG_UTILS_MESSENGER_EXT:
            return "Debug Utils Messenger";
        case VK_OBJECT_TYPE_VALIDATION_CACHE_EXT:
            return "Validation Cache";
        case VK_OBJECT_TYPE_ACCELERATION_STRUCTURE_NV:
            return "Acceleration Structure";
        default:
            return "<Unrecognized>";
    }
}

VKAPI_ATTR VkBool32 VKAPI_CALL
DebugUtilsMessenger(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                    VkDebugUtilsMessageTypeFlagsEXT messageTypes,
                    const VkDebugUtilsMessengerCallbackDataEXT *callbackData,
                    void *userData)
{
    RendererVk *rendererVk = static_cast<RendererVk *>(userData);

    // See if it's an issue we are aware of and don't want to be spammed about.
    if (ShouldReportDebugMessage(rendererVk, callbackData->pMessageIdName,
                                 callbackData->pMessage) == DebugMessageReport::Ignore)
    {
        return VK_FALSE;
    }

    std::ostringstream log;
    if (callbackData->pMessageIdName)
    {
        log << "[ " << callbackData->pMessageIdName << " ] ";
    }
    log << callbackData->pMessage << std::endl;

    // Aesthetic value based on length of the function name, line number, etc.
    constexpr size_t kStartIndent = 28;

    // Output the debug marker hierarchy under which this error has occured.
    size_t indent = kStartIndent;
    if (callbackData->queueLabelCount > 0)
    {
        log << std::string(indent++, ' ') << "<Queue Label Hierarchy:>" << std::endl;
        for (uint32_t i = 0; i < callbackData->queueLabelCount; ++i)
        {
            log << std::string(indent++, ' ') << callbackData->pQueueLabels[i].pLabelName
                << std::endl;
        }
    }
    if (callbackData->cmdBufLabelCount > 0)
    {
        log << std::string(indent++, ' ') << "<Command Buffer Label Hierarchy:>" << std::endl;
        for (uint32_t i = 0; i < callbackData->cmdBufLabelCount; ++i)
        {
            log << std::string(indent++, ' ') << callbackData->pCmdBufLabels[i].pLabelName
                << std::endl;
        }
    }
    // Output the objects involved in this error message.
    if (callbackData->objectCount > 0)
    {
        for (uint32_t i = 0; i < callbackData->objectCount; ++i)
        {
            const char *objectName = callbackData->pObjects[i].pObjectName;
            const char *objectType = GetVkObjectTypeName(callbackData->pObjects[i].objectType);
            uint64_t objectHandle  = callbackData->pObjects[i].objectHandle;
            log << std::string(indent, ' ') << "Object: ";
            if (objectHandle == 0)
            {
                log << "VK_NULL_HANDLE";
            }
            else
            {
                log << "0x" << std::hex << objectHandle << std::dec;
            }
            log << " (type = " << objectType << "(" << callbackData->pObjects[i].objectType << "))";
            if (objectName)
            {
                log << " [" << objectName << "]";
            }
            log << std::endl;
        }
    }

    bool isError    = (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) != 0;
    std::string msg = log.str();

    rendererVk->onNewValidationMessage(msg);

    if (isError)
    {
        ERR() << msg;
    }
    else
    {
        WARN() << msg;
    }

    return VK_FALSE;
}

VKAPI_ATTR void VKAPI_CALL
MemoryReportCallback(const VkDeviceMemoryReportCallbackDataEXT *callbackData, void *userData)
{
    RendererVk *rendererVk = static_cast<RendererVk *>(userData);
    rendererVk->processMemoryReportCallback(*callbackData);
}

bool ShouldUseValidationLayers(const egl::AttributeMap &attribs)
{
#if defined(ANGLE_ENABLE_VULKAN_VALIDATION_LAYERS_BY_DEFAULT)
    return ShouldUseDebugLayers(attribs);
#else
    EGLAttrib debugSetting =
        attribs.get(EGL_PLATFORM_ANGLE_DEBUG_LAYERS_ENABLED_ANGLE, EGL_DONT_CARE);
    return debugSetting == EGL_TRUE;
#endif  // defined(ANGLE_ENABLE_VULKAN_VALIDATION_LAYERS_BY_DEFAULT)
}

gl::Version LimitVersionTo(const gl::Version &current, const gl::Version &lower)
{
    return std::min(current, lower);
}

[[maybe_unused]] bool FencePropertiesCompatibleWithAndroid(
    const VkExternalFenceProperties &externalFenceProperties)
{
    // handleType here is the external fence type -
    // we want type compatible with creating and export/dup() Android FD

    // Imported handleType that can be exported - need for vkGetFenceFdKHR()
    if ((externalFenceProperties.exportFromImportedHandleTypes &
         VK_EXTERNAL_FENCE_HANDLE_TYPE_SYNC_FD_BIT_KHR) == 0)
    {
        return false;
    }

    // HandleTypes which can be specified at creating a fence
    if ((externalFenceProperties.compatibleHandleTypes &
         VK_EXTERNAL_FENCE_HANDLE_TYPE_SYNC_FD_BIT_KHR) == 0)
    {
        return false;
    }

    constexpr VkExternalFenceFeatureFlags kFeatureFlags =
        (VK_EXTERNAL_FENCE_FEATURE_IMPORTABLE_BIT_KHR |
         VK_EXTERNAL_FENCE_FEATURE_EXPORTABLE_BIT_KHR);
    if ((externalFenceProperties.externalFenceFeatures & kFeatureFlags) != kFeatureFlags)
    {
        return false;
    }

    return true;
}

[[maybe_unused]] bool SemaphorePropertiesCompatibleWithAndroid(
    const VkExternalSemaphoreProperties &externalSemaphoreProperties)
{
    // handleType here is the external semaphore type -
    // we want type compatible with importing an Android FD

    constexpr VkExternalSemaphoreFeatureFlags kFeatureFlags =
        (VK_EXTERNAL_SEMAPHORE_FEATURE_IMPORTABLE_BIT_KHR);
    if ((externalSemaphoreProperties.externalSemaphoreFeatures & kFeatureFlags) != kFeatureFlags)
    {
        return false;
    }

    return true;
}

void ComputePipelineCacheVkChunkKey(VkPhysicalDeviceProperties physicalDeviceProperties,
                                    const uint8_t chunkIndex,
                                    egl::BlobCache::Key *hashOut)
{
    std::ostringstream hashStream("ANGLE Pipeline Cache: ", std::ios_base::ate);
    // Add the pipeline cache UUID to make sure the blob cache always gives a compatible pipeline
    // cache.  It's not particularly necessary to write it as a hex number as done here, so long as
    // there is no '\0' in the result.
    for (const uint32_t c : physicalDeviceProperties.pipelineCacheUUID)
    {
        hashStream << std::hex << c;
    }
    // Add the vendor and device id too for good measure.
    hashStream << std::hex << physicalDeviceProperties.vendorID;
    hashStream << std::hex << physicalDeviceProperties.deviceID;

    // Add chunkIndex to generate unique key for chunks.
    hashStream << std::hex << chunkIndex;

    const std::string &hashString = hashStream.str();
    angle::base::SHA1HashBytes(reinterpret_cast<const unsigned char *>(hashString.c_str()),
                               hashString.length(), hashOut->data());
}

void CompressAndStorePipelineCacheVk(VkPhysicalDeviceProperties physicalDeviceProperties,
                                     DisplayVk *displayVk,
                                     ContextVk *contextVk,
                                     const std::vector<uint8_t> &cacheData,
                                     const size_t maxTotalSize)
{
    // Though the pipeline cache will be compressed and divided into several chunks to store in blob
    // cache, the largest total size of blob cache is only 2M in android now, so there is no use to
    // handle big pipeline cache when android will reject it finally.
    if (cacheData.size() >= maxTotalSize)
    {
        // TODO: handle the big pipeline cache. http://anglebug.com/4722
        ANGLE_PERF_WARNING(contextVk->getDebug(), GL_DEBUG_SEVERITY_LOW,
                           "Skip syncing pipeline cache data when it's larger than maxTotalSize.");
        return;
    }

    // To make it possible to store more pipeline cache data, compress the whole pipelineCache.
    angle::MemoryBuffer compressedData;

    if (!egl::CompressBlobCacheData(cacheData.size(), cacheData.data(), &compressedData))
    {
        ANGLE_PERF_WARNING(contextVk->getDebug(), GL_DEBUG_SEVERITY_LOW,
                           "Skip syncing pipeline cache data as it failed compression.");
        return;
    }

    // If the size of compressedData is larger than (kMaxBlobCacheSize - sizeof(numChunks)),
    // the pipelineCache still can't be stored in blob cache. Divide the large compressed
    // pipelineCache into several parts to store seperately. There is no function to
    // query the limit size in android.
    constexpr size_t kMaxBlobCacheSize = 64 * 1024;

    // Store {numChunks, chunkCompressedData} in keyData, numChunks is used to validate the data.
    // For example, if the compressed size is 68841 bytes(67k), divide into {2,34421 bytes} and
    // {2,34420 bytes}.
    constexpr size_t kBlobHeaderSize = sizeof(uint8_t);
    size_t compressedOffset          = 0;

    const size_t numChunks = UnsignedCeilDivide(static_cast<unsigned int>(compressedData.size()),
                                                kMaxBlobCacheSize - kBlobHeaderSize);
    size_t chunkSize       = UnsignedCeilDivide(static_cast<unsigned int>(compressedData.size()),
                                                static_cast<unsigned int>(numChunks));

    for (size_t chunkIndex = 0; chunkIndex < numChunks; ++chunkIndex)
    {
        if (chunkIndex == numChunks - 1)
        {
            chunkSize = compressedData.size() - compressedOffset;
        }

        angle::MemoryBuffer keyData;
        if (!keyData.resize(kBlobHeaderSize + chunkSize))
        {
            ANGLE_PERF_WARNING(contextVk->getDebug(), GL_DEBUG_SEVERITY_LOW,
                               "Skip syncing pipeline cache data due to out of memory.");
            return;
        }

        ASSERT(numChunks <= UINT8_MAX);
        keyData.data()[0] = static_cast<uint8_t>(numChunks);
        memcpy(keyData.data() + kBlobHeaderSize, compressedData.data() + compressedOffset,
               chunkSize);
        compressedOffset += chunkSize;

        // Create unique hash key.
        egl::BlobCache::Key chunkCacheHash;
        ComputePipelineCacheVkChunkKey(physicalDeviceProperties, chunkIndex, &chunkCacheHash);

        displayVk->getBlobCache()->putApplication(chunkCacheHash, keyData);
    }
}

class CompressAndStorePipelineCacheTask : public angle::Closure
{
  public:
    CompressAndStorePipelineCacheTask(DisplayVk *displayVk,
                                      ContextVk *contextVk,
                                      std::vector<uint8_t> &&cacheData,
                                      size_t kMaxTotalSize)
        : mDisplayVk(displayVk),
          mContextVk(contextVk),
          mCacheData(std::move(cacheData)),
          mMaxTotalSize(kMaxTotalSize)
    {}

    void operator()() override
    {
        ANGLE_TRACE_EVENT0("gpu.angle", "CompressAndStorePipelineCacheVk");
        CompressAndStorePipelineCacheVk(mContextVk->getRenderer()->getPhysicalDeviceProperties(),
                                        mDisplayVk, mContextVk, mCacheData, mMaxTotalSize);
    }

  private:
    DisplayVk *mDisplayVk;
    ContextVk *mContextVk;
    std::vector<uint8_t> mCacheData;
    size_t mMaxTotalSize;
};

class WaitableCompressEventImpl : public WaitableCompressEvent
{
  public:
    WaitableCompressEventImpl(std::shared_ptr<angle::WaitableEvent> waitableEvent,
                              std::shared_ptr<CompressAndStorePipelineCacheTask> compressTask)
        : WaitableCompressEvent(waitableEvent), mCompressTask(compressTask)
    {}

  private:
    std::shared_ptr<CompressAndStorePipelineCacheTask> mCompressTask;
};

angle::Result GetAndDecompressPipelineCacheVk(VkPhysicalDeviceProperties physicalDeviceProperties,
                                              DisplayVk *displayVk,
                                              angle::MemoryBuffer *uncompressedData,
                                              bool *success)
{
    // Compute the hash key of chunkIndex 0 and find the first cache data in blob cache.
    egl::BlobCache::Key chunkCacheHash;
    ComputePipelineCacheVkChunkKey(physicalDeviceProperties, 0, &chunkCacheHash);
    egl::BlobCache::Value keyData;
    size_t keySize                   = 0;
    constexpr size_t kBlobHeaderSize = sizeof(uint8_t);

    if (!displayVk->getBlobCache()->get(displayVk->getScratchBuffer(), chunkCacheHash, &keyData,
                                        &keySize) ||
        keyData.size() < kBlobHeaderSize)
    {
        // Nothing in the cache.
        return angle::Result::Continue;
    }

    // Get the number of chunks.
    size_t numChunks      = keyData.data()[0];
    size_t chunkSize      = keySize - kBlobHeaderSize;
    size_t compressedSize = 0;

    // Allocate enough memory.
    angle::MemoryBuffer compressedData;
    ANGLE_VK_CHECK(displayVk, compressedData.resize(chunkSize * numChunks),
                   VK_ERROR_INITIALIZATION_FAILED);

    // To combine the parts of the pipelineCache data.
    for (size_t chunkIndex = 0; chunkIndex < numChunks; ++chunkIndex)
    {
        // Get the unique key by chunkIndex.
        ComputePipelineCacheVkChunkKey(physicalDeviceProperties, chunkIndex, &chunkCacheHash);

        if (!displayVk->getBlobCache()->get(displayVk->getScratchBuffer(), chunkCacheHash, &keyData,
                                            &keySize) ||
            keyData.size() < kBlobHeaderSize)
        {
            // Can't find every part of the cache data.
            WARN() << "Failed to get pipeline cache chunk " << chunkIndex << " of " << numChunks;
            return angle::Result::Continue;
        }

        size_t checkNumber = keyData.data()[0];
        chunkSize          = keySize - kBlobHeaderSize;

        if (checkNumber != numChunks || compressedData.size() < (compressedSize + chunkSize))
        {
            // Validate the number value and enough space to store.
            WARN() << "Pipeline cache chunk header corrupted: checkNumber = " << checkNumber
                   << ", numChunks = " << numChunks
                   << ", compressedData.size() = " << compressedData.size()
                   << ", (compressedSize + chunkSize) = " << (compressedSize + chunkSize);
            return angle::Result::Continue;
        }
        memcpy(compressedData.data() + compressedSize, keyData.data() + kBlobHeaderSize, chunkSize);
        compressedSize += chunkSize;
    }

    ANGLE_VK_CHECK(
        displayVk,
        egl::DecompressBlobCacheData(compressedData.data(), compressedSize, uncompressedData),
        VK_ERROR_INITIALIZATION_FAILED);

    *success = true;
    return angle::Result::Continue;
}

// Environment variable (and associated Android property) to enable Vulkan debug-utils markers
constexpr char kEnableDebugMarkersVarName[]      = "ANGLE_ENABLE_DEBUG_MARKERS";
constexpr char kEnableDebugMarkersPropertyName[] = "debug.angle.markers";

ANGLE_INLINE gl::ShadingRate GetShadingRateFromVkExtent(const VkExtent2D &extent)
{
    if (extent.width == 1 && extent.height == 2)
    {
        return gl::ShadingRate::_1x2;
    }
    else if (extent.width == 2 && extent.height == 1)
    {
        return gl::ShadingRate::_2x1;
    }
    else if (extent.width == 2 && extent.height == 2)
    {
        return gl::ShadingRate::_2x2;
    }
    else if (extent.width == 4 && extent.height == 2)
    {
        return gl::ShadingRate::_4x2;
    }
    else if (extent.width == 4 && extent.height == 4)
    {
        return gl::ShadingRate::_4x4;
    }

    return gl::ShadingRate::_1x1;
}

// Check for currently allocated memory. It is used at the end of the renderer object and when there
// is an allocation error (from ANGLE_VK_TRY()).
void checkForCurrentMemoryAllocations(RendererVk *renderer)
{
    if (kTrackMemoryAllocationDebug)
    {
        for (uint32_t i = 0; i < vk::kMemoryAllocationTypeCount; i++)
        {
            if (renderer->getMemoryAllocationTracker()->getActiveMemoryAllocationsSize(i) == 0)
            {
                continue;
            }

            std::stringstream outStream;

            outStream << "Currently allocated size for memory allocation type ("
                      << vk::kMemoryAllocationTypeMessage[i] << "): "
                      << renderer->getMemoryAllocationTracker()->getActiveMemoryAllocationsSize(i)
                      << " | Count: "
                      << renderer->getMemoryAllocationTracker()->getActiveMemoryAllocationsCount(i)
                      << std::endl;

            for (uint32_t heapIndex = 0;
                 heapIndex < renderer->getMemoryProperties().getMemoryHeapCount(); heapIndex++)
            {
                outStream
                    << "--> Heap index " << heapIndex << ": "
                    << renderer->getMemoryAllocationTracker()->getActiveHeapMemoryAllocationsSize(
                           i, heapIndex)
                    << " | Count: "
                    << renderer->getMemoryAllocationTracker()->getActiveHeapMemoryAllocationsCount(
                           i, heapIndex)
                    << std::endl;
            }

            INFO() << outStream.str();
        }
    }
    else if (kTrackMemoryAllocationSizes)
    {
        for (uint32_t i = 0; i < vk::kMemoryAllocationTypeCount; i++)
        {
            if (renderer->getMemoryAllocationTracker()->getActiveMemoryAllocationsSize(i) == 0)
            {
                continue;
            }

            std::stringstream outStream;

            outStream << "Currently allocated size for memory allocation type ("
                      << vk::kMemoryAllocationTypeMessage[i] << "): "
                      << renderer->getMemoryAllocationTracker()->getActiveMemoryAllocationsSize(i)
                      << std::endl;

            for (uint32_t heapIndex = 0;
                 heapIndex < renderer->getMemoryProperties().getMemoryHeapCount(); heapIndex++)
            {
                outStream << "--> Heap index " << heapIndex << ": "
                          << renderer->getMemoryAllocationTracker()
                                 ->getActiveHeapMemoryAllocationsSize(i, heapIndex)
                          << std::endl;
            }

            INFO() << outStream.str();
        }
    }
}

// In case of an allocation error, log pending memory allocation if the size in non-zero.
void logPendingMemoryAllocation(RendererVk *renderer)
{
    if (!kTrackMemoryAllocationSizes)
    {
        return;
    }

    vk::MemoryAllocationType allocInfo =
        renderer->getMemoryAllocationTracker()->getPendingMemoryAllocationType();
    VkDeviceSize allocSize =
        renderer->getMemoryAllocationTracker()->getPendingMemoryAllocationSize();
    uint32_t memoryHeapIndex = renderer->getMemoryProperties().getHeapIndexForMemoryType(
        renderer->getMemoryAllocationTracker()->getPendingMemoryTypeIndex());

    if (allocSize != 0)
    {
        std::stringstream outStream;

        outStream << "Pending allocation size for memory allocation type ("
                  << vk::kMemoryAllocationTypeMessage[ToUnderlying(allocInfo)]
                  << ") for heap index " << memoryHeapIndex << ": " << allocSize;
        WARN() << outStream.str();
    }
}

// Output memory log stream based on level of severity.
void outputMemoryLogStream(std::stringstream &outStream, vk::MemoryLogSeverity severity)
{
    if (!kTrackMemoryAllocationSizes)
    {
        return;
    }

    switch (severity)
    {
        case vk::MemoryLogSeverity::INFO:
            INFO() << outStream.str();
            break;
        case vk::MemoryLogSeverity::WARN:
            WARN() << outStream.str();
            break;
        default:
            UNREACHABLE();
            break;
    }
}

void logMemoryHeapStats(RendererVk *renderer, vk::MemoryLogSeverity severity)
{
    if (!kTrackMemoryAllocationSizes)
    {
        return;
    }

    // Log stream for the heap information.
    std::stringstream outStream;

    // VkPhysicalDeviceMemoryProperties2 enables the use of memory budget properties if supported.
    VkPhysicalDeviceMemoryProperties2KHR memoryProperties;
    memoryProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2_KHR;
    memoryProperties.pNext = nullptr;

    VkPhysicalDeviceMemoryBudgetPropertiesEXT memoryBudgetProperties;
    memoryBudgetProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT;
    memoryBudgetProperties.pNext = nullptr;

    if (renderer->getFeatures().supportsMemoryBudget.enabled)
    {
        vk::AddToPNextChain(&memoryProperties, &memoryBudgetProperties);
    }

    vkGetPhysicalDeviceMemoryProperties2KHR(renderer->getPhysicalDevice(), &memoryProperties);

    // Add memory heap information to the stream.
    outStream << "Memory heap info" << std::endl;

    outStream << std::endl << "* Available memory heaps:" << std::endl;
    for (uint32_t i = 0; i < memoryProperties.memoryProperties.memoryHeapCount; i++)
    {
        outStream << std::dec << i
                  << " | Heap size: " << memoryProperties.memoryProperties.memoryHeaps[i].size
                  << " | Flags: 0x" << std::hex
                  << memoryProperties.memoryProperties.memoryHeaps[i].flags << std::endl;
    }

    if (renderer->getFeatures().supportsMemoryBudget.enabled)
    {
        outStream << std::endl << "* Available memory budget and usage per heap:" << std::endl;
        for (uint32_t i = 0; i < memoryProperties.memoryProperties.memoryHeapCount; i++)
        {
            outStream << std::dec << i << " | Heap budget: " << memoryBudgetProperties.heapBudget[i]
                      << " | Heap usage: " << memoryBudgetProperties.heapUsage[i] << std::endl;
        }
    }

    outStream << std::endl << "* Available memory types:" << std::endl;
    for (uint32_t i = 0; i < memoryProperties.memoryProperties.memoryTypeCount; i++)
    {
        outStream << std::dec << i
                  << " | Heap index: " << memoryProperties.memoryProperties.memoryTypes[i].heapIndex
                  << " | Property flags: 0x" << std::hex
                  << memoryProperties.memoryProperties.memoryTypes[i].propertyFlags << std::endl;
    }

    // Output the log stream based on the level of severity.
    outputMemoryLogStream(outStream, severity);
}
}  // namespace

// OneOffCommandPool implementation.
void OneOffCommandPool::destroy(VkDevice device)
{
    std::unique_lock<std::mutex> lock(mMutex);
    for (PendingOneOffCommands &pending : mPendingCommands)
    {
        pending.commandBuffer.releaseHandle();
    }
    mCommandPool.destroy(device);
}

angle::Result OneOffCommandPool::getCommandBuffer(vk::Context *context,
                                                  bool hasProtectedContent,
                                                  vk::PrimaryCommandBuffer *commandBufferOut)
{
    std::unique_lock<std::mutex> lock(mMutex);

    if (!mPendingCommands.empty() &&
        !context->getRenderer()->hasUnfinishedUse(mPendingCommands.front().use))
    {
        *commandBufferOut = std::move(mPendingCommands.front().commandBuffer);
        mPendingCommands.pop_front();
        ANGLE_VK_TRY(context, commandBufferOut->reset());
    }
    else
    {
        if (!mCommandPool.valid())
        {
            VkCommandPoolCreateInfo createInfo = {};
            createInfo.sType                   = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
            createInfo.flags                   = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT |
                               (hasProtectedContent ? VK_COMMAND_POOL_CREATE_PROTECTED_BIT : 0);
            ANGLE_VK_TRY(context, mCommandPool.init(context->getDevice(), createInfo));
        }

        VkCommandBufferAllocateInfo allocInfo = {};
        allocInfo.sType                       = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.level                       = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount          = 1;
        allocInfo.commandPool                 = mCommandPool.getHandle();

        ANGLE_VK_TRY(context, commandBufferOut->init(context->getDevice(), allocInfo));
    }

    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType                    = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags                    = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    beginInfo.pInheritanceInfo         = nullptr;
    ANGLE_VK_TRY(context, commandBufferOut->begin(beginInfo));

    return angle::Result::Continue;
}

void OneOffCommandPool::releaseCommandBuffer(const QueueSerial &submitQueueSerial,
                                             vk::PrimaryCommandBuffer &&primary)
{
    std::unique_lock<std::mutex> lock(mMutex);
    mPendingCommands.push_back({vk::ResourceUse(submitQueueSerial), std::move(primary)});
}

// RendererVk implementation.
RendererVk::RendererVk()
    : mDisplay(nullptr),
      mLibVulkanLibrary(nullptr),
      mCapsInitialized(false),
      mApiVersion(0),
      mInstance(VK_NULL_HANDLE),
      mEnableValidationLayers(false),
      mEnableDebugUtils(false),
      mAngleDebuggerMode(false),
      mEnabledICD(angle::vk::ICD::Default),
      mDebugUtilsMessenger(VK_NULL_HANDLE),
      mPhysicalDevice(VK_NULL_HANDLE),
      mMaxVertexAttribDivisor(1),
      mCurrentQueueFamilyIndex(std::numeric_limits<uint32_t>::max()),
      mMaxVertexAttribStride(0),
      mMinImportedHostPointerAlignment(1),
      mDefaultUniformBufferSize(kPreferredDefaultUniformBufferSize),
      mDevice(VK_NULL_HANDLE),
      mDeviceLost(false),
      mSuballocationGarbageSizeInBytes(0),
      mSuballocationGarbageDestroyed(0),
      mSuballocationGarbageSizeInBytesCachedAtomic(0),
      mCoherentStagingBufferMemoryTypeIndex(kInvalidMemoryTypeIndex),
      mNonCoherentStagingBufferMemoryTypeIndex(kInvalidMemoryTypeIndex),
      mStagingBufferAlignment(1),
      mHostVisibleVertexConversionBufferMemoryTypeIndex(kInvalidMemoryTypeIndex),
      mDeviceLocalVertexConversionBufferMemoryTypeIndex(kInvalidMemoryTypeIndex),
      mVertexConversionBufferAlignment(1),
      mPipelineCacheVkUpdateTimeout(kPipelineCacheVkUpdatePeriod),
      mPipelineCacheSizeAtLastSync(0),
      mPipelineCacheInitialized(false),
      mValidationMessageCount(0),
      mCommandProcessor(this, &mCommandQueue),
      mSupportedVulkanPipelineStageMask(0),
      mSupportedVulkanShaderStageMask(0),
      mMemoryAllocationTracker(MemoryAllocationTracker(this))
{
    VkFormatProperties invalid = {0, 0, kInvalidFormatFeatureFlags};
    mFormatProperties.fill(invalid);

    // We currently don't have any big-endian devices in the list of supported platforms.  There are
    // a number of places in the Vulkan backend that make this assumption.  This assertion is made
    // early to fail immediately on big-endian platforms.
    ASSERT(IsLittleEndian());
}

RendererVk::~RendererVk()
{
    mAllocator.release();
    mPipelineCache.release();
    ASSERT(!hasSharedGarbage());

    if (mLibVulkanLibrary)
    {
        angle::CloseSystemLibrary(mLibVulkanLibrary);
    }
}

bool RendererVk::hasSharedGarbage()
{
    std::unique_lock<std::mutex> lock(mGarbageMutex);
    return !mSharedGarbage.empty() || !mPendingSubmissionGarbage.empty() ||
           !mSuballocationGarbage.empty() || !mPendingSubmissionSuballocationGarbage.empty();
}

void RendererVk::onDestroy(vk::Context *context)
{
    if (isDeviceLost())
    {
        handleDeviceLost();
    }

    for (std::unique_ptr<vk::BufferBlock> &block : mOrphanedBufferBlocks)
    {
        ASSERT(block->isEmpty());
        block->destroy(this);
    }
    mOrphanedBufferBlocks.clear();

    if (isAsyncCommandQueueEnabled())
    {
        mCommandProcessor.destroy(context);
    }

    mCommandQueue.destroy(context);

    // mCommandQueue.destroy should already set "last completed" serials to infinite.
    cleanupGarbage();
    ASSERT(!hasSharedGarbage());

    mOneOffCommandPool.destroy(mDevice);

    mPipelineCache.destroy(mDevice);
    mSamplerCache.destroy(this);
    mYuvConversionCache.destroy(this);
    mVkFormatDescriptorCountMap.clear();

    mOutsideRenderPassCommandBufferRecycler.onDestroy();
    mRenderPassCommandBufferRecycler.onDestroy();

    mAllocator.destroy();

    // When the renderer is being destroyed, it is possible to check if all the allocated memory
    // throughout the execution has been freed.
    if (kTrackMemoryAllocationDebug)
    {
        checkForCurrentMemoryAllocations(this);
    }

    if (mDevice)
    {
        vkDestroyDevice(mDevice, nullptr);
        mDevice = VK_NULL_HANDLE;
    }

    if (mDebugUtilsMessenger)
    {
        vkDestroyDebugUtilsMessengerEXT(mInstance, mDebugUtilsMessenger, nullptr);
    }

    logCacheStats();

    if (mInstance)
    {
        vkDestroyInstance(mInstance, nullptr);
        mInstance = VK_NULL_HANDLE;
    }

    if (mCompressEvent)
    {
        mCompressEvent->wait();
        mCompressEvent.reset();
    }

    mMemoryProperties.destroy();
    mPhysicalDevice = VK_NULL_HANDLE;

    mEnabledInstanceExtensions.clear();
    mEnabledDeviceExtensions.clear();
}

void RendererVk::notifyDeviceLost()
{
    mDeviceLost = true;
    mDisplay->notifyDeviceLost();
}

bool RendererVk::isDeviceLost() const
{
    return mDeviceLost;
}

angle::Result RendererVk::initialize(DisplayVk *displayVk,
                                     egl::Display *display,
                                     const char *wsiExtension,
                                     const char *wsiLayer)
{
    bool canLoadDebugUtils = true;
#if defined(ANGLE_SHARED_LIBVULKAN)
    {
        ANGLE_SCOPED_DISABLE_MSAN();
        mLibVulkanLibrary = angle::vk::OpenLibVulkan();
        ANGLE_VK_CHECK(displayVk, mLibVulkanLibrary, VK_ERROR_INITIALIZATION_FAILED);

        PFN_vkGetInstanceProcAddr vulkanLoaderGetInstanceProcAddr =
            reinterpret_cast<PFN_vkGetInstanceProcAddr>(
                angle::GetLibrarySymbol(mLibVulkanLibrary, "vkGetInstanceProcAddr"));

        // Set all vk* function ptrs
        volkInitializeCustom(vulkanLoaderGetInstanceProcAddr);

        uint32_t ver = volkGetInstanceVersion();
        if (!IsAndroid() && VK_API_VERSION_MAJOR(ver) == 1 &&
            (VK_API_VERSION_MINOR(ver) < 1 ||
             (VK_API_VERSION_MINOR(ver) == 1 && VK_API_VERSION_PATCH(ver) < 91)))
        {
            // http://crbug.com/1205999 - non-Android Vulkan Loader versions before 1.1.91 have a
            // bug which prevents loading VK_EXT_debug_utils function pointers.
            canLoadDebugUtils = false;
        }
    }
#endif  // defined(ANGLE_SHARED_LIBVULKAN)

    mDisplay                         = display;
    const egl::AttributeMap &attribs = mDisplay->getAttributeMap();
    angle::vk::ScopedVkLoaderEnvironment scopedEnvironment(ShouldUseValidationLayers(attribs),
                                                           ChooseICDFromAttribs(attribs));
    mEnableValidationLayers = scopedEnvironment.canEnableValidationLayers();
    mEnabledICD             = scopedEnvironment.getEnabledICD();

    // Gather global layer properties.
    uint32_t instanceLayerCount = 0;
    {
        ANGLE_SCOPED_DISABLE_LSAN();
        ANGLE_SCOPED_DISABLE_MSAN();
        ANGLE_VK_TRY(displayVk, vkEnumerateInstanceLayerProperties(&instanceLayerCount, nullptr));
    }

    std::vector<VkLayerProperties> instanceLayerProps(instanceLayerCount);
    if (instanceLayerCount > 0)
    {
        ANGLE_SCOPED_DISABLE_LSAN();
        ANGLE_SCOPED_DISABLE_MSAN();
        ANGLE_VK_TRY(displayVk, vkEnumerateInstanceLayerProperties(&instanceLayerCount,
                                                                   instanceLayerProps.data()));
    }

    VulkanLayerVector enabledInstanceLayerNames;
    if (mEnableValidationLayers)
    {
        bool layersRequested =
            (attribs.get(EGL_PLATFORM_ANGLE_DEBUG_LAYERS_ENABLED_ANGLE, EGL_DONT_CARE) == EGL_TRUE);
        mEnableValidationLayers = GetAvailableValidationLayers(instanceLayerProps, layersRequested,
                                                               &enabledInstanceLayerNames);
    }

    if (wsiLayer)
    {
        enabledInstanceLayerNames.push_back(wsiLayer);
    }

    // Enumerate instance extensions that are provided by the vulkan
    // implementation and implicit layers.
    uint32_t instanceExtensionCount = 0;
    {
        ANGLE_SCOPED_DISABLE_LSAN();
        ANGLE_SCOPED_DISABLE_MSAN();
        ANGLE_VK_TRY(displayVk, vkEnumerateInstanceExtensionProperties(
                                    nullptr, &instanceExtensionCount, nullptr));
    }

    std::vector<VkExtensionProperties> instanceExtensionProps(instanceExtensionCount);
    if (instanceExtensionCount > 0)
    {
        ANGLE_SCOPED_DISABLE_LSAN();
        ANGLE_SCOPED_DISABLE_MSAN();
        ANGLE_VK_TRY(displayVk,
                     vkEnumerateInstanceExtensionProperties(nullptr, &instanceExtensionCount,
                                                            instanceExtensionProps.data()));
        // In case fewer items were returned than requested, resize instanceExtensionProps to the
        // number of extensions returned (i.e. instanceExtensionCount).
        instanceExtensionProps.resize(instanceExtensionCount);
    }

    // Enumerate instance extensions that are provided by explicit layers.
    for (const char *layerName : enabledInstanceLayerNames)
    {
        uint32_t previousExtensionCount      = static_cast<uint32_t>(instanceExtensionProps.size());
        uint32_t instanceLayerExtensionCount = 0;
        {
            ANGLE_SCOPED_DISABLE_LSAN();
            ANGLE_SCOPED_DISABLE_MSAN();
            ANGLE_VK_TRY(displayVk, vkEnumerateInstanceExtensionProperties(
                                        layerName, &instanceLayerExtensionCount, nullptr));
        }
        instanceExtensionProps.resize(previousExtensionCount + instanceLayerExtensionCount);
        {
            ANGLE_SCOPED_DISABLE_LSAN();
            ANGLE_SCOPED_DISABLE_MSAN();
            ANGLE_VK_TRY(displayVk, vkEnumerateInstanceExtensionProperties(
                                        layerName, &instanceLayerExtensionCount,
                                        instanceExtensionProps.data() + previousExtensionCount));
        }
        // In case fewer items were returned than requested, resize instanceExtensionProps to the
        // number of extensions returned (i.e. instanceLayerExtensionCount).
        instanceExtensionProps.resize(previousExtensionCount + instanceLayerExtensionCount);
    }

    vk::ExtensionNameList instanceExtensionNames;
    if (!instanceExtensionProps.empty())
    {
        for (const VkExtensionProperties &i : instanceExtensionProps)
        {
            instanceExtensionNames.push_back(i.extensionName);
        }
        std::sort(instanceExtensionNames.begin(), instanceExtensionNames.end(), StrLess);
    }

    if (displayVk->isUsingSwapchain())
    {
        mEnabledInstanceExtensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
        if (ExtensionFound(VK_EXT_SWAPCHAIN_COLOR_SPACE_EXTENSION_NAME, instanceExtensionNames))
        {
            mEnabledInstanceExtensions.push_back(VK_EXT_SWAPCHAIN_COLOR_SPACE_EXTENSION_NAME);
        }
    }

    if (wsiExtension)
    {
        mEnabledInstanceExtensions.push_back(wsiExtension);
    }

    mEnableDebugUtils = canLoadDebugUtils && mEnableValidationLayers &&
                        ExtensionFound(VK_EXT_DEBUG_UTILS_EXTENSION_NAME, instanceExtensionNames);

    if (mEnableDebugUtils)
    {
        mEnabledInstanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    if (ExtensionFound(VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME, instanceExtensionNames))
    {
        mEnabledInstanceExtensions.push_back(VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME);
        ANGLE_FEATURE_CONDITION(&mFeatures, supportsSurfaceCapabilities2Extension, true);
    }

    if (ExtensionFound(VK_KHR_SURFACE_PROTECTED_CAPABILITIES_EXTENSION_NAME,
                       instanceExtensionNames))
    {
        mEnabledInstanceExtensions.push_back(VK_KHR_SURFACE_PROTECTED_CAPABILITIES_EXTENSION_NAME);
        ANGLE_FEATURE_CONDITION(&mFeatures, supportsSurfaceProtectedCapabilitiesExtension, true);
    }

    if (ExtensionFound(VK_GOOGLE_SURFACELESS_QUERY_EXTENSION_NAME, instanceExtensionNames))
    {
        // TODO: Validation layer has a bug when vkGetPhysicalDeviceSurfaceFormats2KHR is called
        // on Mock ICD with surface handle set as VK_NULL_HANDLE. http://anglebug.com/7631
        mEnabledInstanceExtensions.push_back(VK_GOOGLE_SURFACELESS_QUERY_EXTENSION_NAME);
        ANGLE_FEATURE_CONDITION(&mFeatures, supportsSurfacelessQueryExtension, !isMockICDEnabled());
    }

    // Enable VK_KHR_get_physical_device_properties_2 if available.
    if (ExtensionFound(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
                       instanceExtensionNames))
    {
        mEnabledInstanceExtensions.push_back(
            VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
    }

    if (ExtensionFound(VK_EXT_SURFACE_MAINTENANCE_1_EXTENSION_NAME, instanceExtensionNames))
    {
        mEnabledInstanceExtensions.push_back(VK_EXT_SURFACE_MAINTENANCE_1_EXTENSION_NAME);
    }

    if (ExtensionFound(VK_KHR_EXTERNAL_FENCE_CAPABILITIES_EXTENSION_NAME, instanceExtensionNames))
    {
        mEnabledInstanceExtensions.push_back(VK_KHR_EXTERNAL_FENCE_CAPABILITIES_EXTENSION_NAME);
        ANGLE_FEATURE_CONDITION(&mFeatures, supportsExternalFenceCapabilities, true);
    }

    if (ExtensionFound(VK_KHR_EXTERNAL_SEMAPHORE_CAPABILITIES_EXTENSION_NAME,
                       instanceExtensionNames))
    {
        mEnabledInstanceExtensions.push_back(VK_KHR_EXTERNAL_SEMAPHORE_CAPABILITIES_EXTENSION_NAME);
        ANGLE_FEATURE_CONDITION(&mFeatures, supportsExternalSemaphoreCapabilities, true);
    }

    // Verify the required extensions are in the extension names set. Fail if not.
    std::sort(mEnabledInstanceExtensions.begin(), mEnabledInstanceExtensions.end(), StrLess);
    ANGLE_VK_TRY(displayVk,
                 VerifyExtensionsPresent(instanceExtensionNames, mEnabledInstanceExtensions));

    mApplicationInfo                    = {};
    mApplicationInfo.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    std::string appName                 = angle::GetExecutableName();
    mApplicationInfo.pApplicationName   = appName.c_str();
    mApplicationInfo.applicationVersion = 1;
    mApplicationInfo.pEngineName        = "ANGLE";
    mApplicationInfo.engineVersion      = 1;

    auto enumerateInstanceVersion = reinterpret_cast<PFN_vkEnumerateInstanceVersion>(
        vkGetInstanceProcAddr(nullptr, "vkEnumerateInstanceVersion"));
    if (!enumerateInstanceVersion)
    {
        mApiVersion = VK_API_VERSION_1_0;
    }
    else
    {
        uint32_t apiVersion = VK_API_VERSION_1_0;
        {
            ANGLE_SCOPED_DISABLE_LSAN();
            ANGLE_SCOPED_DISABLE_MSAN();
            ANGLE_VK_TRY(displayVk, enumerateInstanceVersion(&apiVersion));
        }
        if ((VK_VERSION_MAJOR(apiVersion) > 1) || (VK_VERSION_MINOR(apiVersion) >= 1))
        {
            // This is the highest version of core Vulkan functionality that ANGLE uses.
            mApiVersion = kPreferredVulkanAPIVersion;
        }
        else
        {
            // Since only 1.0 instance-level functionality is available, this must set to 1.0.
            mApiVersion = VK_API_VERSION_1_0;
        }
    }
    mApplicationInfo.apiVersion = mApiVersion;

    VkInstanceCreateInfo instanceInfo = {};
    instanceInfo.sType                = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceInfo.flags                = 0;
    instanceInfo.pApplicationInfo     = &mApplicationInfo;

    // Enable requested layers and extensions.
    instanceInfo.enabledExtensionCount = static_cast<uint32_t>(mEnabledInstanceExtensions.size());
    instanceInfo.ppEnabledExtensionNames =
        mEnabledInstanceExtensions.empty() ? nullptr : mEnabledInstanceExtensions.data();

    instanceInfo.enabledLayerCount   = static_cast<uint32_t>(enabledInstanceLayerNames.size());
    instanceInfo.ppEnabledLayerNames = enabledInstanceLayerNames.data();

    // http://anglebug.com/7050 - Shader validation caching is broken on Android
    VkValidationFeaturesEXT validationFeatures       = {};
    VkValidationFeatureDisableEXT disabledFeatures[] = {
        VK_VALIDATION_FEATURE_DISABLE_SHADER_VALIDATION_CACHE_EXT};
    if (mEnableValidationLayers && IsAndroid())
    {
        validationFeatures.sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT;
        validationFeatures.disabledValidationFeatureCount = 1;
        validationFeatures.pDisabledValidationFeatures    = disabledFeatures;

        vk::AddToPNextChain(&instanceInfo, &validationFeatures);
    }

    {
        ANGLE_SCOPED_DISABLE_MSAN();
        ANGLE_VK_TRY(displayVk, vkCreateInstance(&instanceInfo, nullptr, &mInstance));
#if defined(ANGLE_SHARED_LIBVULKAN)
        // Load volk if we are linking dynamically
        volkLoadInstance(mInstance);
#endif  // defined(ANGLE_SHARED_LIBVULKAN)
    }

    if (mEnableDebugUtils)
    {
        // Use the newer EXT_debug_utils if it exists.
#if !defined(ANGLE_SHARED_LIBVULKAN)
        InitDebugUtilsEXTFunctions(mInstance);
#endif  // !defined(ANGLE_SHARED_LIBVULKAN)

        // Create the messenger callback.
        VkDebugUtilsMessengerCreateInfoEXT messengerInfo = {};

        constexpr VkDebugUtilsMessageSeverityFlagsEXT kSeveritiesToLog =
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;

        constexpr VkDebugUtilsMessageTypeFlagsEXT kMessagesToLog =
            VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;

        messengerInfo.sType           = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        messengerInfo.messageSeverity = kSeveritiesToLog;
        messengerInfo.messageType     = kMessagesToLog;
        messengerInfo.pfnUserCallback = &DebugUtilsMessenger;
        messengerInfo.pUserData       = this;

        ANGLE_VK_TRY(displayVk, vkCreateDebugUtilsMessengerEXT(mInstance, &messengerInfo, nullptr,
                                                               &mDebugUtilsMessenger));
    }

    if (std::find(mEnabledInstanceExtensions.begin(), mEnabledInstanceExtensions.end(),
                  VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME) !=
        mEnabledInstanceExtensions.end())
    {
#if !defined(ANGLE_SHARED_LIBVULKAN)
        InitGetPhysicalDeviceProperties2KHRFunctions(mInstance);
#endif  // !defined(ANGLE_SHARED_LIBVULKAN)
        ASSERT(vkGetPhysicalDeviceProperties2KHR);
    }

    uint32_t physicalDeviceCount = 0;
    ANGLE_VK_TRY(displayVk, vkEnumeratePhysicalDevices(mInstance, &physicalDeviceCount, nullptr));
    ANGLE_VK_CHECK(displayVk, physicalDeviceCount > 0, VK_ERROR_INITIALIZATION_FAILED);

    // TODO(jmadill): Handle multiple physical devices. For now, use the first device.
    std::vector<VkPhysicalDevice> physicalDevices(physicalDeviceCount);
    ANGLE_VK_TRY(displayVk, vkEnumeratePhysicalDevices(mInstance, &physicalDeviceCount,
                                                       physicalDevices.data()));
    uint32_t preferredVendorId =
        static_cast<uint32_t>(attribs.get(EGL_PLATFORM_ANGLE_DEVICE_ID_HIGH_ANGLE, 0));
    uint32_t preferredDeviceId =
        static_cast<uint32_t>(attribs.get(EGL_PLATFORM_ANGLE_DEVICE_ID_LOW_ANGLE, 0));
    ChoosePhysicalDevice(vkGetPhysicalDeviceProperties, physicalDevices, mEnabledICD,
                         preferredVendorId, preferredDeviceId, &mPhysicalDevice,
                         &mPhysicalDeviceProperties);

    mGarbageCollectionFlushThreshold =
        static_cast<uint32_t>(mPhysicalDeviceProperties.limits.maxMemoryAllocationCount *
                              kPercentMaxMemoryAllocationCount);
    vkGetPhysicalDeviceFeatures(mPhysicalDevice, &mPhysicalDeviceFeatures);

    // Ensure we can find a graphics queue family.
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(mPhysicalDevice, &queueFamilyCount, nullptr);

    ANGLE_VK_CHECK(displayVk, queueFamilyCount > 0, VK_ERROR_INITIALIZATION_FAILED);

    mQueueFamilyProperties.resize(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(mPhysicalDevice, &queueFamilyCount,
                                             mQueueFamilyProperties.data());

    uint32_t queueFamilyMatchCount = 0;
    // Try first for a protected graphics queue family
    uint32_t firstGraphicsQueueFamily = vk::QueueFamily::FindIndex(
        mQueueFamilyProperties,
        (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_PROTECTED_BIT), 0,
        &queueFamilyMatchCount);
    // else just a graphics queue family
    if (queueFamilyMatchCount == 0)
    {
        firstGraphicsQueueFamily = vk::QueueFamily::FindIndex(
            mQueueFamilyProperties, (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT), 0,
            &queueFamilyMatchCount);
    }
    ANGLE_VK_CHECK(displayVk, queueFamilyMatchCount > 0, VK_ERROR_INITIALIZATION_FAILED);

    // Store the physical device memory properties so we can find the right memory pools.
    mMemoryProperties.init(mPhysicalDevice);
    ANGLE_VK_CHECK(displayVk, mMemoryProperties.getMemoryTypeCount() > 0,
                   VK_ERROR_INITIALIZATION_FAILED);

    // The counters for the memory allocation tracker should be initialized.
    // Each memory allocation could be made in one of the available memory heaps. We initialize the
    // per-heap memory allocation trackers for MemoryAllocationType objects here, after
    // mMemoryProperties has been set up.
    mMemoryAllocationTracker.initMemoryTrackers();

    // If only one queue family, go ahead and initialize the device. If there is more than one
    // queue, we'll have to wait until we see a WindowSurface to know which supports present.
    if (queueFamilyMatchCount == 1)
    {
        ANGLE_TRY(initializeDevice(displayVk, firstGraphicsQueueFamily));
    }

    ANGLE_TRY(initializeMemoryAllocator(displayVk));

    // Initialize the format table.
    mFormatTable.initialize(this, &mNativeTextureCaps);

    setGlobalDebugAnnotator();

    // Null terminate the extension list returned for EGL_VULKAN_INSTANCE_EXTENSIONS_ANGLE.
    mEnabledInstanceExtensions.push_back(nullptr);

    return angle::Result::Continue;
}

angle::Result RendererVk::initializeMemoryAllocator(DisplayVk *displayVk)
{
    // This number matches Chromium and was picked by looking at memory usage of
    // Android apps. The allocator will start making blocks at 1/8 the max size
    // and builds up block size as needed before capping at the max set here.
    mPreferredLargeHeapBlockSize = 4 * 1024 * 1024;

    // Create VMA allocator
    ANGLE_VK_TRY(displayVk,
                 mAllocator.init(mPhysicalDevice, mDevice, mInstance, mApplicationInfo.apiVersion,
                                 mPreferredLargeHeapBlockSize));

    // Figure out the alignment for default buffer allocations
    VkBufferCreateInfo createInfo    = {};
    createInfo.sType                 = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    createInfo.flags                 = 0;
    createInfo.size                  = 4096;
    createInfo.usage                 = GetDefaultBufferUsageFlags(this);
    createInfo.sharingMode           = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.queueFamilyIndexCount = 0;
    createInfo.pQueueFamilyIndices   = nullptr;

    vk::DeviceScoped<vk::Buffer> tempBuffer(mDevice);
    tempBuffer.get().init(mDevice, createInfo);

    VkMemoryRequirements defaultBufferMemoryRequirements;
    tempBuffer.get().getMemoryRequirements(mDevice, &defaultBufferMemoryRequirements);
    ASSERT(gl::isPow2(defaultBufferMemoryRequirements.alignment));

    const VkPhysicalDeviceLimits &limitsVk = getPhysicalDeviceProperties().limits;
    ASSERT(gl::isPow2(limitsVk.minUniformBufferOffsetAlignment));
    ASSERT(gl::isPow2(limitsVk.minStorageBufferOffsetAlignment));
    ASSERT(gl::isPow2(limitsVk.minTexelBufferOffsetAlignment));
    ASSERT(gl::isPow2(limitsVk.minMemoryMapAlignment));

    mDefaultBufferAlignment =
        std::max({static_cast<size_t>(limitsVk.minUniformBufferOffsetAlignment),
                  static_cast<size_t>(limitsVk.minStorageBufferOffsetAlignment),
                  static_cast<size_t>(limitsVk.minTexelBufferOffsetAlignment),
                  static_cast<size_t>(limitsVk.minMemoryMapAlignment),
                  static_cast<size_t>(defaultBufferMemoryRequirements.alignment)});

    // Initialize staging buffer memory type index and alignment.
    // These buffers will only be used as transfer sources or transfer targets.
    createInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    VkMemoryPropertyFlags requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    bool persistentlyMapped             = mFeatures.persistentlyMappedBuffers.enabled;

    // Uncached coherent staging buffer
    VkMemoryPropertyFlags preferredFlags = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    ANGLE_VK_TRY(displayVk, mAllocator.findMemoryTypeIndexForBufferInfo(
                                createInfo, requiredFlags, preferredFlags, persistentlyMapped,
                                &mCoherentStagingBufferMemoryTypeIndex));
    ASSERT(mCoherentStagingBufferMemoryTypeIndex != kInvalidMemoryTypeIndex);

    // Cached (b/219974369) Non-coherent staging buffer
    preferredFlags = VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
    ANGLE_VK_TRY(displayVk, mAllocator.findMemoryTypeIndexForBufferInfo(
                                createInfo, requiredFlags, preferredFlags, persistentlyMapped,
                                &mNonCoherentStagingBufferMemoryTypeIndex));
    ASSERT(mNonCoherentStagingBufferMemoryTypeIndex != kInvalidMemoryTypeIndex);

    // Alignment
    mStagingBufferAlignment =
        static_cast<size_t>(mPhysicalDeviceProperties.limits.minMemoryMapAlignment);
    ASSERT(gl::isPow2(mPhysicalDeviceProperties.limits.nonCoherentAtomSize));
    ASSERT(gl::isPow2(mPhysicalDeviceProperties.limits.optimalBufferCopyOffsetAlignment));
    // Usually minTexelBufferOffsetAlignment is much smaller than  nonCoherentAtomSize
    ASSERT(gl::isPow2(mPhysicalDeviceProperties.limits.minTexelBufferOffsetAlignment));
    mStagingBufferAlignment = std::max(
        {mStagingBufferAlignment,
         static_cast<size_t>(mPhysicalDeviceProperties.limits.optimalBufferCopyOffsetAlignment),
         static_cast<size_t>(mPhysicalDeviceProperties.limits.nonCoherentAtomSize),
         static_cast<size_t>(mPhysicalDeviceProperties.limits.minTexelBufferOffsetAlignment)});
    ASSERT(gl::isPow2(mStagingBufferAlignment));

    // Device local vertex conversion buffer
    createInfo.usage = vk::kVertexBufferUsageFlags;
    requiredFlags    = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    preferredFlags   = 0;
    ANGLE_VK_TRY(displayVk, mAllocator.findMemoryTypeIndexForBufferInfo(
                                createInfo, requiredFlags, preferredFlags, persistentlyMapped,
                                &mDeviceLocalVertexConversionBufferMemoryTypeIndex));
    ASSERT(mDeviceLocalVertexConversionBufferMemoryTypeIndex != kInvalidMemoryTypeIndex);

    // Host visible and non-coherent vertex conversion buffer, which is the same as non-coherent
    // staging buffer
    mHostVisibleVertexConversionBufferMemoryTypeIndex = mNonCoherentStagingBufferMemoryTypeIndex;

    // We may use compute shader to do conversion, so we must meet
    // minStorageBufferOffsetAlignment requirement as well. Also take into account non-coherent
    // alignment requirements.
    mVertexConversionBufferAlignment = std::max(
        {vk::kVertexBufferAlignment,
         static_cast<size_t>(mPhysicalDeviceProperties.limits.minStorageBufferOffsetAlignment),
         static_cast<size_t>(mPhysicalDeviceProperties.limits.nonCoherentAtomSize),
         static_cast<size_t>(defaultBufferMemoryRequirements.alignment)});
    ASSERT(gl::isPow2(mVertexConversionBufferAlignment));

    return angle::Result::Continue;
}

void RendererVk::queryDeviceExtensionFeatures(const vk::ExtensionNameList &deviceExtensionNames)
{
    // Default initialize all extension features to false.
    mLineRasterizationFeatures = {};
    mLineRasterizationFeatures.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_LINE_RASTERIZATION_FEATURES_EXT;

    mProvokingVertexFeatures = {};
    mProvokingVertexFeatures.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROVOKING_VERTEX_FEATURES_EXT;

    mVertexAttributeDivisorFeatures = {};
    mVertexAttributeDivisorFeatures.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VERTEX_ATTRIBUTE_DIVISOR_FEATURES_EXT;

    mVertexAttributeDivisorProperties = {};
    mVertexAttributeDivisorProperties.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VERTEX_ATTRIBUTE_DIVISOR_PROPERTIES_EXT;

    mTransformFeedbackFeatures = {};
    mTransformFeedbackFeatures.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TRANSFORM_FEEDBACK_FEATURES_EXT;

    mIndexTypeUint8Features       = {};
    mIndexTypeUint8Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_INDEX_TYPE_UINT8_FEATURES_EXT;

    mSubgroupProperties       = {};
    mSubgroupProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES;

    mSubgroupExtendedTypesFeatures = {};
    mSubgroupExtendedTypesFeatures.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_SUBGROUP_EXTENDED_TYPES_FEATURES;

    mMemoryReportFeatures = {};
    mMemoryReportFeatures.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEVICE_MEMORY_REPORT_FEATURES_EXT;

    mExternalMemoryHostProperties = {};
    mExternalMemoryHostProperties.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_MEMORY_HOST_PROPERTIES_EXT;

    mShaderFloat16Int8Features = {};
    mShaderFloat16Int8Features.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_FLOAT16_INT8_FEATURES;

    mDepthStencilResolveProperties = {};
    mDepthStencilResolveProperties.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEPTH_STENCIL_RESOLVE_PROPERTIES;

    mCustomBorderColorFeatures = {};
    mCustomBorderColorFeatures.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CUSTOM_BORDER_COLOR_FEATURES_EXT;

    mMultisampledRenderToSingleSampledFeatures = {};
    mMultisampledRenderToSingleSampledFeatures.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTISAMPLED_RENDER_TO_SINGLE_SAMPLED_FEATURES_EXT;

    mMultisampledRenderToSingleSampledFeaturesGOOGLEX = {};
    mMultisampledRenderToSingleSampledFeaturesGOOGLEX.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTISAMPLED_RENDER_TO_SINGLE_SAMPLED_FEATURES_GOOGLEX;

    mImage2dViewOf3dFeatures = {};
    mImage2dViewOf3dFeatures.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_2D_VIEW_OF_3D_FEATURES_EXT;

    mMultiviewFeatures       = {};
    mMultiviewFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_FEATURES;

    mMultiviewProperties       = {};
    mMultiviewProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_PROPERTIES;

    mDriverProperties       = {};
    mDriverProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES;

    mSamplerYcbcrConversionFeatures = {};
    mSamplerYcbcrConversionFeatures.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLER_YCBCR_CONVERSION_FEATURES;

    mProtectedMemoryFeatures       = {};
    mProtectedMemoryFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROTECTED_MEMORY_FEATURES;

    mProtectedMemoryProperties = {};
    mProtectedMemoryProperties.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROTECTED_MEMORY_PROPERTIES;

    mHostQueryResetFeatures       = {};
    mHostQueryResetFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_HOST_QUERY_RESET_FEATURES_EXT;

    mDepthClipControlFeatures = {};
    mDepthClipControlFeatures.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEPTH_CLIP_CONTROL_FEATURES_EXT;

    mPrimitivesGeneratedQueryFeatures = {};
    mPrimitivesGeneratedQueryFeatures.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRIMITIVES_GENERATED_QUERY_FEATURES_EXT;

    mPrimitiveTopologyListRestartFeatures = {};
    mPrimitiveTopologyListRestartFeatures.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRIMITIVE_TOPOLOGY_LIST_RESTART_FEATURES_EXT;

    mBlendOperationAdvancedFeatures = {};
    mBlendOperationAdvancedFeatures.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BLEND_OPERATION_ADVANCED_FEATURES_EXT;

    mPipelineCreationCacheControlFeatures = {};
    mPipelineCreationCacheControlFeatures.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PIPELINE_CREATION_CACHE_CONTROL_FEATURES_EXT;

    mExtendedDynamicStateFeatures = {};
    mExtendedDynamicStateFeatures.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_FEATURES_EXT;

    mExtendedDynamicState2Features = {};
    mExtendedDynamicState2Features.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_2_FEATURES_EXT;

    mGraphicsPipelineLibraryFeatures = {};
    mGraphicsPipelineLibraryFeatures.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_GRAPHICS_PIPELINE_LIBRARY_FEATURES_EXT;

    mGraphicsPipelineLibraryProperties = {};
    mGraphicsPipelineLibraryProperties.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_GRAPHICS_PIPELINE_LIBRARY_PROPERTIES_EXT;

    mFragmentShadingRateFeatures = {};
    mFragmentShadingRateFeatures.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_FEATURES_KHR;

    mFragmentShaderInterlockFeatures = {};
    mFragmentShaderInterlockFeatures.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADER_INTERLOCK_FEATURES_EXT;

    mImagelessFramebufferFeatures = {};
    mImagelessFramebufferFeatures.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGELESS_FRAMEBUFFER_FEATURES_KHR;

    mPipelineRobustnessFeatures = {};
    mPipelineRobustnessFeatures.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PIPELINE_ROBUSTNESS_FEATURES_EXT;

    mPipelineProtectedAccessFeatures = {};
    mPipelineProtectedAccessFeatures.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PIPELINE_PROTECTED_ACCESS_FEATURES_EXT;

    mRasterizationOrderAttachmentAccessFeatures = {};
    mRasterizationOrderAttachmentAccessFeatures.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RASTERIZATION_ORDER_ATTACHMENT_ACCESS_FEATURES_EXT;

    mSwapchainMaintenance1FeaturesEXT = {};
    mSwapchainMaintenance1FeaturesEXT.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SWAPCHAIN_MAINTENANCE_1_FEATURES_EXT;

    mDrmProperties       = {};
    mDrmProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRM_PROPERTIES_EXT;

    if (!vkGetPhysicalDeviceProperties2KHR || !vkGetPhysicalDeviceFeatures2KHR)
    {
        return;
    }

    // Query features and properties.
    VkPhysicalDeviceFeatures2KHR deviceFeatures = {};
    deviceFeatures.sType                        = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;

    VkPhysicalDeviceProperties2 deviceProperties = {};
    deviceProperties.sType                       = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;

    // Query line rasterization features
    if (ExtensionFound(VK_EXT_LINE_RASTERIZATION_EXTENSION_NAME, deviceExtensionNames))
    {
        vk::AddToPNextChain(&deviceFeatures, &mLineRasterizationFeatures);
    }

    // Query provoking vertex features
    if (ExtensionFound(VK_EXT_PROVOKING_VERTEX_EXTENSION_NAME, deviceExtensionNames))
    {
        vk::AddToPNextChain(&deviceFeatures, &mProvokingVertexFeatures);
    }

    // Query attribute divisor features and properties
    if (ExtensionFound(VK_EXT_VERTEX_ATTRIBUTE_DIVISOR_EXTENSION_NAME, deviceExtensionNames))
    {
        vk::AddToPNextChain(&deviceFeatures, &mVertexAttributeDivisorFeatures);
        vk::AddToPNextChain(&deviceProperties, &mVertexAttributeDivisorProperties);
    }

    // Query transform feedback features
    if (ExtensionFound(VK_EXT_TRANSFORM_FEEDBACK_EXTENSION_NAME, deviceExtensionNames))
    {
        vk::AddToPNextChain(&deviceFeatures, &mTransformFeedbackFeatures);
    }

    // Query uint8 index type features
    if (ExtensionFound(VK_EXT_INDEX_TYPE_UINT8_EXTENSION_NAME, deviceExtensionNames))
    {
        vk::AddToPNextChain(&deviceFeatures, &mIndexTypeUint8Features);
    }

    // Query memory report features
    if (ExtensionFound(VK_EXT_DEVICE_MEMORY_REPORT_EXTENSION_NAME, deviceExtensionNames))
    {
        vk::AddToPNextChain(&deviceFeatures, &mMemoryReportFeatures);
    }

    // Query external memory host properties
    if (ExtensionFound(VK_EXT_EXTERNAL_MEMORY_HOST_EXTENSION_NAME, deviceExtensionNames))
    {
        vk::AddToPNextChain(&deviceProperties, &mExternalMemoryHostProperties);
    }

    // Query Ycbcr conversion properties
    if (ExtensionFound(VK_KHR_SAMPLER_YCBCR_CONVERSION_EXTENSION_NAME, deviceExtensionNames))
    {
        vk::AddToPNextChain(&deviceFeatures, &mSamplerYcbcrConversionFeatures);
    }

    // Query float16/int8 features
    if (ExtensionFound(VK_KHR_SHADER_FLOAT16_INT8_EXTENSION_NAME, deviceExtensionNames))
    {
        vk::AddToPNextChain(&deviceFeatures, &mShaderFloat16Int8Features);
    }

    // Query depth/stencil resolve properties
    if (ExtensionFound(VK_KHR_DEPTH_STENCIL_RESOLVE_EXTENSION_NAME, deviceExtensionNames))
    {
        vk::AddToPNextChain(&deviceProperties, &mDepthStencilResolveProperties);
    }

    // Query multisampled render to single-sampled features
    if (ExtensionFound(VK_EXT_MULTISAMPLED_RENDER_TO_SINGLE_SAMPLED_EXTENSION_NAME,
                       deviceExtensionNames))
    {
        vk::AddToPNextChain(&deviceFeatures, &mMultisampledRenderToSingleSampledFeatures);
    }
    else if (ExtensionFound(VK_GOOGLEX_MULTISAMPLED_RENDER_TO_SINGLE_SAMPLED_EXTENSION_NAME,
                            deviceExtensionNames))
    {
        vk::AddToPNextChain(&deviceFeatures, &mMultisampledRenderToSingleSampledFeaturesGOOGLEX);
    }

    if (ExtensionFound(VK_EXT_IMAGE_2D_VIEW_OF_3D_EXTENSION_NAME, deviceExtensionNames))
    {
        vk::AddToPNextChain(&deviceFeatures, &mImage2dViewOf3dFeatures);
    }

    // Query multiview features and properties
    if (ExtensionFound(VK_KHR_MULTIVIEW_EXTENSION_NAME, deviceExtensionNames))
    {
        vk::AddToPNextChain(&deviceFeatures, &mMultiviewFeatures);
        vk::AddToPNextChain(&deviceProperties, &mMultiviewProperties);
    }

    // Query driver properties
    if (ExtensionFound(VK_KHR_DRIVER_PROPERTIES_EXTENSION_NAME, deviceExtensionNames))
    {
        vk::AddToPNextChain(&deviceProperties, &mDriverProperties);
    }

    // Query custom border color features
    if (ExtensionFound(VK_EXT_CUSTOM_BORDER_COLOR_EXTENSION_NAME, deviceExtensionNames))
    {
        vk::AddToPNextChain(&deviceFeatures, &mCustomBorderColorFeatures);
    }

    // Query subgroup properties
    vk::AddToPNextChain(&deviceProperties, &mSubgroupProperties);

    // Query subgroup extended types features
    if (ExtensionFound(VK_KHR_SHADER_SUBGROUP_EXTENDED_TYPES_EXTENSION_NAME, deviceExtensionNames))
    {
        vk::AddToPNextChain(&deviceFeatures, &mSubgroupExtendedTypesFeatures);
    }

    // Query protected memory features and properties
    if (mPhysicalDeviceProperties.apiVersion >= VK_MAKE_VERSION(1, 1, 0))
    {
        vk::AddToPNextChain(&deviceFeatures, &mProtectedMemoryFeatures);
        vk::AddToPNextChain(&deviceProperties, &mProtectedMemoryProperties);
    }

    // Query host query reset features
    if (ExtensionFound(VK_EXT_HOST_QUERY_RESET_EXTENSION_NAME, deviceExtensionNames) ||
        mPhysicalDeviceProperties.apiVersion >= VK_MAKE_VERSION(1, 2, 0))
    {
        vk::AddToPNextChain(&deviceFeatures, &mHostQueryResetFeatures);
    }

    if (ExtensionFound(VK_EXT_DEPTH_CLIP_CONTROL_EXTENSION_NAME, deviceExtensionNames))
    {
        vk::AddToPNextChain(&deviceFeatures, &mDepthClipControlFeatures);
    }

    if (ExtensionFound(VK_EXT_PRIMITIVES_GENERATED_QUERY_EXTENSION_NAME, deviceExtensionNames))
    {
        vk::AddToPNextChain(&deviceFeatures, &mPrimitivesGeneratedQueryFeatures);
    }

    if (ExtensionFound(VK_EXT_PRIMITIVE_TOPOLOGY_LIST_RESTART_EXTENSION_NAME, deviceExtensionNames))
    {
        vk::AddToPNextChain(&deviceFeatures, &mPrimitiveTopologyListRestartFeatures);
    }

    if (ExtensionFound(VK_EXT_BLEND_OPERATION_ADVANCED_EXTENSION_NAME, deviceExtensionNames))
    {
        vk::AddToPNextChain(&deviceFeatures, &mBlendOperationAdvancedFeatures);
    }

    if (ExtensionFound(VK_EXT_PIPELINE_CREATION_CACHE_CONTROL_EXTENSION_NAME, deviceExtensionNames))
    {
        vk::AddToPNextChain(&deviceFeatures, &mPipelineCreationCacheControlFeatures);
    }

    if (ExtensionFound(VK_EXT_EXTENDED_DYNAMIC_STATE_EXTENSION_NAME, deviceExtensionNames))
    {
        vk::AddToPNextChain(&deviceFeatures, &mExtendedDynamicStateFeatures);
    }

    if (ExtensionFound(VK_EXT_EXTENDED_DYNAMIC_STATE_2_EXTENSION_NAME, deviceExtensionNames))
    {
        vk::AddToPNextChain(&deviceFeatures, &mExtendedDynamicState2Features);
    }

    if (ExtensionFound(VK_EXT_GRAPHICS_PIPELINE_LIBRARY_EXTENSION_NAME, deviceExtensionNames))
    {
        vk::AddToPNextChain(&deviceFeatures, &mGraphicsPipelineLibraryFeatures);
        vk::AddToPNextChain(&deviceProperties, &mGraphicsPipelineLibraryProperties);
    }

    if (ExtensionFound(VK_KHR_FRAGMENT_SHADING_RATE_EXTENSION_NAME, deviceExtensionNames))
    {
        vk::AddToPNextChain(&deviceFeatures, &mFragmentShadingRateFeatures);
    }

    if (ExtensionFound(VK_EXT_FRAGMENT_SHADER_INTERLOCK_EXTENSION_NAME, deviceExtensionNames))
    {
        vk::AddToPNextChain(&deviceFeatures, &mFragmentShaderInterlockFeatures);
    }

    if (ExtensionFound(VK_KHR_IMAGELESS_FRAMEBUFFER_EXTENSION_NAME, deviceExtensionNames))
    {
        vk::AddToPNextChain(&deviceFeatures, &mImagelessFramebufferFeatures);
    }

    if (ExtensionFound(VK_EXT_PIPELINE_ROBUSTNESS_EXTENSION_NAME, deviceExtensionNames))
    {
        vk::AddToPNextChain(&deviceFeatures, &mPipelineRobustnessFeatures);
    }

    if (ExtensionFound(VK_EXT_PIPELINE_PROTECTED_ACCESS_EXTENSION_NAME, deviceExtensionNames))
    {
        vk::AddToPNextChain(&deviceFeatures, &mPipelineProtectedAccessFeatures);
    }

    // The EXT and ARM versions are interchangeable. The structs and enums alias each other.
    if (ExtensionFound(VK_EXT_RASTERIZATION_ORDER_ATTACHMENT_ACCESS_EXTENSION_NAME,
                       deviceExtensionNames))
    {
        vk::AddToPNextChain(&deviceFeatures, &mRasterizationOrderAttachmentAccessFeatures);
    }
    else if (ExtensionFound(VK_ARM_RASTERIZATION_ORDER_ATTACHMENT_ACCESS_EXTENSION_NAME,
                            deviceExtensionNames))
    {
        vk::AddToPNextChain(&deviceFeatures, &mRasterizationOrderAttachmentAccessFeatures);
    }

    if (ExtensionFound(VK_EXT_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME, deviceExtensionNames))
    {
        vk::AddToPNextChain(&deviceFeatures, &mSwapchainMaintenance1FeaturesEXT);
    }

    if (ExtensionFound(VK_EXT_PHYSICAL_DEVICE_DRM_EXTENSION_NAME, deviceExtensionNames))
    {
        vk::AddToPNextChain(&deviceProperties, &mDrmProperties);
    }

    vkGetPhysicalDeviceFeatures2KHR(mPhysicalDevice, &deviceFeatures);
    vkGetPhysicalDeviceProperties2KHR(mPhysicalDevice, &deviceProperties);

    // Clean up pNext chains
    mLineRasterizationFeatures.pNext                        = nullptr;
    mMemoryReportFeatures.pNext                             = nullptr;
    mProvokingVertexFeatures.pNext                          = nullptr;
    mVertexAttributeDivisorFeatures.pNext                   = nullptr;
    mVertexAttributeDivisorProperties.pNext                 = nullptr;
    mTransformFeedbackFeatures.pNext                        = nullptr;
    mIndexTypeUint8Features.pNext                           = nullptr;
    mSubgroupProperties.pNext                               = nullptr;
    mSubgroupExtendedTypesFeatures.pNext                    = nullptr;
    mExternalMemoryHostProperties.pNext                     = nullptr;
    mCustomBorderColorFeatures.pNext                        = nullptr;
    mShaderFloat16Int8Features.pNext                        = nullptr;
    mDepthStencilResolveProperties.pNext                    = nullptr;
    mMultisampledRenderToSingleSampledFeatures.pNext        = nullptr;
    mMultisampledRenderToSingleSampledFeaturesGOOGLEX.pNext = nullptr;
    mImage2dViewOf3dFeatures.pNext                          = nullptr;
    mMultiviewFeatures.pNext                                = nullptr;
    mMultiviewProperties.pNext                              = nullptr;
    mDriverProperties.pNext                                 = nullptr;
    mSamplerYcbcrConversionFeatures.pNext                   = nullptr;
    mProtectedMemoryFeatures.pNext                          = nullptr;
    mProtectedMemoryProperties.pNext                        = nullptr;
    mHostQueryResetFeatures.pNext                           = nullptr;
    mDepthClipControlFeatures.pNext                         = nullptr;
    mPrimitivesGeneratedQueryFeatures.pNext                 = nullptr;
    mPrimitiveTopologyListRestartFeatures.pNext             = nullptr;
    mBlendOperationAdvancedFeatures.pNext                   = nullptr;
    mPipelineCreationCacheControlFeatures.pNext             = nullptr;
    mExtendedDynamicStateFeatures.pNext                     = nullptr;
    mExtendedDynamicState2Features.pNext                    = nullptr;
    mGraphicsPipelineLibraryFeatures.pNext                  = nullptr;
    mGraphicsPipelineLibraryProperties.pNext                = nullptr;
    mFragmentShadingRateFeatures.pNext                      = nullptr;
    mFragmentShaderInterlockFeatures.pNext                  = nullptr;
    mImagelessFramebufferFeatures.pNext                     = nullptr;
    mPipelineRobustnessFeatures.pNext                       = nullptr;
    mPipelineProtectedAccessFeatures.pNext                  = nullptr;
    mRasterizationOrderAttachmentAccessFeatures.pNext       = nullptr;
    mSwapchainMaintenance1FeaturesEXT.pNext                 = nullptr;
    mDrmProperties.pNext                                    = nullptr;
}

angle::Result RendererVk::initializeDevice(DisplayVk *displayVk, uint32_t queueFamilyIndex)
{
    uint32_t deviceLayerCount = 0;
    ANGLE_VK_TRY(displayVk,
                 vkEnumerateDeviceLayerProperties(mPhysicalDevice, &deviceLayerCount, nullptr));

    std::vector<VkLayerProperties> deviceLayerProps(deviceLayerCount);
    ANGLE_VK_TRY(displayVk, vkEnumerateDeviceLayerProperties(mPhysicalDevice, &deviceLayerCount,
                                                             deviceLayerProps.data()));

    VulkanLayerVector enabledDeviceLayerNames;
    if (mEnableValidationLayers)
    {
        mEnableValidationLayers =
            GetAvailableValidationLayers(deviceLayerProps, false, &enabledDeviceLayerNames);
    }

    const char *wsiLayer = displayVk->getWSILayer();
    if (wsiLayer)
    {
        enabledDeviceLayerNames.push_back(wsiLayer);
    }

    // Enumerate device extensions that are provided by the vulkan
    // implementation and implicit layers.
    uint32_t deviceExtensionCount = 0;
    ANGLE_VK_TRY(displayVk, vkEnumerateDeviceExtensionProperties(mPhysicalDevice, nullptr,
                                                                 &deviceExtensionCount, nullptr));

    // Work-around a race condition in the Android platform during Android start-up, that can cause
    // the second call to vkEnumerateDeviceExtensionProperties to have an additional extension.  In
    // that case, the second call will return VK_INCOMPLETE.  To work-around that, add 1 to
    // deviceExtensionCount and ask for one more extension property than the first call said there
    // were.  See: http://anglebug.com/6715 and internal-to-Google bug: b/206733351.
    deviceExtensionCount++;
    std::vector<VkExtensionProperties> deviceExtensionProps(deviceExtensionCount);
    ANGLE_VK_TRY(displayVk,
                 vkEnumerateDeviceExtensionProperties(
                     mPhysicalDevice, nullptr, &deviceExtensionCount, deviceExtensionProps.data()));
    // In case fewer items were returned than requested, resize deviceExtensionProps to the number
    // of extensions returned (i.e. deviceExtensionCount).  See: b/208937840
    deviceExtensionProps.resize(deviceExtensionCount);

    // Enumerate device extensions that are provided by explicit layers.
    for (const char *layerName : enabledDeviceLayerNames)
    {
        uint32_t previousExtensionCount    = static_cast<uint32_t>(deviceExtensionProps.size());
        uint32_t deviceLayerExtensionCount = 0;
        ANGLE_VK_TRY(displayVk,
                     vkEnumerateDeviceExtensionProperties(mPhysicalDevice, layerName,
                                                          &deviceLayerExtensionCount, nullptr));
        deviceExtensionProps.resize(previousExtensionCount + deviceLayerExtensionCount);
        ANGLE_VK_TRY(displayVk, vkEnumerateDeviceExtensionProperties(
                                    mPhysicalDevice, layerName, &deviceLayerExtensionCount,
                                    deviceExtensionProps.data() + previousExtensionCount));
        // In case fewer items were returned than requested, resize deviceExtensionProps to the
        // number of extensions returned (i.e. deviceLayerExtensionCount).
        deviceExtensionProps.resize(previousExtensionCount + deviceLayerExtensionCount);
    }

    vk::ExtensionNameList deviceExtensionNames;
    if (!deviceExtensionProps.empty())
    {
        ASSERT(deviceExtensionNames.size() <= deviceExtensionProps.size());
        for (const VkExtensionProperties &prop : deviceExtensionProps)
        {
            deviceExtensionNames.push_back(prop.extensionName);
        }
        std::sort(deviceExtensionNames.begin(), deviceExtensionNames.end(), StrLess);
    }

    if (displayVk->isUsingSwapchain())
    {
        mEnabledDeviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    }

    // Query extensions and their features.
    queryDeviceExtensionFeatures(deviceExtensionNames);

    // Initialize features and workarounds.
    initFeatures(displayVk, deviceExtensionNames);

    // App based feature overrides.
    appBasedFeatureOverrides(displayVk, deviceExtensionNames);

    // Enable VK_KHR_shared_presentable_image
    if (ExtensionFound(VK_KHR_SHARED_PRESENTABLE_IMAGE_EXTENSION_NAME, deviceExtensionNames))
    {
        mEnabledDeviceExtensions.push_back(VK_KHR_SHARED_PRESENTABLE_IMAGE_EXTENSION_NAME);
        ANGLE_FEATURE_CONDITION(&mFeatures, supportsSharedPresentableImageExtension, true);
    }

    // Enable VK_EXT_depth_clip_enable, if supported
    if (ExtensionFound(VK_EXT_DEPTH_CLIP_ENABLE_EXTENSION_NAME, deviceExtensionNames))
    {
        mEnabledDeviceExtensions.push_back(VK_EXT_DEPTH_CLIP_ENABLE_EXTENSION_NAME);
    }

    // Enable VK_KHR_get_memory_requirements2, if supported
    bool hasGetMemoryRequirements2KHR = false;
    if (ExtensionFound(VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME, deviceExtensionNames))
    {
        mEnabledDeviceExtensions.push_back(VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME);
        hasGetMemoryRequirements2KHR = true;
    }

    // Enable VK_EXT_MEMORY_BUDGET_EXTENSION_NAME, if supported
    if (ExtensionFound(VK_EXT_MEMORY_BUDGET_EXTENSION_NAME, deviceExtensionNames))
    {
        mEnabledDeviceExtensions.push_back(VK_EXT_MEMORY_BUDGET_EXTENSION_NAME);
    }

    if (ExtensionFound(VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME, deviceExtensionNames))
    {
        mEnabledDeviceExtensions.push_back(VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME);
    }

    // Enable VK_KHR_bind_memory2, if supported
    bool hasBindMemory2KHR = false;
    if (ExtensionFound(VK_KHR_BIND_MEMORY_2_EXTENSION_NAME, deviceExtensionNames))
    {
        hasBindMemory2KHR = true;
        mEnabledDeviceExtensions.push_back(VK_KHR_BIND_MEMORY_2_EXTENSION_NAME);
    }

    // Enable KHR_MAINTENANCE1 to support viewport flipping.
    if (mPhysicalDeviceProperties.apiVersion < VK_MAKE_VERSION(1, 1, 0))
    {
        mEnabledDeviceExtensions.push_back(VK_KHR_MAINTENANCE1_EXTENSION_NAME);
    }

    if (getFeatures().supportsRenderpass2.enabled)
    {
        mEnabledDeviceExtensions.push_back(VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME);
    }

    if (getFeatures().supportsIncrementalPresent.enabled)
    {
        mEnabledDeviceExtensions.push_back(VK_KHR_INCREMENTAL_PRESENT_EXTENSION_NAME);
    }

#if defined(ANGLE_PLATFORM_ANDROID)
    if (getFeatures().supportsAndroidHardwareBuffer.enabled)
    {
        mEnabledDeviceExtensions.push_back(VK_EXT_QUEUE_FAMILY_FOREIGN_EXTENSION_NAME);
        mEnabledDeviceExtensions.push_back(
            VK_ANDROID_EXTERNAL_MEMORY_ANDROID_HARDWARE_BUFFER_EXTENSION_NAME);
#    if !defined(ANGLE_SHARED_LIBVULKAN)
        InitExternalMemoryHardwareBufferANDROIDFunctions(mInstance);
#    endif  // !defined(ANGLE_SHARED_LIBVULKAN)
    }
#else
    ASSERT(!getFeatures().supportsAndroidHardwareBuffer.enabled);
#endif

#if defined(ANGLE_PLATFORM_GGP)
    if (getFeatures().supportsGGPFrameToken.enabled)
    {
        mEnabledDeviceExtensions.push_back(VK_GGP_FRAME_TOKEN_EXTENSION_NAME);
    }
    ANGLE_VK_CHECK(displayVk, getFeatures().supportsGGPFrameToken.enabled,
                   VK_ERROR_EXTENSION_NOT_PRESENT);
#else
    ASSERT(!getFeatures().supportsGGPFrameToken.enabled);
#endif

    if (getFeatures().supportsAndroidHardwareBuffer.enabled ||
        getFeatures().supportsExternalMemoryFd.enabled ||
        getFeatures().supportsExternalMemoryFuchsia.enabled)
    {
        mEnabledDeviceExtensions.push_back(VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME);
    }

    if (getFeatures().supportsExternalMemoryFd.enabled)
    {
        mEnabledDeviceExtensions.push_back(VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME);
    }

    if (getFeatures().supportsExternalMemoryFuchsia.enabled)
    {
        mEnabledDeviceExtensions.push_back(VK_FUCHSIA_EXTERNAL_MEMORY_EXTENSION_NAME);
    }

    if (getFeatures().supportsExternalSemaphoreFd.enabled ||
        getFeatures().supportsExternalSemaphoreFuchsia.enabled)
    {
        mEnabledDeviceExtensions.push_back(VK_KHR_EXTERNAL_SEMAPHORE_EXTENSION_NAME);
#if !defined(ANGLE_SHARED_LIBVULKAN)
        InitExternalSemaphoreFdFunctions(mInstance);
#endif  // !defined(ANGLE_SHARED_LIBVULKAN)
    }

    if (getFeatures().supportsExternalSemaphoreFd.enabled)
    {
        mEnabledDeviceExtensions.push_back(VK_KHR_EXTERNAL_SEMAPHORE_FD_EXTENSION_NAME);
    }

    if (getFeatures().supportsExternalSemaphoreCapabilities.enabled)
    {
#if !defined(ANGLE_SHARED_LIBVULKAN)
        InitExternalSemaphoreCapabilitiesFunctions(mInstance);
#endif  // !defined(ANGLE_SHARED_LIBVULKAN)
    }

    if (getFeatures().supportsExternalFenceFd.enabled)
    {
        mEnabledDeviceExtensions.push_back(VK_KHR_EXTERNAL_FENCE_FD_EXTENSION_NAME);
#if !defined(ANGLE_SHARED_LIBVULKAN)
        InitExternalFenceFdFunctions(mInstance);
#endif  // !defined(ANGLE_SHARED_LIBVULKAN)
    }

    if (getFeatures().supportsExternalFenceCapabilities.enabled)
    {
#if !defined(ANGLE_SHARED_LIBVULKAN)
        InitExternalFenceCapabilitiesFunctions(mInstance);
#endif  // !defined(ANGLE_SHARED_LIBVULKAN)
    }

    if (getFeatures().supportsExternalSemaphoreFuchsia.enabled)
    {
        mEnabledDeviceExtensions.push_back(VK_FUCHSIA_EXTERNAL_SEMAPHORE_EXTENSION_NAME);
    }

    if (getFeatures().supportsShaderStencilExport.enabled)
    {
        mEnabledDeviceExtensions.push_back(VK_EXT_SHADER_STENCIL_EXPORT_EXTENSION_NAME);
    }

    if (getFeatures().supportsRenderPassLoadStoreOpNone.enabled)
    {
        mEnabledDeviceExtensions.push_back(VK_EXT_LOAD_STORE_OP_NONE_EXTENSION_NAME);
    }
    else if (getFeatures().supportsRenderPassStoreOpNone.enabled)
    {
        mEnabledDeviceExtensions.push_back(VK_QCOM_RENDER_PASS_STORE_OPS_EXTENSION_NAME);
    }

    if (getFeatures().supportsImageFormatList.enabled)
    {
        mEnabledDeviceExtensions.push_back(VK_KHR_IMAGE_FORMAT_LIST_EXTENSION_NAME);
    }

    if (getFeatures().supportsTimestampSurfaceAttribute.enabled)
    {
        mEnabledDeviceExtensions.push_back(VK_GOOGLE_DISPLAY_TIMING_EXTENSION_NAME);
    }

    std::sort(mEnabledDeviceExtensions.begin(), mEnabledDeviceExtensions.end(), StrLess);
    ANGLE_VK_TRY(displayVk,
                 VerifyExtensionsPresent(deviceExtensionNames, mEnabledDeviceExtensions));

    // Select additional features to be enabled.
    mEnabledFeatures       = {};
    mEnabledFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    // Used to support cubemap array:
    mEnabledFeatures.features.imageCubeArray = getFeatures().supportsImageCubeArray.enabled;
    // Used to support framebuffers with multiple attachments:
    mEnabledFeatures.features.independentBlend = mPhysicalDeviceFeatures.independentBlend;
    // Used to support multi_draw_indirect
    mEnabledFeatures.features.multiDrawIndirect = mPhysicalDeviceFeatures.multiDrawIndirect;
    // Used to support robust buffer access, if VK_EXT_pipeline_robustness is not supported.
    if (!getFeatures().supportsPipelineRobustness.enabled)
    {
        mEnabledFeatures.features.robustBufferAccess = mPhysicalDeviceFeatures.robustBufferAccess;
    }
    // Used to support Anisotropic filtering:
    mEnabledFeatures.features.samplerAnisotropy = mPhysicalDeviceFeatures.samplerAnisotropy;
    // Used to support wide lines:
    mEnabledFeatures.features.wideLines = mPhysicalDeviceFeatures.wideLines;
    // Used to emulate transform feedback:
    mEnabledFeatures.features.vertexPipelineStoresAndAtomics =
        mPhysicalDeviceFeatures.vertexPipelineStoresAndAtomics;
    // Used to implement storage buffers and images in the fragment shader:
    mEnabledFeatures.features.fragmentStoresAndAtomics =
        mPhysicalDeviceFeatures.fragmentStoresAndAtomics;
    // Used to emulate the primitives generated query:
    mEnabledFeatures.features.pipelineStatisticsQuery =
        !getFeatures().supportsPrimitivesGeneratedQuery.enabled &&
        getFeatures().supportsPipelineStatisticsQuery.enabled;
    // Used to support geometry shaders:
    mEnabledFeatures.features.geometryShader = mPhysicalDeviceFeatures.geometryShader;
    // Used to support EXT_gpu_shader5:
    mEnabledFeatures.features.shaderImageGatherExtended =
        mPhysicalDeviceFeatures.shaderImageGatherExtended;
    // Used to support EXT_gpu_shader5:
    mEnabledFeatures.features.shaderUniformBufferArrayDynamicIndexing =
        mPhysicalDeviceFeatures.shaderUniformBufferArrayDynamicIndexing;
    mEnabledFeatures.features.shaderSampledImageArrayDynamicIndexing =
        mPhysicalDeviceFeatures.shaderSampledImageArrayDynamicIndexing;
    // Used to support APPLE_clip_distance
    mEnabledFeatures.features.shaderClipDistance = mPhysicalDeviceFeatures.shaderClipDistance;
    // Used to support OES_sample_shading
    mEnabledFeatures.features.sampleRateShading = mPhysicalDeviceFeatures.sampleRateShading;
    // Used to support depth clears through draw calls.
    mEnabledFeatures.features.depthClamp = mPhysicalDeviceFeatures.depthClamp;
    // Used to support EXT_polygon_offset_clamp
    mEnabledFeatures.features.depthBiasClamp = mPhysicalDeviceFeatures.depthBiasClamp;
    // Used to support EXT_clip_cull_distance
    mEnabledFeatures.features.shaderCullDistance = mPhysicalDeviceFeatures.shaderCullDistance;
    // Used to support tessellation Shader:
    mEnabledFeatures.features.tessellationShader = mPhysicalDeviceFeatures.tessellationShader;
    // Used to support EXT_blend_func_extended
    mEnabledFeatures.features.dualSrcBlend = mPhysicalDeviceFeatures.dualSrcBlend;
    // Used to support ANGLE_logic_op and GLES1
    mEnabledFeatures.features.logicOp = mPhysicalDeviceFeatures.logicOp;
    // Used to support EXT_multisample_compatibility
    mEnabledFeatures.features.alphaToOne = mPhysicalDeviceFeatures.alphaToOne;

    if (!vk::OutsideRenderPassCommandBuffer::ExecutesInline() ||
        !vk::RenderPassCommandBuffer::ExecutesInline())
    {
        mEnabledFeatures.features.inheritedQueries = mPhysicalDeviceFeatures.inheritedQueries;
    }

    // Setup device initialization struct
    VkDeviceCreateInfo createInfo = {};

    // Based on available extension features, decide on which extensions and features to enable.

    if (mLineRasterizationFeatures.bresenhamLines)
    {
        mEnabledDeviceExtensions.push_back(VK_EXT_LINE_RASTERIZATION_EXTENSION_NAME);
        vk::AddToPNextChain(&mEnabledFeatures, &mLineRasterizationFeatures);
    }

    if (mProvokingVertexFeatures.provokingVertexLast)
    {
        mEnabledDeviceExtensions.push_back(VK_EXT_PROVOKING_VERTEX_EXTENSION_NAME);
        vk::AddToPNextChain(&mEnabledFeatures, &mProvokingVertexFeatures);
    }

    if (mVertexAttributeDivisorFeatures.vertexAttributeInstanceRateDivisor)
    {
        mEnabledDeviceExtensions.push_back(VK_EXT_VERTEX_ATTRIBUTE_DIVISOR_EXTENSION_NAME);
        vk::AddToPNextChain(&mEnabledFeatures, &mVertexAttributeDivisorFeatures);

        // We only store 8 bit divisor in GraphicsPipelineDesc so capping value & we emulate if
        // exceeded
        mMaxVertexAttribDivisor =
            std::min(mVertexAttributeDivisorProperties.maxVertexAttribDivisor,
                     static_cast<uint32_t>(std::numeric_limits<uint8_t>::max()));
    }

    // Enable VK_KHR_shader_subgroup_extended_types if needed
    if (getFeatures().allowGenerateMipmapWithCompute.enabled)
    {
        mEnabledDeviceExtensions.push_back(VK_KHR_SHADER_SUBGROUP_EXTENDED_TYPES_EXTENSION_NAME);
        vk::AddToPNextChain(&mEnabledFeatures, &mSubgroupExtendedTypesFeatures);
    }

    if (getFeatures().supportsTransformFeedbackExtension.enabled)
    {
        mEnabledDeviceExtensions.push_back(VK_EXT_TRANSFORM_FEEDBACK_EXTENSION_NAME);
        vk::AddToPNextChain(&mEnabledFeatures, &mTransformFeedbackFeatures);
    }

    if (getFeatures().supportsCustomBorderColor.enabled)
    {
        mEnabledDeviceExtensions.push_back(VK_EXT_CUSTOM_BORDER_COLOR_EXTENSION_NAME);
        vk::AddToPNextChain(&mEnabledFeatures, &mCustomBorderColorFeatures);
    }

    if (getFeatures().supportsIndexTypeUint8.enabled)
    {
        mEnabledDeviceExtensions.push_back(VK_EXT_INDEX_TYPE_UINT8_EXTENSION_NAME);
        vk::AddToPNextChain(&mEnabledFeatures, &mIndexTypeUint8Features);
    }

    if (getFeatures().supportsDepthStencilResolve.enabled)
    {
        mEnabledDeviceExtensions.push_back(VK_KHR_DEPTH_STENCIL_RESOLVE_EXTENSION_NAME);
    }

    if (getFeatures().supportsMultisampledRenderToSingleSampled.enabled)
    {
        mEnabledDeviceExtensions.push_back(
            VK_EXT_MULTISAMPLED_RENDER_TO_SINGLE_SAMPLED_EXTENSION_NAME);
        vk::AddToPNextChain(&mEnabledFeatures, &mMultisampledRenderToSingleSampledFeatures);
    }

    if (getFeatures().supportsMultisampledRenderToSingleSampledGOOGLEX.enabled)
    {
        ASSERT(!getFeatures().supportsMultisampledRenderToSingleSampled.enabled);
        mEnabledDeviceExtensions.push_back(
            VK_GOOGLEX_MULTISAMPLED_RENDER_TO_SINGLE_SAMPLED_EXTENSION_NAME);
        vk::AddToPNextChain(&mEnabledFeatures, &mMultisampledRenderToSingleSampledFeaturesGOOGLEX);
    }

    if (getFeatures().supportsMultiview.enabled)
    {
        // OVR_multiview disallows multiview with geometry and tessellation, so don't request these
        // features.
        mMultiviewFeatures.multiviewGeometryShader     = VK_FALSE;
        mMultiviewFeatures.multiviewTessellationShader = VK_FALSE;
        mEnabledDeviceExtensions.push_back(VK_KHR_MULTIVIEW_EXTENSION_NAME);
        vk::AddToPNextChain(&mEnabledFeatures, &mMultiviewFeatures);
    }

    if (getFeatures().logMemoryReportCallbacks.enabled ||
        getFeatures().logMemoryReportStats.enabled)
    {
        if (mMemoryReportFeatures.deviceMemoryReport)
        {
            mEnabledDeviceExtensions.push_back(VK_EXT_DEVICE_MEMORY_REPORT_EXTENSION_NAME);

            mMemoryReportCallback = {};
            mMemoryReportCallback.sType =
                VK_STRUCTURE_TYPE_DEVICE_DEVICE_MEMORY_REPORT_CREATE_INFO_EXT;
            mMemoryReportCallback.pfnUserCallback = &MemoryReportCallback;
            mMemoryReportCallback.pUserData       = this;
            vk::AddToPNextChain(&createInfo, &mMemoryReportCallback);
        }
        else
        {
            WARN() << "Disabling the following feature(s) because driver does not support "
                      "VK_EXT_device_memory_report extension:";
            if (getFeatures().logMemoryReportStats.enabled)
            {
                WARN() << "\tlogMemoryReportStats";
                ANGLE_FEATURE_CONDITION(&mFeatures, logMemoryReportStats, false);
            }
            if (getFeatures().logMemoryReportCallbacks.enabled)
            {
                WARN() << "\tlogMemoryReportCallbacks";
                ANGLE_FEATURE_CONDITION(&mFeatures, logMemoryReportCallbacks, false);
            }
        }
    }

    if (getFeatures().supportsExternalMemoryDmaBufAndModifiers.enabled)
    {
        mEnabledDeviceExtensions.push_back(VK_EXT_EXTERNAL_MEMORY_DMA_BUF_EXTENSION_NAME);
        mEnabledDeviceExtensions.push_back(VK_EXT_IMAGE_DRM_FORMAT_MODIFIER_EXTENSION_NAME);
    }

    if (getFeatures().supportsExternalMemoryHost.enabled)
    {
        mEnabledDeviceExtensions.push_back(VK_EXT_EXTERNAL_MEMORY_HOST_EXTENSION_NAME);
        mMinImportedHostPointerAlignment =
            mExternalMemoryHostProperties.minImportedHostPointerAlignment;
#if !defined(ANGLE_SHARED_LIBVULKAN)
        InitExternalMemoryHostFunctions(mInstance);
#endif  // !defined(ANGLE_SHARED_LIBVULKAN)
    }

    if (getFeatures().supportsYUVSamplerConversion.enabled)
    {
        mEnabledDeviceExtensions.push_back(VK_KHR_SAMPLER_YCBCR_CONVERSION_EXTENSION_NAME);
        vk::AddToPNextChain(&mEnabledFeatures, &mSamplerYcbcrConversionFeatures);
    }

    if (getFeatures().supportsShaderFloat16.enabled)
    {
        mEnabledDeviceExtensions.push_back(VK_KHR_SHADER_FLOAT16_INT8_EXTENSION_NAME);
        vk::AddToPNextChain(&mEnabledFeatures, &mShaderFloat16Int8Features);
    }

    if (getFeatures().supportsProtectedMemory.enabled)
    {
        vk::AddToPNextChain(&mEnabledFeatures, &mProtectedMemoryFeatures);
    }

    if (getFeatures().supportsHostQueryReset.enabled)
    {
        mEnabledDeviceExtensions.push_back(VK_EXT_HOST_QUERY_RESET_EXTENSION_NAME);
        vk::AddToPNextChain(&mEnabledFeatures, &mHostQueryResetFeatures);
    }

    if (getFeatures().supportsPipelineCreationCacheControl.enabled)
    {
        mEnabledDeviceExtensions.push_back(VK_EXT_PIPELINE_CREATION_CACHE_CONTROL_EXTENSION_NAME);
        vk::AddToPNextChain(&mEnabledFeatures, &mPipelineCreationCacheControlFeatures);
    }

    if (getFeatures().supportsPipelineCreationFeedback.enabled)
    {
        mEnabledDeviceExtensions.push_back(VK_EXT_PIPELINE_CREATION_FEEDBACK_EXTENSION_NAME);
    }

    if (getFeatures().supportsDepthClipControl.enabled)
    {
        mEnabledDeviceExtensions.push_back(VK_EXT_DEPTH_CLIP_CONTROL_EXTENSION_NAME);
        vk::AddToPNextChain(&mEnabledFeatures, &mDepthClipControlFeatures);
    }

    if (getFeatures().supportsPrimitivesGeneratedQuery.enabled)
    {
        mEnabledDeviceExtensions.push_back(VK_EXT_PRIMITIVES_GENERATED_QUERY_EXTENSION_NAME);
        vk::AddToPNextChain(&mEnabledFeatures, &mPrimitivesGeneratedQueryFeatures);
    }

    if (getFeatures().supportsPrimitiveTopologyListRestart.enabled)
    {
        mEnabledDeviceExtensions.push_back(VK_EXT_PRIMITIVE_TOPOLOGY_LIST_RESTART_EXTENSION_NAME);
        vk::AddToPNextChain(&mEnabledFeatures, &mPrimitiveTopologyListRestartFeatures);
    }

    if (getFeatures().supportsBlendOperationAdvanced.enabled)
    {
        mEnabledDeviceExtensions.push_back(VK_EXT_BLEND_OPERATION_ADVANCED_EXTENSION_NAME);
        vk::AddToPNextChain(&mEnabledFeatures, &mBlendOperationAdvancedFeatures);
    }

    if (getFeatures().supportsExtendedDynamicState.enabled)
    {
        mEnabledDeviceExtensions.push_back(VK_EXT_EXTENDED_DYNAMIC_STATE_EXTENSION_NAME);
        vk::AddToPNextChain(&mEnabledFeatures, &mExtendedDynamicStateFeatures);
    }

    if (getFeatures().supportsExtendedDynamicState2.enabled)
    {
        mEnabledDeviceExtensions.push_back(VK_EXT_EXTENDED_DYNAMIC_STATE_2_EXTENSION_NAME);
        vk::AddToPNextChain(&mEnabledFeatures, &mExtendedDynamicState2Features);
    }

    if (getFeatures().supportsGraphicsPipelineLibrary.enabled)
    {
        // VK_EXT_graphics_pipeline_library requires VK_KHR_pipeline_library
        ASSERT(ExtensionFound(VK_KHR_PIPELINE_LIBRARY_EXTENSION_NAME, deviceExtensionNames));
        mEnabledDeviceExtensions.push_back(VK_KHR_PIPELINE_LIBRARY_EXTENSION_NAME);

        mEnabledDeviceExtensions.push_back(VK_EXT_GRAPHICS_PIPELINE_LIBRARY_EXTENSION_NAME);
        vk::AddToPNextChain(&mEnabledFeatures, &mGraphicsPipelineLibraryFeatures);
    }

    if (getFeatures().supportsFragmentShadingRate.enabled)
    {
        mEnabledDeviceExtensions.push_back(VK_KHR_FRAGMENT_SHADING_RATE_EXTENSION_NAME);
        vk::AddToPNextChain(&mEnabledFeatures, &mFragmentShadingRateFeatures);
    }

    if (getFeatures().supportsFragmentShaderPixelInterlock.enabled)
    {
        mEnabledDeviceExtensions.push_back(VK_EXT_FRAGMENT_SHADER_INTERLOCK_EXTENSION_NAME);
        vk::AddToPNextChain(&mEnabledFeatures, &mFragmentShaderInterlockFeatures);
    }

    if (getFeatures().supportsImagelessFramebuffer.enabled)
    {
        mEnabledDeviceExtensions.push_back(VK_KHR_IMAGELESS_FRAMEBUFFER_EXTENSION_NAME);
        vk::AddToPNextChain(&mEnabledFeatures, &mImagelessFramebufferFeatures);
    }

    if (getFeatures().supportsPipelineRobustness.enabled)
    {
        mEnabledDeviceExtensions.push_back(VK_EXT_PIPELINE_ROBUSTNESS_EXTENSION_NAME);
        vk::AddToPNextChain(&mEnabledFeatures, &mPipelineRobustnessFeatures);
    }

    if (getFeatures().supportsPipelineProtectedAccess.enabled)
    {
        mEnabledDeviceExtensions.push_back(VK_EXT_PIPELINE_PROTECTED_ACCESS_EXTENSION_NAME);
        vk::AddToPNextChain(&mEnabledFeatures, &mPipelineProtectedAccessFeatures);
    }

    if (getFeatures().supportsRasterizationOrderAttachmentAccess.enabled)
    {
        if (ExtensionFound(VK_EXT_RASTERIZATION_ORDER_ATTACHMENT_ACCESS_EXTENSION_NAME,
                           deviceExtensionNames))
        {
            mEnabledDeviceExtensions.push_back(
                VK_EXT_RASTERIZATION_ORDER_ATTACHMENT_ACCESS_EXTENSION_NAME);
        }
        else
        {
            ASSERT(ExtensionFound(VK_ARM_RASTERIZATION_ORDER_ATTACHMENT_ACCESS_EXTENSION_NAME,
                                  deviceExtensionNames));
            mEnabledDeviceExtensions.push_back(
                VK_ARM_RASTERIZATION_ORDER_ATTACHMENT_ACCESS_EXTENSION_NAME);
        }
        vk::AddToPNextChain(&mEnabledFeatures, &mRasterizationOrderAttachmentAccessFeatures);
    }

    if (getFeatures().supportsSwapchainMaintenance1.enabled)
    {
        vk::AddToPNextChain(&mEnabledFeatures, &mSwapchainMaintenance1FeaturesEXT);
    }

    mCurrentQueueFamilyIndex = queueFamilyIndex;

    vk::QueueFamily queueFamily;
    queueFamily.initialize(mQueueFamilyProperties[queueFamilyIndex], queueFamilyIndex);
    ANGLE_VK_CHECK(displayVk, queueFamily.getDeviceQueueCount() > 0,
                   VK_ERROR_INITIALIZATION_FAILED);

    // We enable protected context only if both supportsProtectedMemory and device also supports
    // protected. There are cases we have to disable supportsProtectedMemory feature due to driver
    // bugs.
    bool enableProtectedContent =
        queueFamily.supportsProtected() && getFeatures().supportsProtectedMemory.enabled;

    uint32_t queueCount = std::min(queueFamily.getDeviceQueueCount(),
                                   static_cast<uint32_t>(egl::ContextPriority::EnumCount));

    uint32_t queueCreateInfoCount              = 1;
    VkDeviceQueueCreateInfo queueCreateInfo[1] = {};
    queueCreateInfo[0].sType                   = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo[0].flags = enableProtectedContent ? VK_DEVICE_QUEUE_CREATE_PROTECTED_BIT : 0;
    queueCreateInfo[0].queueFamilyIndex = queueFamilyIndex;
    queueCreateInfo[0].queueCount       = queueCount;
    queueCreateInfo[0].pQueuePriorities = vk::QueueFamily::kQueuePriorities;

    // Create Device
    createInfo.sType                 = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.flags                 = 0;
    createInfo.queueCreateInfoCount  = queueCreateInfoCount;
    createInfo.pQueueCreateInfos     = queueCreateInfo;
    createInfo.enabledLayerCount     = static_cast<uint32_t>(enabledDeviceLayerNames.size());
    createInfo.ppEnabledLayerNames   = enabledDeviceLayerNames.data();
    createInfo.enabledExtensionCount = static_cast<uint32_t>(mEnabledDeviceExtensions.size());
    createInfo.ppEnabledExtensionNames =
        mEnabledDeviceExtensions.empty() ? nullptr : mEnabledDeviceExtensions.data();
    mEnabledDeviceExtensions.push_back(nullptr);

    // Enable core features without assuming VkPhysicalDeviceFeatures2KHR is accepted in the
    // pNext chain of VkDeviceCreateInfo.
    createInfo.pEnabledFeatures = &mEnabledFeatures.features;

    // Append the feature structs chain to the end of createInfo structs chain.
    if (mEnabledFeatures.pNext)
    {
        vk::AppendToPNextChain(&createInfo, mEnabledFeatures.pNext);
    }

    // Create the list of expected VVL messages to suppress.  Done before creating the device, as it
    // may also generate messages.
    initializeValidationMessageSuppressions();

    ANGLE_VK_TRY(displayVk, vkCreateDevice(mPhysicalDevice, &createInfo, nullptr, &mDevice));
#if defined(ANGLE_SHARED_LIBVULKAN)
    // Load volk if we are loading dynamically
    volkLoadDevice(mDevice);
#endif  // defined(ANGLE_SHARED_LIBVULKAN)

    // crbug:1273344 In some driver we are seeing vkResetQueryPoolEXT is null but feature is
    // enabled. Disable the feature flag in this case
    if (mFeatures.supportsHostQueryReset.enabled && vkResetQueryPoolEXT == nullptr)
    {
        mFeatures.supportsHostQueryReset.enabled = false;
    }

    vk::DeviceQueueMap graphicsQueueMap =
        queueFamily.initializeQueueMap(mDevice, enableProtectedContent, 0, queueCount);

    ANGLE_TRY(mCommandQueue.init(displayVk, graphicsQueueMap));

    if (isAsyncCommandQueueEnabled())
    {
        ANGLE_TRY(mCommandProcessor.init());
    }

#if defined(ANGLE_SHARED_LIBVULKAN)
    // Avoid compiler warnings on unused-but-set variables.
    ANGLE_UNUSED_VARIABLE(hasGetMemoryRequirements2KHR);
    ANGLE_UNUSED_VARIABLE(hasBindMemory2KHR);
#else
    if (getFeatures().supportsHostQueryReset.enabled)
    {
        InitHostQueryResetFunctions(mInstance);
    }
    if (hasGetMemoryRequirements2KHR)
    {
        InitGetMemoryRequirements2KHRFunctions(mDevice);
    }
    if (hasBindMemory2KHR)
    {
        InitBindMemory2KHRFunctions(mDevice);
    }
    if (getFeatures().supportsTransformFeedbackExtension.enabled)
    {
        InitTransformFeedbackEXTFunctions(mDevice);
    }
    if (getFeatures().supportsYUVSamplerConversion.enabled)
    {
        InitSamplerYcbcrKHRFunctions(mDevice);
    }
    if (getFeatures().supportsRenderpass2.enabled)
    {
        InitRenderPass2KHRFunctions(mDevice);
    }
    if (getFeatures().supportsExtendedDynamicState.enabled)
    {
        InitExtendedDynamicStateEXTFunctions(mDevice);
    }
    if (getFeatures().supportsExtendedDynamicState2.enabled)
    {
        InitExtendedDynamicState2EXTFunctions(mDevice);
    }
    if (getFeatures().supportsFragmentShadingRate.enabled)
    {
        InitFragmentShadingRateKHRDeviceFunction(mDevice);
        ASSERT(vkCmdSetFragmentShadingRateKHR);
    }
    if (getFeatures().supportsTimestampSurfaceAttribute.enabled)
    {
        InitGetPastPresentationTimingGoogleFunction(mDevice);
        ASSERT(vkGetPastPresentationTimingGOOGLE);
    }
#endif  // !defined(ANGLE_SHARED_LIBVULKAN)

    if (getFeatures().forceMaxUniformBufferSize16KB.enabled)
    {
        mDefaultUniformBufferSize = kMinDefaultUniformBufferSize;
    }
    // Cap it with the driver limit
    mDefaultUniformBufferSize = std::min(
        mDefaultUniformBufferSize, getPhysicalDeviceProperties().limits.maxUniformBufferRange);

    // Initialize the vulkan pipeline cache.
    bool success = false;
    {
        std::unique_lock<std::mutex> lock(mPipelineCacheMutex);
        ANGLE_TRY(initPipelineCache(displayVk, &mPipelineCache, &success));
        ANGLE_TRY(getPipelineCacheSize(displayVk, &mPipelineCacheSizeAtLastSync));
    }

    // Track the set of supported pipeline stages.  This is used when issuing image layout
    // transitions that cover many stages (such as AllGraphicsReadOnly) to mask out unsupported
    // stages, which avoids enumerating every possible combination of stages in the layouts.
    VkPipelineStageFlags unsupportedStages = 0;
    mSupportedVulkanShaderStageMask =
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT;
    if (!mPhysicalDeviceFeatures.tessellationShader)
    {
        unsupportedStages |= VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT |
                             VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT;
    }
    else
    {
        mSupportedVulkanShaderStageMask |=
            VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT | VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
    }
    if (!mPhysicalDeviceFeatures.geometryShader)
    {
        unsupportedStages |= VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT;
    }
    else
    {
        mSupportedVulkanShaderStageMask |= VK_SHADER_STAGE_GEOMETRY_BIT;
    }
    mSupportedVulkanPipelineStageMask = ~unsupportedStages;

    // Log the memory heap stats when the device has been initialized (when debugging).
    if (kTrackMemoryAllocationDebug)
    {
        logMemoryHeapStats(this, vk::MemoryLogSeverity::INFO);
    }

    return angle::Result::Continue;
}

void RendererVk::initializeValidationMessageSuppressions()
{
    // Build the list of validation errors that are currently expected and should be skipped.
    mSkippedValidationMessages.insert(mSkippedValidationMessages.end(), kSkippedMessages,
                                      kSkippedMessages + ArraySize(kSkippedMessages));
    if (!getFeatures().supportsPrimitiveTopologyListRestart.enabled)
    {
        mSkippedValidationMessages.insert(
            mSkippedValidationMessages.end(), kNoListRestartSkippedMessages,
            kNoListRestartSkippedMessages + ArraySize(kNoListRestartSkippedMessages));
    }

    // Build the list of syncval errors that are currently expected and should be skipped.
    mSkippedSyncvalMessages.insert(mSkippedSyncvalMessages.end(), kSkippedSyncvalMessages,
                                   kSkippedSyncvalMessages + ArraySize(kSkippedSyncvalMessages));
    if (!getFeatures().supportsRenderPassStoreOpNone.enabled &&
        !getFeatures().supportsRenderPassLoadStoreOpNone.enabled)
    {
        mSkippedSyncvalMessages.insert(mSkippedSyncvalMessages.end(),
                                       kSkippedSyncvalMessagesWithoutStoreOpNone,
                                       kSkippedSyncvalMessagesWithoutStoreOpNone +
                                           ArraySize(kSkippedSyncvalMessagesWithoutStoreOpNone));
    }
    if (!getFeatures().supportsRenderPassLoadStoreOpNone.enabled)
    {
        mSkippedSyncvalMessages.insert(
            mSkippedSyncvalMessages.end(), kSkippedSyncvalMessagesWithoutLoadStoreOpNone,
            kSkippedSyncvalMessagesWithoutLoadStoreOpNone +
                ArraySize(kSkippedSyncvalMessagesWithoutLoadStoreOpNone));
    }
}

angle::Result RendererVk::selectPresentQueueForSurface(DisplayVk *displayVk,
                                                       VkSurfaceKHR surface,
                                                       uint32_t *presentQueueOut)
{
    // We've already initialized a device, and can't re-create it unless it's never been used.
    // TODO(jmadill): Handle the re-creation case if necessary.
    if (mDevice != VK_NULL_HANDLE)
    {
        ASSERT(mCurrentQueueFamilyIndex != std::numeric_limits<uint32_t>::max());

        // Check if the current device supports present on this surface.
        VkBool32 supportsPresent = VK_FALSE;
        ANGLE_VK_TRY(displayVk,
                     vkGetPhysicalDeviceSurfaceSupportKHR(mPhysicalDevice, mCurrentQueueFamilyIndex,
                                                          surface, &supportsPresent));

        if (supportsPresent == VK_TRUE)
        {
            *presentQueueOut = mCurrentQueueFamilyIndex;
            return angle::Result::Continue;
        }
    }

    // Find a graphics and present queue.
    Optional<uint32_t> newPresentQueue;
    uint32_t queueCount = static_cast<uint32_t>(mQueueFamilyProperties.size());
    constexpr VkQueueFlags kGraphicsAndCompute = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT;
    for (uint32_t queueIndex = 0; queueIndex < queueCount; ++queueIndex)
    {
        const auto &queueInfo = mQueueFamilyProperties[queueIndex];
        if ((queueInfo.queueFlags & kGraphicsAndCompute) == kGraphicsAndCompute)
        {
            VkBool32 supportsPresent = VK_FALSE;
            ANGLE_VK_TRY(displayVk, vkGetPhysicalDeviceSurfaceSupportKHR(
                                        mPhysicalDevice, queueIndex, surface, &supportsPresent));

            if (supportsPresent == VK_TRUE)
            {
                newPresentQueue = queueIndex;
                break;
            }
        }
    }

    ANGLE_VK_CHECK(displayVk, newPresentQueue.valid(), VK_ERROR_INITIALIZATION_FAILED);
    ANGLE_TRY(initializeDevice(displayVk, newPresentQueue.value()));

    *presentQueueOut = newPresentQueue.value();
    return angle::Result::Continue;
}

std::string RendererVk::getVendorString() const
{
    return GetVendorString(mPhysicalDeviceProperties.vendorID);
}

std::string RendererVk::getRendererDescription() const
{
    std::stringstream strstr;

    uint32_t apiVersion = mPhysicalDeviceProperties.apiVersion;

    strstr << "Vulkan ";
    strstr << VK_VERSION_MAJOR(apiVersion) << ".";
    strstr << VK_VERSION_MINOR(apiVersion) << ".";
    strstr << VK_VERSION_PATCH(apiVersion);

    strstr << " (";

    // In the case of NVIDIA, deviceName does not necessarily contain "NVIDIA". Add "NVIDIA" so that
    // Vulkan end2end tests can be selectively disabled on NVIDIA. TODO(jmadill): should not be
    // needed after http://anglebug.com/1874 is fixed and end2end_tests use more sophisticated
    // driver detection.
    if (mPhysicalDeviceProperties.vendorID == VENDOR_ID_NVIDIA)
    {
        strstr << GetVendorString(mPhysicalDeviceProperties.vendorID) << " ";
    }

    strstr << mPhysicalDeviceProperties.deviceName;
    strstr << " (" << gl::FmtHex(mPhysicalDeviceProperties.deviceID) << ")";

    strstr << ")";

    return strstr.str();
}

std::string RendererVk::getVersionString(bool includeFullVersion) const
{
    std::stringstream strstr;

    uint32_t driverVersion = mPhysicalDeviceProperties.driverVersion;
    std::string driverName = std::string(mDriverProperties.driverName);

    if (!driverName.empty())
    {
        strstr << driverName;
    }
    else
    {
        strstr << GetVendorString(mPhysicalDeviceProperties.vendorID);
    }

    if (includeFullVersion)
    {
        strstr << "-";

        if (mPhysicalDeviceProperties.vendorID == VENDOR_ID_NVIDIA)
        {
            strstr << ANGLE_VK_VERSION_MAJOR_NVIDIA(driverVersion) << ".";
            strstr << ANGLE_VK_VERSION_MINOR_NVIDIA(driverVersion) << ".";
            strstr << ANGLE_VK_VERSION_SUB_MINOR_NVIDIA(driverVersion) << ".";
            strstr << ANGLE_VK_VERSION_PATCH_NVIDIA(driverVersion);
        }
        else if (mPhysicalDeviceProperties.vendorID == VENDOR_ID_INTEL && IsWindows())
        {
            strstr << ANGLE_VK_VERSION_MAJOR_WIN_INTEL(driverVersion) << ".";
            strstr << ANGLE_VK_VERSION_MAJOR_WIN_INTEL(driverVersion) << ".";
        }
        // All other drivers use the Vulkan standard
        else
        {
            strstr << VK_VERSION_MAJOR(driverVersion) << ".";
            strstr << VK_VERSION_MINOR(driverVersion) << ".";
            strstr << VK_VERSION_PATCH(driverVersion);
        }
    }

    return strstr.str();
}

gl::Version RendererVk::getMaxSupportedESVersion() const
{
    // Current highest supported version
    gl::Version maxVersion = gl::Version(3, 2);

    // Early out without downgrading ES version if mock ICD enabled.
    // Mock ICD doesn't expose sufficient capabilities yet.
    // https://github.com/KhronosGroup/Vulkan-Tools/issues/84
    if (isMockICDEnabled())
    {
        return maxVersion;
    }

    // Limit to ES3.1 if there are any blockers for 3.2.
    if (!vk::CanSupportGPUShader5EXT(mPhysicalDeviceFeatures) &&
        !mFeatures.exposeNonConformantExtensionsAndVersions.enabled)
    {
        maxVersion = LimitVersionTo(maxVersion, {3, 1});
    }

    // TODO: more extension checks for 3.2.  http://anglebug.com/5366
    if (!mFeatures.exposeNonConformantExtensionsAndVersions.enabled)
    {
        maxVersion = LimitVersionTo(maxVersion, {3, 1});
    }

    // Limit to ES3.0 if there are any blockers for 3.1.

    // ES3.1 requires at least one atomic counter buffer and four storage buffers in compute.
    // Atomic counter buffers are emulated with storage buffers.  For simplicity, we always support
    // either none or IMPLEMENTATION_MAX_ATOMIC_COUNTER_BUFFERS atomic counter buffers.  So if
    // Vulkan doesn't support at least that many storage buffers in compute, we don't support 3.1.
    const uint32_t kMinimumStorageBuffersForES31 =
        gl::limits::kMinimumComputeStorageBuffers +
        gl::IMPLEMENTATION_MAX_ATOMIC_COUNTER_BUFFER_BINDINGS;
    if (mPhysicalDeviceProperties.limits.maxPerStageDescriptorStorageBuffers <
        kMinimumStorageBuffersForES31)
    {
        maxVersion = LimitVersionTo(maxVersion, {3, 0});
    }

    // ES3.1 requires at least a maximum offset of at least 2047.
    // If the Vulkan implementation can't support that, we cannot support 3.1.
    if (mPhysicalDeviceProperties.limits.maxVertexInputAttributeOffset < 2047)
    {
        maxVersion = LimitVersionTo(maxVersion, {3, 0});
    }

    // Limit to ES2.0 if there are any blockers for 3.0.
    // TODO: http://anglebug.com/3972 Limit to GLES 2.0 if flat shading can't be emulated

    // Multisample textures (ES3.1) and multisample renderbuffers (ES3.0) require the Vulkan driver
    // to support the standard sample locations (in order to pass dEQP tests that check these
    // locations).  If the Vulkan implementation can't support that, we cannot support 3.0/3.1.
    if (mPhysicalDeviceProperties.limits.standardSampleLocations != VK_TRUE)
    {
        maxVersion = LimitVersionTo(maxVersion, {2, 0});
    }

    // If independentBlend is not supported, we can't have a mix of has-alpha and emulated-alpha
    // render targets in a framebuffer.  We also cannot perform masked clears of multiple render
    // targets.
    if (!mPhysicalDeviceFeatures.independentBlend)
    {
        maxVersion = LimitVersionTo(maxVersion, {2, 0});
    }

    // If the Vulkan transform feedback extension is not present, we use an emulation path that
    // requires the vertexPipelineStoresAndAtomics feature. Without the extension or this feature,
    // we can't currently support transform feedback.
    if (!mFeatures.supportsTransformFeedbackExtension.enabled &&
        !mFeatures.emulateTransformFeedback.enabled)
    {
        maxVersion = LimitVersionTo(maxVersion, {2, 0});
    }

    // Limit to GLES 2.0 if maxPerStageDescriptorUniformBuffers is too low.
    // Table 6.31 MAX_VERTEX_UNIFORM_BLOCKS minimum value = 12
    // Table 6.32 MAX_FRAGMENT_UNIFORM_BLOCKS minimum value = 12
    // NOTE: We reserve some uniform buffers for emulation, so use the NativeCaps which takes this
    // into account, rather than the physical device maxPerStageDescriptorUniformBuffers limits.
    for (gl::ShaderType shaderType : gl::AllShaderTypes())
    {
        if (static_cast<GLuint>(getNativeCaps().maxShaderUniformBlocks[shaderType]) <
            gl::limits::kMinimumShaderUniformBlocks)
        {
            maxVersion = LimitVersionTo(maxVersion, {2, 0});
        }
    }

    // Limit to GLES 2.0 if maxVertexOutputComponents is too low.
    // Table 6.31 MAX VERTEX OUTPUT COMPONENTS minimum value = 64
    // NOTE: We reserve some vertex output components for emulation, so use the NativeCaps which
    // takes this into account, rather than the physical device maxVertexOutputComponents limits.
    if (static_cast<GLuint>(getNativeCaps().maxVertexOutputComponents) <
        gl::limits::kMinimumVertexOutputComponents)
    {
        maxVersion = LimitVersionTo(maxVersion, {2, 0});
    }

    return maxVersion;
}

gl::Version RendererVk::getMaxConformantESVersion() const
{
    const gl::Version maxSupportedESVersion = getMaxSupportedESVersion();
    const bool hasGeometryAndTessSupport =
        getNativeExtensions().geometryShaderAny() && getNativeExtensions().tessellationShaderEXT;

    if (!hasGeometryAndTessSupport || !mFeatures.exposeNonConformantExtensionsAndVersions.enabled)
    {
        return LimitVersionTo(maxSupportedESVersion, {3, 1});
    }

    return maxSupportedESVersion;
}

bool RendererVk::canSupportFragmentShadingRate(const vk::ExtensionNameList &deviceExtensionNames)
{
    // Device needs to support VK_KHR_fragment_shading_rate and specifically
    // pipeline fragment shading rate.
    if (mFragmentShadingRateFeatures.pipelineFragmentShadingRate != VK_TRUE)
    {
        return false;
    }

    // Init required functions
#if !defined(ANGLE_SHARED_LIBVULKAN)
    InitFragmentShadingRateKHRInstanceFunction(mInstance);
#endif  // !defined(ANGLE_SHARED_LIBVULKAN)
    ASSERT(vkGetPhysicalDeviceFragmentShadingRatesKHR);

    // Query number of supported shading rates first
    uint32_t shadingRatesCount = 0;
    VkResult result =
        vkGetPhysicalDeviceFragmentShadingRatesKHR(mPhysicalDevice, &shadingRatesCount, nullptr);
    ASSERT(result == VK_SUCCESS);
    ASSERT(shadingRatesCount > 0);

    std::vector<VkPhysicalDeviceFragmentShadingRateKHR> shadingRates(
        shadingRatesCount,
        {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_KHR, nullptr, 0, {0, 0}});

    // Query supported shading rates
    result = vkGetPhysicalDeviceFragmentShadingRatesKHR(mPhysicalDevice, &shadingRatesCount,
                                                        shadingRates.data());
    ASSERT(result == VK_SUCCESS);

    // Cache supported fragment shading rates
    mSupportedFragmentShadingRates.reset();
    for (const VkPhysicalDeviceFragmentShadingRateKHR &shadingRate : shadingRates)
    {
        if (shadingRate.sampleCounts == 0)
        {
            continue;
        }
        mSupportedFragmentShadingRates.set(GetShadingRateFromVkExtent(shadingRate.fragmentSize));
    }

    // To implement GL_QCOM_shading_rate extension the Vulkan ICD needs to support at least the
    // following shading rates -
    //     {1, 1}
    //     {1, 2}
    //     {2, 1}
    //     {2, 2}
    return mSupportedFragmentShadingRates.test(gl::ShadingRate::_1x1) &&
           mSupportedFragmentShadingRates.test(gl::ShadingRate::_1x2) &&
           mSupportedFragmentShadingRates.test(gl::ShadingRate::_2x1) &&
           mSupportedFragmentShadingRates.test(gl::ShadingRate::_2x2);
}

bool RendererVk::canPreferDeviceLocalMemoryHostVisible(VkPhysicalDeviceType deviceType)
{
    if (deviceType == VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU)
    {
        const vk::MemoryProperties &memoryProperties = getMemoryProperties();
        static constexpr VkMemoryPropertyFlags kHostVisiableDeviceLocalFlags =
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
        VkDeviceSize minHostVisiableDeviceLocalHeapSize = std::numeric_limits<VkDeviceSize>::max();
        VkDeviceSize maxDeviceLocalHeapSize             = 0;
        for (uint32_t i = 0; i < memoryProperties.getMemoryTypeCount(); ++i)
        {
            if ((memoryProperties.getMemoryType(i).propertyFlags &
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) != 0)
            {
                maxDeviceLocalHeapSize =
                    std::max(maxDeviceLocalHeapSize, memoryProperties.getHeapSizeForMemoryType(i));
            }
            if ((memoryProperties.getMemoryType(i).propertyFlags & kHostVisiableDeviceLocalFlags) ==
                kHostVisiableDeviceLocalFlags)
            {
                minHostVisiableDeviceLocalHeapSize =
                    std::min(minHostVisiableDeviceLocalHeapSize,
                             memoryProperties.getHeapSizeForMemoryType(i));
            }
        }
        return minHostVisiableDeviceLocalHeapSize != std::numeric_limits<VkDeviceSize>::max() &&
               minHostVisiableDeviceLocalHeapSize >=
                   static_cast<VkDeviceSize>(maxDeviceLocalHeapSize * 0.8);
    }
    return deviceType != VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
}

void RendererVk::initFeatures(DisplayVk *displayVk,
                              const vk::ExtensionNameList &deviceExtensionNames)
{
    if (displayVk->getState().featuresAllDisabled)
    {
        ApplyFeatureOverrides(&mFeatures, displayVk->getState());
        return;
    }

    constexpr uint32_t kPixel2DriverWithRelaxedPrecision        = 0x801EA000;
    constexpr uint32_t kPixel4DriverWithWorkingSpecConstSupport = 0x80201000;

    const bool isAMD      = IsAMD(mPhysicalDeviceProperties.vendorID);
    const bool isARM      = IsARM(mPhysicalDeviceProperties.vendorID);
    const bool isIntel    = IsIntel(mPhysicalDeviceProperties.vendorID);
    const bool isNvidia   = IsNvidia(mPhysicalDeviceProperties.vendorID);
    const bool isPowerVR  = IsPowerVR(mPhysicalDeviceProperties.vendorID);
    const bool isQualcomm = IsQualcomm(mPhysicalDeviceProperties.vendorID);
    const bool isBroadcom = IsBroadcom(mPhysicalDeviceProperties.vendorID);
    const bool isSamsung  = IsSamsung(mPhysicalDeviceProperties.vendorID);
    const bool isSwiftShader =
        IsSwiftshader(mPhysicalDeviceProperties.vendorID, mPhysicalDeviceProperties.deviceID);

    // Distinguish between the open source and proprietary Qualcomm drivers
    const bool isQualcommOpenSource =
        IsQualcommOpenSource(mPhysicalDeviceProperties.vendorID, mDriverProperties.driverID,
                             mPhysicalDeviceProperties.deviceName);
    const bool isQualcommProprietary = isQualcomm && !isQualcommOpenSource;

    // Parse the ARM driver version to be readable/comparable
    const ARMDriverVersion armDriverVersion =
        ParseARMDriverVersion(mPhysicalDeviceProperties.driverVersion);

    angle::VersionInfo nvidiaVersion;
    if (isNvidia)
    {
        nvidiaVersion = angle::ParseNvidiaDriverVersion(mPhysicalDeviceProperties.driverVersion);
    }

    angle::VersionInfo mesaVersion;
    if (isIntel && IsLinux())
    {
        mesaVersion = angle::ParseMesaDriverVersion(mPhysicalDeviceProperties.driverVersion);
    }

    // Classify devices based on general architecture:
    //
    // - IMR (Immediate-Mode Rendering) devices generally progress through draw calls once and use
    //   the main GPU memory (accessed through caches) to store intermediate rendering results.
    // - TBR (Tile-Based Rendering) devices issue a pre-rendering geometry pass, then run through
    //   draw calls once per tile and store intermediate rendering results on the tile cache.
    //
    // Due to these key architectural differences, some operations improve performance on one while
    // deteriorating performance on the other.  ANGLE will accordingly make some decisions based on
    // the device architecture for optimal performance on both.
    const bool isImmediateModeRenderer = isNvidia || isAMD || isIntel || isSamsung || isSwiftShader;
    const bool isTileBasedRenderer     = isARM || isPowerVR || isQualcomm || isBroadcom;

    // Make sure all known architectures are accounted for.
    if (!isImmediateModeRenderer && !isTileBasedRenderer && !isMockICDEnabled())
    {
        WARN() << "Unknown GPU architecture";
    }

    bool supportsNegativeViewport =
        ExtensionFound(VK_KHR_MAINTENANCE1_EXTENSION_NAME, deviceExtensionNames) ||
        mPhysicalDeviceProperties.apiVersion >= VK_API_VERSION_1_1;

    if (mLineRasterizationFeatures.bresenhamLines == VK_TRUE)
    {
        ASSERT(mLineRasterizationFeatures.sType ==
               VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_LINE_RASTERIZATION_FEATURES_EXT);
        ANGLE_FEATURE_CONDITION(&mFeatures, bresenhamLineRasterization, true);
    }

    if (mProvokingVertexFeatures.provokingVertexLast == VK_TRUE)
    {
        ASSERT(mProvokingVertexFeatures.sType ==
               VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROVOKING_VERTEX_FEATURES_EXT);
        ANGLE_FEATURE_CONDITION(&mFeatures, provokingVertex, true);
    }

    // http://b/208458772. ARM driver supports this protected memory extension but we are seeing
    // excessive load/store unit activity when this extension is enabled, even if not been used.
    // Disable this extension on older ARM platforms that don't support
    // VK_EXT_pipeline_protected_access.
    // http://anglebug.com/7714
    ANGLE_FEATURE_CONDITION(
        &mFeatures, supportsProtectedMemory,
        mProtectedMemoryFeatures.protectedMemory == VK_TRUE &&
            (!isARM || mPipelineProtectedAccessFeatures.pipelineProtectedAccess == VK_TRUE));

    ANGLE_FEATURE_CONDITION(&mFeatures, supportsHostQueryReset,
                            mHostQueryResetFeatures.hostQueryReset == VK_TRUE);

    // VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL and
    // VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL are introduced by
    // VK_KHR_maintenance2 and promoted to Vulkan 1.1.  For simplicity, this feature is only enabled
    // on Vulkan 1.1.
    ANGLE_FEATURE_CONDITION(&mFeatures, supportsMixedReadWriteDepthStencilLayouts,
                            mPhysicalDeviceProperties.apiVersion >= VK_API_VERSION_1_1);

    // VK_EXT_pipeline_creation_feedback is promoted to core in Vulkan 1.3.
    ANGLE_FEATURE_CONDITION(
        &mFeatures, supportsPipelineCreationFeedback,
        ExtensionFound(VK_EXT_PIPELINE_CREATION_FEEDBACK_EXTENSION_NAME, deviceExtensionNames) ||
            mPhysicalDeviceProperties.apiVersion >= VK_API_VERSION_1_3);

    // Incomplete implementation on SwiftShader: http://issuetracker.google.com/234439593
    ANGLE_FEATURE_CONDITION(
        &mFeatures, supportsPipelineCreationCacheControl,
        mPipelineCreationCacheControlFeatures.pipelineCreationCacheControl && !isSwiftShader);

    // Note: Protected Swapchains is not determined until we have a VkSurface to query.
    // So here vendors should indicate support so that protected_content extension
    // is enabled.
    ANGLE_FEATURE_CONDITION(&mFeatures, supportsSurfaceProtectedSwapchains, IsAndroid());

    // Work around incorrect NVIDIA point size range clamping.
    // http://anglebug.com/2970#c10
    // Clamp if driver version is:
    //   < 430 on Windows
    //   < 421 otherwise
    ANGLE_FEATURE_CONDITION(&mFeatures, clampPointSize,
                            isNvidia && nvidiaVersion.major < uint32_t(IsWindows() ? 430 : 421));
    // http://anglebug.com/3970#c25.
    // The workaround requires the VK_EXT_depth_clip_enable extension and the 'depthClamp' physical
    // device feature. This workaround caused test failures on Quadro P400/driver 418.56/Linux.
    // Therefore, on Linux we require a major version > 418.
    ANGLE_FEATURE_CONDITION(
        &mFeatures, depthClamping,
        isNvidia && mPhysicalDeviceFeatures.depthClamp &&
            ExtensionFound(VK_EXT_DEPTH_CLIP_ENABLE_EXTENSION_NAME, deviceExtensionNames) &&
            (!IsLinux() || nvidiaVersion.major > 418u));

    ANGLE_FEATURE_CONDITION(
        &mFeatures, supportsRenderpass2,
        ExtensionFound(VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME, deviceExtensionNames));

    ANGLE_FEATURE_CONDITION(
        &mFeatures, supportsIncrementalPresent,
        ExtensionFound(VK_KHR_INCREMENTAL_PRESENT_EXTENSION_NAME, deviceExtensionNames));

#if defined(ANGLE_PLATFORM_ANDROID)
    ANGLE_FEATURE_CONDITION(
        &mFeatures, supportsAndroidHardwareBuffer,
        IsAndroid() &&
            ExtensionFound(VK_ANDROID_EXTERNAL_MEMORY_ANDROID_HARDWARE_BUFFER_EXTENSION_NAME,
                           deviceExtensionNames) &&
            ExtensionFound(VK_EXT_QUEUE_FAMILY_FOREIGN_EXTENSION_NAME, deviceExtensionNames));
#endif

#if defined(ANGLE_PLATFORM_GGP)
    ANGLE_FEATURE_CONDITION(
        &mFeatures, supportsGGPFrameToken,
        ExtensionFound(VK_GGP_FRAME_TOKEN_EXTENSION_NAME, deviceExtensionNames));
#endif

    ANGLE_FEATURE_CONDITION(
        &mFeatures, supportsExternalMemoryFd,
        ExtensionFound(VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME, deviceExtensionNames));

    ANGLE_FEATURE_CONDITION(
        &mFeatures, supportsExternalMemoryFuchsia,
        ExtensionFound(VK_FUCHSIA_EXTERNAL_MEMORY_EXTENSION_NAME, deviceExtensionNames));

    ANGLE_FEATURE_CONDITION(
        &mFeatures, supportsFilteringPrecision,
        ExtensionFound(VK_GOOGLE_SAMPLER_FILTERING_PRECISION_EXTENSION_NAME, deviceExtensionNames));

    ANGLE_FEATURE_CONDITION(
        &mFeatures, supportsExternalSemaphoreFd,
        ExtensionFound(VK_KHR_EXTERNAL_SEMAPHORE_FD_EXTENSION_NAME, deviceExtensionNames));

    ANGLE_FEATURE_CONDITION(
        &mFeatures, supportsExternalSemaphoreFuchsia,
        ExtensionFound(VK_FUCHSIA_EXTERNAL_SEMAPHORE_EXTENSION_NAME, deviceExtensionNames));

    ANGLE_FEATURE_CONDITION(
        &mFeatures, supportsExternalFenceFd,
        ExtensionFound(VK_KHR_EXTERNAL_FENCE_FD_EXTENSION_NAME, deviceExtensionNames));

#if defined(ANGLE_PLATFORM_ANDROID)
    if ((mFeatures.supportsExternalFenceCapabilities.enabled &&
         mFeatures.supportsExternalSemaphoreCapabilities.enabled) ||
        mPhysicalDeviceProperties.apiVersion >= VK_MAKE_VERSION(1, 1, 0))
    {
        VkExternalFenceProperties externalFenceProperties = {};
        externalFenceProperties.sType = VK_STRUCTURE_TYPE_EXTERNAL_FENCE_PROPERTIES;

        VkPhysicalDeviceExternalFenceInfo externalFenceInfo = {};
        externalFenceInfo.sType      = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_FENCE_INFO;
        externalFenceInfo.handleType = VK_EXTERNAL_FENCE_HANDLE_TYPE_SYNC_FD_BIT_KHR;

        vkGetPhysicalDeviceExternalFencePropertiesKHR(mPhysicalDevice, &externalFenceInfo,
                                                      &externalFenceProperties);

        VkExternalSemaphoreProperties externalSemaphoreProperties = {};
        externalSemaphoreProperties.sType = VK_STRUCTURE_TYPE_EXTERNAL_SEMAPHORE_PROPERTIES;

        VkPhysicalDeviceExternalSemaphoreInfo externalSemaphoreInfo = {};
        externalSemaphoreInfo.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_SEMAPHORE_INFO;
        externalSemaphoreInfo.handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT_KHR;

        vkGetPhysicalDeviceExternalSemaphorePropertiesKHR(mPhysicalDevice, &externalSemaphoreInfo,
                                                          &externalSemaphoreProperties);

        ANGLE_FEATURE_CONDITION(
            &mFeatures, supportsAndroidNativeFenceSync,
            (mFeatures.supportsExternalFenceFd.enabled &&
             FencePropertiesCompatibleWithAndroid(externalFenceProperties) &&
             mFeatures.supportsExternalSemaphoreFd.enabled &&
             SemaphorePropertiesCompatibleWithAndroid(externalSemaphoreProperties)));
    }
    else
    {
        ANGLE_FEATURE_CONDITION(&mFeatures, supportsAndroidNativeFenceSync,
                                (mFeatures.supportsExternalFenceFd.enabled &&
                                 mFeatures.supportsExternalSemaphoreFd.enabled));
    }
#endif  // defined(ANGLE_PLATFORM_ANDROID)

    ANGLE_FEATURE_CONDITION(
        &mFeatures, supportsShaderStencilExport,
        ExtensionFound(VK_EXT_SHADER_STENCIL_EXPORT_EXTENSION_NAME, deviceExtensionNames));

    ANGLE_FEATURE_CONDITION(
        &mFeatures, supportsRenderPassLoadStoreOpNone,
        ExtensionFound(VK_EXT_LOAD_STORE_OP_NONE_EXTENSION_NAME, deviceExtensionNames));

    ANGLE_FEATURE_CONDITION(&mFeatures, disallowMixedDepthStencilLoadOpNoneAndLoad,
                            isARM && armDriverVersion < ARMDriverVersion(38, 1, 0));

    ANGLE_FEATURE_CONDITION(
        &mFeatures, supportsRenderPassStoreOpNone,
        !mFeatures.supportsRenderPassLoadStoreOpNone.enabled &&
            ExtensionFound(VK_QCOM_RENDER_PASS_STORE_OPS_EXTENSION_NAME, deviceExtensionNames));

    ANGLE_FEATURE_CONDITION(&mFeatures, supportsDepthClipControl,
                            mDepthClipControlFeatures.depthClipControl == VK_TRUE);

    ANGLE_FEATURE_CONDITION(&mFeatures, supportsPrimitivesGeneratedQuery,
                            mPrimitivesGeneratedQueryFeatures.primitivesGeneratedQuery == VK_TRUE);

    ANGLE_FEATURE_CONDITION(
        &mFeatures, supportsPrimitiveTopologyListRestart,
        mPrimitiveTopologyListRestartFeatures.primitiveTopologyListRestart == VK_TRUE);

    ANGLE_FEATURE_CONDITION(
        &mFeatures, supportsBlendOperationAdvanced,
        ExtensionFound(VK_EXT_BLEND_OPERATION_ADVANCED_EXTENSION_NAME, deviceExtensionNames));

    ANGLE_FEATURE_CONDITION(&mFeatures, supportsTransformFeedbackExtension,
                            mTransformFeedbackFeatures.transformFeedback == VK_TRUE);

    ANGLE_FEATURE_CONDITION(&mFeatures, supportsGeometryStreamsCapability,
                            mTransformFeedbackFeatures.geometryStreams == VK_TRUE);

    ANGLE_FEATURE_CONDITION(&mFeatures, supportsIndexTypeUint8,
                            mIndexTypeUint8Features.indexTypeUint8 == VK_TRUE);

    ANGLE_FEATURE_CONDITION(&mFeatures, supportsDepthStencilResolve,
                            mFeatures.supportsRenderpass2.enabled &&
                                mDepthStencilResolveProperties.supportedDepthResolveModes != 0);

    ANGLE_FEATURE_CONDITION(
        &mFeatures, supportsMultisampledRenderToSingleSampled,
        mFeatures.supportsRenderpass2.enabled && mFeatures.supportsDepthStencilResolve.enabled &&
            mMultisampledRenderToSingleSampledFeatures.multisampledRenderToSingleSampled ==
                VK_TRUE);

    ANGLE_FEATURE_CONDITION(
        &mFeatures, supportsMultisampledRenderToSingleSampledGOOGLEX,
        !mFeatures.supportsMultisampledRenderToSingleSampled.enabled &&
            mFeatures.supportsRenderpass2.enabled &&
            mFeatures.supportsDepthStencilResolve.enabled &&
            mMultisampledRenderToSingleSampledFeaturesGOOGLEX.multisampledRenderToSingleSampled ==
                VK_TRUE);

    ANGLE_FEATURE_CONDITION(&mFeatures, supportsImage2dViewOf3d,
                            mImage2dViewOf3dFeatures.image2DViewOf3D == VK_TRUE &&
                                mImage2dViewOf3dFeatures.sampler2DViewOf3D == VK_TRUE);

    ANGLE_FEATURE_CONDITION(&mFeatures, supportsMultiview, mMultiviewFeatures.multiview == VK_TRUE);

    ANGLE_FEATURE_CONDITION(&mFeatures, emulateTransformFeedback,
                            (!mFeatures.supportsTransformFeedbackExtension.enabled &&
                             mPhysicalDeviceFeatures.vertexPipelineStoresAndAtomics == VK_TRUE));

    // TODO: http://anglebug.com/5927 - drop dependency on customBorderColorWithoutFormat.
    ANGLE_FEATURE_CONDITION(
        &mFeatures, supportsCustomBorderColor,
        mCustomBorderColorFeatures.customBorderColors == VK_TRUE &&
            mCustomBorderColorFeatures.customBorderColorWithoutFormat == VK_TRUE);

    ANGLE_FEATURE_CONDITION(&mFeatures, supportsMultiDrawIndirect,
                            mPhysicalDeviceFeatures.multiDrawIndirect == VK_TRUE);

    ANGLE_FEATURE_CONDITION(&mFeatures, perFrameWindowSizeQuery,
                            IsAndroid() || isIntel || (IsWindows() && isAMD) || IsFuchsia() ||
                                isSamsung || displayVk->isWayland());

    ANGLE_FEATURE_CONDITION(&mFeatures, padBuffersToMaxVertexAttribStride, isAMD || isSamsung);
    mMaxVertexAttribStride = std::min(static_cast<uint32_t>(gl::limits::kMaxVertexAttribStride),
                                      mPhysicalDeviceProperties.limits.maxVertexInputBindingStride);

    ANGLE_FEATURE_CONDITION(&mFeatures, forceD16TexFilter, IsAndroid() && isQualcommProprietary);

    ANGLE_FEATURE_CONDITION(&mFeatures, disableFlippingBlitWithCommand,
                            IsAndroid() && isQualcommProprietary);

    // Allocation sanitization disabled by default because of a heaveyweight implementation
    // that can cause OOM and timeouts.
    ANGLE_FEATURE_CONDITION(&mFeatures, allocateNonZeroMemory, false);

    // ARM does buffer copy on geometry pipeline, which may create a GPU pipeline bubble that
    // prevents vertex shader to overlap with fragment shader. For now we always choose CPU to do
    // copy on ARM. This may need to test with future ARM GPU architecture as well.
    ANGLE_FEATURE_CONDITION(&mFeatures, preferCPUForBufferSubData, isARM);

    // On android, we usually are GPU limited, we try to use CPU to do data copy when other
    // conditions are the same. Set to zero will use GPU to do copy. This is subject to further
    // tuning for each platform https://issuetracker.google.com/201826021
    mMaxCopyBytesUsingCPUWhenPreservingBufferData =
        IsAndroid() ? std::numeric_limits<uint32_t>::max() : 0;

    ANGLE_FEATURE_CONDITION(&mFeatures, persistentlyMappedBuffers, true);

    ANGLE_FEATURE_CONDITION(&mFeatures, logMemoryReportCallbacks, false);
    ANGLE_FEATURE_CONDITION(&mFeatures, logMemoryReportStats, false);

    ANGLE_FEATURE_CONDITION(
        &mFeatures, supportsExternalMemoryDmaBufAndModifiers,
        ExtensionFound(VK_EXT_EXTERNAL_MEMORY_DMA_BUF_EXTENSION_NAME, deviceExtensionNames) &&
            ExtensionFound(VK_EXT_IMAGE_DRM_FORMAT_MODIFIER_EXTENSION_NAME, deviceExtensionNames));

    ANGLE_FEATURE_CONDITION(
        &mFeatures, supportsExternalMemoryHost,
        ExtensionFound(VK_EXT_EXTERNAL_MEMORY_HOST_EXTENSION_NAME, deviceExtensionNames));

    // Android pre-rotation support can be disabled.
    ANGLE_FEATURE_CONDITION(&mFeatures, enablePreRotateSurfaces,
                            IsAndroid() && supportsNegativeViewport);

    // http://anglebug.com/3078
    ANGLE_FEATURE_CONDITION(
        &mFeatures, enablePrecisionQualifiers,
        !(IsPixel2(mPhysicalDeviceProperties.vendorID, mPhysicalDeviceProperties.deviceID) &&
          (mPhysicalDeviceProperties.driverVersion < kPixel2DriverWithRelaxedPrecision)) &&
            !IsPixel4(mPhysicalDeviceProperties.vendorID, mPhysicalDeviceProperties.deviceID));

    // http://anglebug.com/7488
    ANGLE_FEATURE_CONDITION(&mFeatures, varyingsRequireMatchingPrecisionInSpirv, isPowerVR);

    // IMR devices are less sensitive to the src/dst stage masks in barriers, and behave more
    // efficiently when all barriers are aggregated, rather than individually and precisely
    // specified.
    ANGLE_FEATURE_CONDITION(&mFeatures, preferAggregateBarrierCalls, isImmediateModeRenderer);

    // For IMR devices, it's more efficient to ignore invalidate of framebuffer attachments with
    // emulated formats that have extra channels.  For TBR devices, the invalidate will be followed
    // by a clear to retain valid values in said extra channels.
    ANGLE_FEATURE_CONDITION(&mFeatures, preferSkippingInvalidateForEmulatedFormats,
                            isImmediateModeRenderer);

    // Currently disabled by default: http://anglebug.com/4324
    ANGLE_FEATURE_CONDITION(&mFeatures, asyncCommandQueue, false);

    ANGLE_FEATURE_CONDITION(&mFeatures, supportsYUVSamplerConversion,
                            mSamplerYcbcrConversionFeatures.samplerYcbcrConversion != VK_FALSE);

    ANGLE_FEATURE_CONDITION(&mFeatures, supportsShaderFloat16,
                            mShaderFloat16Int8Features.shaderFloat16 == VK_TRUE);

    // Prefer driver uniforms over specialization constants in the following:
    //
    // - Older Qualcomm drivers where specialization constants severly degrade the performance of
    //   pipeline creation.  http://issuetracker.google.com/173636783
    // - ARM hardware
    // - Imagination hardware
    // - SwiftShader
    //
    ANGLE_FEATURE_CONDITION(
        &mFeatures, preferDriverUniformOverSpecConst,
        (isQualcommProprietary &&
         mPhysicalDeviceProperties.driverVersion < kPixel4DriverWithWorkingSpecConstSupport) ||
            isARM || isPowerVR || isSwiftShader);

    // The compute shader used to generate mipmaps needs -
    // 1. subgroup quad operations in compute shader stage.
    // 2. subgroup operations that can use extended types.
    // 3. 256-wide workgroup.
    //
    // Furthermore, VK_IMAGE_USAGE_STORAGE_BIT is detrimental to performance on many platforms, on
    // which this path is not enabled.  Platforms that are known to have better performance with
    // this path are:
    //
    // - AMD
    // - Nvidia
    // - Samsung
    //
    // Additionally, this path is disabled on buggy drivers:
    //
    // - AMD/Windows: Unfortunately the trybots use ancient AMD cards and drivers.
    const bool supportsSubgroupQuadOpsInComputeShader =
        (mSubgroupProperties.supportedStages & VK_SHADER_STAGE_COMPUTE_BIT) &&
        (mSubgroupProperties.supportedOperations & VK_SUBGROUP_FEATURE_QUAD_BIT);

    const uint32_t maxComputeWorkGroupInvocations =
        mPhysicalDeviceProperties.limits.maxComputeWorkGroupInvocations;

    ANGLE_FEATURE_CONDITION(&mFeatures, allowGenerateMipmapWithCompute,
                            supportsSubgroupQuadOpsInComputeShader &&
                                mSubgroupExtendedTypesFeatures.shaderSubgroupExtendedTypes &&
                                maxComputeWorkGroupInvocations >= 256 &&
                                ((isAMD && !IsWindows()) || isNvidia || isSamsung));

    bool isAdreno540 = mPhysicalDeviceProperties.deviceID == angle::kDeviceID_Adreno540;
    ANGLE_FEATURE_CONDITION(&mFeatures, forceMaxUniformBufferSize16KB,
                            isQualcommProprietary && isAdreno540);

    ANGLE_FEATURE_CONDITION(
        &mFeatures, supportsImageFormatList,
        ExtensionFound(VK_KHR_IMAGE_FORMAT_LIST_EXTENSION_NAME, deviceExtensionNames));

    // Emulation of GL_EXT_multisampled_render_to_texture is only really useful on tiling hardware,
    // but is exposed on any configuration deployed on Android, such as Samsung's AMD-based GPU.
    //
    // During testing, it was also discovered that emulation triggers bugs on some platforms:
    //
    // - Swiftshader:
    //   * Failure on mac: http://anglebug.com/4937
    //   * OOM: http://crbug.com/1263046
    // - Intel on windows: http://anglebug.com/5032
    // - AMD on windows: http://crbug.com/1132366
    //
    const bool supportsIndependentDepthStencilResolve =
        mFeatures.supportsDepthStencilResolve.enabled &&
        mDepthStencilResolveProperties.independentResolveNone == VK_TRUE;
    ANGLE_FEATURE_CONDITION(
        &mFeatures, enableMultisampledRenderToTexture,
        mFeatures.supportsMultisampledRenderToSingleSampled.enabled ||
            mFeatures.supportsMultisampledRenderToSingleSampledGOOGLEX.enabled ||
            (supportsIndependentDepthStencilResolve && (isTileBasedRenderer || isSamsung)));

    // Currently we enable cube map arrays based on the imageCubeArray Vk feature.
    // TODO: Check device caps for full cube map array support. http://anglebug.com/5143
    ANGLE_FEATURE_CONDITION(&mFeatures, supportsImageCubeArray,
                            mPhysicalDeviceFeatures.imageCubeArray == VK_TRUE);

    ANGLE_FEATURE_CONDITION(&mFeatures, supportsPipelineStatisticsQuery,
                            mPhysicalDeviceFeatures.pipelineStatisticsQuery == VK_TRUE);

    // Defer glFLush call causes manhattan 3.0 perf regression. Let Qualcomm driver opt out from
    // this optimization.
    ANGLE_FEATURE_CONDITION(&mFeatures, deferFlushUntilEndRenderPass, !isQualcommProprietary);

    // Android mistakenly destroys the old swapchain when creating a new one.
    ANGLE_FEATURE_CONDITION(&mFeatures, waitIdleBeforeSwapchainRecreation, IsAndroid() && isARM);

    // vkCmdClearAttachments races with draw calls on Qualcomm hardware as observed on Pixel2 and
    // Pixel4.  https://issuetracker.google.com/issues/166809097
    ANGLE_FEATURE_CONDITION(&mFeatures, preferDrawClearOverVkCmdClearAttachments,
                            isQualcommProprietary);

    // r32f image emulation is done unconditionally so VK_FORMAT_FEATURE_STORAGE_*_ATOMIC_BIT is not
    // required.
    ANGLE_FEATURE_CONDITION(&mFeatures, emulateR32fImageAtomicExchange, true);

    // Negative viewports are exposed in the Maintenance1 extension and in core Vulkan 1.1+.
    ANGLE_FEATURE_CONDITION(&mFeatures, supportsNegativeViewport, supportsNegativeViewport);

    // Whether non-conformant configurations and extensions should be exposed.
    ANGLE_FEATURE_CONDITION(&mFeatures, exposeNonConformantExtensionsAndVersions,
                            kExposeNonConformantExtensionsAndVersions);

    // Memory budget extension is disabled for Swiftshader due to lack of support.
    ANGLE_FEATURE_CONDITION(&mFeatures, supportsMemoryBudget, !isSwiftShader);

    // Disabled by default. Only enable it for experimental purpose, as this will cause various
    // tests to fail.
    ANGLE_FEATURE_CONDITION(&mFeatures, forceFragmentShaderPrecisionHighpToMediump, false);

    // Testing shows that on ARM GPU, doing implicit flush at framebuffer boundary improves
    // performance. Most app traces shows frame time reduced and manhattan 3.1 offscreen score
    // improves 7%.
    ANGLE_FEATURE_CONDITION(&mFeatures, preferSubmitAtFBOBoundary, isARM || isSwiftShader);

    // In order to support immutable samplers tied to external formats, we need to overallocate
    // descriptor counts for such immutable samplers
    ANGLE_FEATURE_CONDITION(&mFeatures, useMultipleDescriptorsForExternalFormats, true);

    // http://anglebug.com/6651
    // When creating a surface with the format GL_RGB8, override the format to be GL_RGBA8, since
    // Android prevents creating swapchain images with VK_FORMAT_R8G8B8_UNORM.
    // Do this for all platforms, since few (none?) IHVs support 24-bit formats with their HW
    // natively anyway.
    ANGLE_FEATURE_CONDITION(&mFeatures, overrideSurfaceFormatRGB8ToRGBA8, true);

    // We set
    //
    // - VK_PIPELINE_COLOR_BLEND_STATE_CREATE_RASTERIZATION_ORDER_ATTACHMENT_ACCESS_BIT_EXT
    // - VK_SUBPASS_DESCRIPTION_RASTERIZATION_ORDER_ATTACHMENT_COLOR_ACCESS_BIT_EXT
    //
    // when this feature is supported and there is framebuffer fetch.  But the
    // check for framebuffer fetch is not accurate enough and those bits can
    // have great impact on Qualcomm (it only affects the open source driver
    // because the proprietary driver does not expose the extension).  Let's
    // disable it on Qualcomm.
    //
    // https://issuetracker.google.com/issues/255837430
    ANGLE_FEATURE_CONDITION(
        &mFeatures, supportsRasterizationOrderAttachmentAccess,
        !isQualcomm &&
            mRasterizationOrderAttachmentAccessFeatures.rasterizationOrderColorAttachmentAccess ==
                VK_TRUE);

    // The VK_EXT_surface_maintenance1 and VK_EXT_swapchain_maintenance1 extensions are used for a
    // variety of improvements:
    //
    // - Recycling present semaphores
    // - Avoiding swapchain recreation when present modes change
    // - Amortizing the cost of memory allocation for swapchain creation over multiple frames
    //
    ANGLE_FEATURE_CONDITION(&mFeatures, supportsSwapchainMaintenance1,
                            mSwapchainMaintenance1FeaturesEXT.swapchainMaintenance1 == VK_TRUE);

    // http://anglebug.com/6872
    // On ARM hardware, framebuffer-fetch-like behavior on Vulkan is already coherent, so we can
    // expose the coherent version of the GL extension despite unofficial Vulkan support.
    ANGLE_FEATURE_CONDITION(
        &mFeatures, supportsShaderFramebufferFetch,
        (IsAndroid() && isARM) || mFeatures.supportsRasterizationOrderAttachmentAccess.enabled);

    // Important games are not checking supported extensions properly, and are confusing the
    // GL_EXT_shader_framebuffer_fetch_non_coherent as the GL_EXT_shader_framebuffer_fetch
    // extension.  Therefore, don't enable the extension on Arm and Qualcomm by default.
    // https://issuetracker.google.com/issues/186643966
    ANGLE_FEATURE_CONDITION(&mFeatures, supportsShaderFramebufferFetchNonCoherent,
                            (IsAndroid() && !(isARM || isQualcomm)) || isSwiftShader);

    // On tile-based renderers, breaking the render pass is costly.  Changing into and out of
    // framebuffer fetch causes the render pass to break so that the layout of the color attachments
    // can be adjusted.  On such hardware, the switch to framebuffer fetch mode is made permanent so
    // such render pass breaks don't happen.
    ANGLE_FEATURE_CONDITION(&mFeatures, permanentlySwitchToFramebufferFetchMode,
                            isTileBasedRenderer);

    // Support EGL_KHR_lock_surface3 extension.
    ANGLE_FEATURE_CONDITION(&mFeatures, supportsLockSurfaceExtension, IsAndroid());

    // http://anglebug.com/6878
    // Android needs swapbuffers to update image and present to display.
    ANGLE_FEATURE_CONDITION(&mFeatures, swapbuffersOnFlushOrFinishWithSingleBuffer, IsAndroid());

    // Applications on Android have come to rely on hardware dithering, and visually regress without
    // it.  On desktop GPUs, OpenGL's dithering is a no-op.  The following setting mimics that
    // behavior.  Dithering is also currently not enabled on SwiftShader, but can be as needed
    // (which would require Chromium and Capture/Replay test expectations updates).
    ANGLE_FEATURE_CONDITION(&mFeatures, emulateDithering, IsAndroid());

    // Workaround a Qualcomm imprecision with dithering
    ANGLE_FEATURE_CONDITION(&mFeatures, roundOutputAfterDithering, isQualcomm);

    // GL_KHR_blend_equation_advanced is emulated when the equivalent Vulkan extension is not
    // usable.  Additionally, the following platforms don't support INPUT_ATTACHMENT usage for the
    // swapchain, so they are excluded:
    //
    // - Intel
    //
    // Without VK_GOOGLE_surfaceless_query, there is no way to automatically deduce this support.
    ANGLE_FEATURE_CONDITION(&mFeatures, emulateAdvancedBlendEquations,
                            !mFeatures.supportsBlendOperationAdvanced.enabled && !isIntel);

    // Workaround for platforms that do not return 1.0f even when dividend and divisor have the same
    // value.
    ANGLE_FEATURE_CONDITION(&mFeatures, precisionSafeDivision, isSamsung || isAMD);

    // http://anglebug.com/6933
    // Android expects VkPresentRegionsKHR rectangles with a bottom-left origin, while spec
    // states they should have a top-left origin.
    ANGLE_FEATURE_CONDITION(&mFeatures, bottomLeftOriginPresentRegionRectangles, IsAndroid());

    // http://anglebug.com/7308
    // Flushing mutable textures causes flakes in perf tests using Windows/Intel GPU. Failures are
    // due to lost context/device.
    // http://b/264143971
    // The mutable texture uploading feature can sometimes result in incorrect rendering of some
    // textures.
    ANGLE_FEATURE_CONDITION(&mFeatures, mutableMipmapTextureUpload, false);

    // Retain debug info in SPIR-V blob.
    ANGLE_FEATURE_CONDITION(&mFeatures, retainSPIRVDebugInfo, getEnableValidationLayers());

    // For discrete GPUs, most of device local memory is host invisible. We should not force the
    // host visible flag for them and result in allocation failure.
    ANGLE_FEATURE_CONDITION(
        &mFeatures, preferDeviceLocalMemoryHostVisible,
        canPreferDeviceLocalMemoryHostVisible(mPhysicalDeviceProperties.deviceType));

    ANGLE_FEATURE_CONDITION(&mFeatures, supportsExtendedDynamicState,
                            mExtendedDynamicStateFeatures.extendedDynamicState == VK_TRUE);

    ANGLE_FEATURE_CONDITION(&mFeatures, supportsExtendedDynamicState2,
                            mExtendedDynamicState2Features.extendedDynamicState2 == VK_TRUE);

    // Disabled on Intel/Mesa due to driver bug (crbug.com/1379201).  This bug is fixed since Mesa
    // 22.2.0.
    const bool isAtLeastMesa22_2 =
        mesaVersion.major >= 22 || (mesaVersion.major == 22 && mesaVersion.minor >= 2);
    ANGLE_FEATURE_CONDITION(
        &mFeatures, supportsLogicOpDynamicState,
        mExtendedDynamicState2Features.extendedDynamicState2LogicOp == VK_TRUE &&
            (!(IsLinux() && isIntel) || isAtLeastMesa22_2));

    // Avoid dynamic state for vertex input binding stride on buggy drivers.
    ANGLE_FEATURE_CONDITION(&mFeatures, forceStaticVertexStrideState,
                            mFeatures.supportsExtendedDynamicState.enabled && isARM);

    // Support GL_QCOM_shading_rate extension
    ANGLE_FEATURE_CONDITION(&mFeatures, supportsFragmentShadingRate,
                            canSupportFragmentShadingRate(deviceExtensionNames));

    // We can use the interlock to support GL_ANGLE_shader_pixel_local_storage_coherent.
    ANGLE_FEATURE_CONDITION(
        &mFeatures, supportsFragmentShaderPixelInterlock,
        mFragmentShaderInterlockFeatures.fragmentShaderPixelInterlock == VK_TRUE);

    ANGLE_FEATURE_CONDITION(&mFeatures, supportsImagelessFramebuffer,
                            mImagelessFramebufferFeatures.imagelessFramebuffer == VK_TRUE);

    // The VK_PIPELINE_ROBUSTNESS_BUFFER_BEHAVIOR_ROBUST_BUFFER_ACCESS_EXT behavior is used by
    // ANGLE, which requires the robustBufferAccess feature to be available.
    ANGLE_FEATURE_CONDITION(&mFeatures, supportsPipelineRobustness,
                            mPipelineRobustnessFeatures.pipelineRobustness == VK_TRUE &&
                                mPhysicalDeviceFeatures.robustBufferAccess);

    ANGLE_FEATURE_CONDITION(&mFeatures, supportsPipelineProtectedAccess,
                            mPipelineProtectedAccessFeatures.pipelineProtectedAccess == VK_TRUE &&
                                mProtectedMemoryFeatures.protectedMemory == VK_TRUE);

    ANGLE_FEATURE_CONDITION(&mFeatures, supportsGraphicsPipelineLibrary,
                            mGraphicsPipelineLibraryFeatures.graphicsPipelineLibrary == VK_TRUE);

    // The following drivers are known to key the pipeline cache blobs with vertex input and
    // fragment output state, causing draw-time pipeline creation to miss the cache regardless of
    // warmup:
    //
    // - ARM drivers
    // - Imagination drivers
    //
    // The following drivers are instead known to _not_ include said state, and hit the cache at
    // draw time.
    //
    // - SwiftShader
    // - Open source Qualcomm drivers
    //
    // The situation is unknown for other drivers.
    //
    // Additionally, numerous tests that previously never created a Vulkan pipeline fail or crash on
    // proprietary Qualcomm drivers when they do during cache warm up.  On Intel/Linux, one trace
    // shows flakiness with this.
    const bool libraryBlobsAreReusedByMonolithicPipelines = !isARM && !isPowerVR;
    ANGLE_FEATURE_CONDITION(&mFeatures, warmUpPipelineCacheAtLink,
                            libraryBlobsAreReusedByMonolithicPipelines && !isQualcommProprietary &&
                                !(IsLinux() && isIntel) && !(IsChromeOS() && isSwiftShader));

    // On SwiftShader, no data is retrieved from the pipeline cache, so there is no reason to
    // serialize it or put it in the blob cache.
    ANGLE_FEATURE_CONDITION(&mFeatures, hasEffectivePipelineCacheSerialization, !isSwiftShader);

    // When the driver sets graphicsPipelineLibraryFastLinking, it means that monolithic pipelines
    // are just a bundle of the libraries, and that there is no benefit in creating monolithic
    // pipelines.
    //
    // Note: for testing purposes, this is enabled on SwiftShader despite the fact that it doesn't
    // need it.  This should be undone once there is at least one bot that supports
    // VK_EXT_graphics_pipeline_library without graphicsPipelineLibraryFastLinking
    ANGLE_FEATURE_CONDITION(
        &mFeatures, preferMonolithicPipelinesOverLibraries,
        !mGraphicsPipelineLibraryProperties.graphicsPipelineLibraryFastLinking || isSwiftShader);

    // Whether the pipeline caches should merge into the global pipeline cache.  This should only be
    // enabled on platforms if:
    //
    // - VK_EXT_graphics_pipeline_library is not supported.  In that case, only the program's cache
    //   used during warm up is merged into the global cache for later monolithic pipeline creation.
    // - VK_EXT_graphics_pipeline_library is supported, monolithic pipelines are preferred, and the
    //   driver is able to reuse blobs from partial pipelines when creating monolithic pipelines.
    ANGLE_FEATURE_CONDITION(&mFeatures, mergeProgramPipelineCachesToGlobalCache,
                            !mFeatures.supportsGraphicsPipelineLibrary.enabled ||
                                (mFeatures.preferMonolithicPipelinesOverLibraries.enabled &&
                                 libraryBlobsAreReusedByMonolithicPipelines));

    ANGLE_FEATURE_CONDITION(&mFeatures, enableAsyncPipelineCacheCompression, false);

    // Sync monolithic pipelines to the blob cache occasionally on platforms that would benefit from
    // it:
    //
    // - VK_EXT_graphics_pipeline_library is not supported, and the program cache is not warmed up:
    //   If the pipeline cache is being warmed up at link time, the blobs corresponding to each
    //   program is individually retrieved and stored in the blob cache already.
    // - VK_EXT_graphics_pipeline_library is supported, but monolithic pipelines are still prefered,
    //   and the cost of syncing the large cache is acceptable.
    //
    // Otherwise monolithic pipelines are recreated on every run.
    const bool hasNoPipelineWarmUp = !mFeatures.supportsGraphicsPipelineLibrary.enabled &&
                                     !mFeatures.warmUpPipelineCacheAtLink.enabled;
    const bool canSyncLargeMonolithicCache =
        mFeatures.supportsGraphicsPipelineLibrary.enabled &&
        mFeatures.preferMonolithicPipelinesOverLibraries.enabled &&
        (!IsAndroid() || mFeatures.enableAsyncPipelineCacheCompression.enabled);
    ANGLE_FEATURE_CONDITION(&mFeatures, syncMonolithicPipelinesToBlobCache,
                            mFeatures.hasEffectivePipelineCacheSerialization.enabled &&
                                (hasNoPipelineWarmUp || canSyncLargeMonolithicCache));

    // On ARM, dynamic state for stencil write mask doesn't work correctly in the presence of
    // discard or alpha to coverage, if the static state provided when creating the pipeline has a
    // value of 0.
    ANGLE_FEATURE_CONDITION(&mFeatures, useNonZeroStencilWriteMaskStaticState,
                            isARM && armDriverVersion < ARMDriverVersion(40, 0, 0));

    // On ARM, per-sample shading is not enabled despite the presence of a Sample decoration.  As a
    // workaround, per-sample shading is inferred by ANGLE and explicitly enabled by the API.
    ANGLE_FEATURE_CONDITION(&mFeatures, explicitlyEnablePerSampleShading, isARM);

    // Force to create swapchain with continuous refresh on shared present. Disabled by default.
    // Only enable it on integrations without EGL_FRONT_BUFFER_AUTO_REFRESH_ANDROID passthrough.
    ANGLE_FEATURE_CONDITION(&mFeatures, forceContinuousRefreshOnSharedPresent, false);

    // Enable setting frame timestamp surface attribute on Android platform.
    // Frame timestamp is enabled by calling into "vkGetPastPresentationTimingGOOGLE"
    // which, on Android platforms, makes the necessary ANativeWindow API calls.
    ANGLE_FEATURE_CONDITION(&mFeatures, supportsTimestampSurfaceAttribute,
                            IsAndroid() && ExtensionFound(VK_GOOGLE_DISPLAY_TIMING_EXTENSION_NAME,
                                                          deviceExtensionNames));

    // 1) host vk driver does not natively support ETC format.
    // 2) host vk driver supports BC format.
    // 3) host vk driver supports subgroup instructions: clustered, shuffle.
    //    * This limitation can be removed if necessary.
    // 4) host vk driver has maxTexelBufferSize >= 64M.
    //    * Usually on desktop device the limit is more than 128M. we may switch to dynamic
    //    decide cpu or gpu upload texture based on texture size.
    constexpr VkSubgroupFeatureFlags kRequiredSubgroupOp =
        VK_SUBGROUP_FEATURE_SHUFFLE_BIT | VK_SUBGROUP_FEATURE_CLUSTERED_BIT;
    static constexpr bool kSupportTranscodeEtcToBc = false;
    static constexpr uint32_t kMaxTexelBufferSize  = 64 * 1024 * 1024;
    const VkPhysicalDeviceLimits &limitsVk         = mPhysicalDeviceProperties.limits;
    ANGLE_FEATURE_CONDITION(&mFeatures, supportsComputeTranscodeEtcToBc,
                            !mPhysicalDeviceFeatures.textureCompressionETC2 &&
                                kSupportTranscodeEtcToBc &&
                                (mSubgroupProperties.supportedOperations & kRequiredSubgroupOp) ==
                                    kRequiredSubgroupOp &&
                                (limitsVk.maxTexelBufferElements >= kMaxTexelBufferSize));

    // Allow passthrough of EGL colorspace attributes on Android platform and for vendors that
    // are known to support wide color gamut.
    ANGLE_FEATURE_CONDITION(&mFeatures, eglColorspaceAttributePassthrough,
                            IsAndroid() && isSamsung);

    // GBM does not have a VkSurface hence it does not support presentation through a Vulkan queue.
    ANGLE_FEATURE_CONDITION(&mFeatures, supportsPresentation, !displayVk->isGBM());

    // For tiled renderer, the renderpass query result may not available until the entire renderpass
    // is completed. This may cause a bubble in the application thread waiting result to be
    // available. When this feature flag is enabled, we will issue an immediate flush when we detect
    // there is switch from query enabled draw to query disabled draw. Since most apps uses bunch of
    // query back to back, this should only introduce one extra flush per frame.
    // https://issuetracker.google.com/250706693
    ANGLE_FEATURE_CONDITION(&mFeatures, preferSubmitOnAnySamplesPassedQueryEnd,
                            isTileBasedRenderer);

    // ARM driver appears having a bug that if we did not wait for submission to complete, but call
    // vkGetQueryPoolResults(VK_QUERY_RESULT_WAIT_BIT), it may result VK_NOT_READY.
    // https://issuetracker.google.com/253522366
    //
    // Workaround for nvidia earlier version driver which appears having a bug that On older nvidia
    // driver, vkGetQueryPoolResult() with VK_QUERY_RESULT_WAIT_BIT may result in incorrect result.
    // In that case we force into CPU wait for submission to complete. http://anglebug.com/6692
    ANGLE_FEATURE_CONDITION(&mFeatures, forceWaitForSubmissionToCompleteForQueryResult,
                            isARM || (isNvidia && nvidiaVersion.major < 470u));

    ApplyFeatureOverrides(&mFeatures, displayVk->getState());

    // Disable async command queue when using Vulkan secondary command buffers temporarily to avoid
    // threading hazards with ContextVk::mCommandPools.  Note that this is done even if the feature
    // is enabled through an override.
    // TODO: Investigate whether async command queue is useful with Vulkan secondary command buffers
    // and enable the feature.  http://anglebug.com/6811
    if (!vk::OutsideRenderPassCommandBuffer::ExecutesInline() ||
        !vk::RenderPassCommandBuffer::ExecutesInline())
    {
        mFeatures.asyncCommandQueue.enabled = false;
    }
}

void RendererVk::appBasedFeatureOverrides(DisplayVk *display,
                                          const vk::ExtensionNameList &extensions)
{
    // NOOP for now.
}

angle::Result RendererVk::initPipelineCache(DisplayVk *display,
                                            vk::PipelineCache *pipelineCache,
                                            bool *success)
{
    angle::MemoryBuffer initialData;
    ANGLE_TRY(
        GetAndDecompressPipelineCacheVk(mPhysicalDeviceProperties, display, &initialData, success));

    VkPipelineCacheCreateInfo pipelineCacheCreateInfo = {};

    pipelineCacheCreateInfo.sType           = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
    pipelineCacheCreateInfo.flags           = 0;
    pipelineCacheCreateInfo.initialDataSize = *success ? initialData.size() : 0;
    pipelineCacheCreateInfo.pInitialData    = *success ? initialData.data() : nullptr;

    if (display->getRenderer()->getFeatures().supportsPipelineCreationCacheControl.enabled)
    {
        pipelineCacheCreateInfo.flags |= VK_PIPELINE_CACHE_CREATE_EXTERNALLY_SYNCHRONIZED_BIT_EXT;
    }

    ANGLE_VK_TRY(display, pipelineCache->init(mDevice, pipelineCacheCreateInfo));

    return angle::Result::Continue;
}

angle::Result RendererVk::getPipelineCache(vk::PipelineCacheAccess *pipelineCacheOut)
{
    DisplayVk *displayVk = vk::GetImpl(mDisplay);

    // Note that ANGLE externally synchronizes the pipeline cache, and uses
    // VK_EXT_pipeline_creation_cache_control (where available) to disable internal synchronization.
    std::unique_lock<std::mutex> lock(mPipelineCacheMutex);

    if (!mPipelineCacheInitialized)
    {
        // We should now recreate the pipeline cache with the blob cache pipeline data.
        vk::PipelineCache pCache;
        bool success = false;
        ANGLE_TRY(initPipelineCache(displayVk, &pCache, &success));
        if (success)
        {
            // Merge the newly created pipeline cache into the existing one.
            mPipelineCache.merge(mDevice, 1, pCache.ptr());
        }
        mPipelineCacheInitialized = true;
        pCache.destroy(mDevice);

        ANGLE_TRY(getPipelineCacheSize(displayVk, &mPipelineCacheSizeAtLastSync));
    }

    pipelineCacheOut->init(&mPipelineCache, &mPipelineCacheMutex);
    return angle::Result::Continue;
}

angle::Result RendererVk::mergeIntoPipelineCache(const vk::PipelineCache &pipelineCache)
{
    vk::PipelineCacheAccess globalCache;
    ANGLE_TRY(getPipelineCache(&globalCache));

    globalCache.merge(this, pipelineCache);

    return angle::Result::Continue;
}

const gl::Caps &RendererVk::getNativeCaps() const
{
    ensureCapsInitialized();
    return mNativeCaps;
}

const gl::TextureCapsMap &RendererVk::getNativeTextureCaps() const
{
    ensureCapsInitialized();
    return mNativeTextureCaps;
}

const gl::Extensions &RendererVk::getNativeExtensions() const
{
    ensureCapsInitialized();
    return mNativeExtensions;
}

const gl::Limitations &RendererVk::getNativeLimitations() const
{
    ensureCapsInitialized();
    return mNativeLimitations;
}

const ShPixelLocalStorageOptions &RendererVk::getNativePixelLocalStorageOptions() const
{
    return mNativePLSOptions;
}

void RendererVk::initializeFrontendFeatures(angle::FrontendFeatures *features) const
{
    const bool isSwiftShader =
        IsSwiftshader(mPhysicalDeviceProperties.vendorID, mPhysicalDeviceProperties.deviceID);

    // Hopefully-temporary work-around for a crash on SwiftShader.  An Android process is turning
    // off GL error checking, and then asking ANGLE to write past the end of a buffer.
    // https://issuetracker.google.com/issues/220069903
    ANGLE_FEATURE_CONDITION(features, forceGlErrorChecking, (IsAndroid() && isSwiftShader));

    ANGLE_FEATURE_CONDITION(features, cacheCompiledShader, true);
}

angle::Result RendererVk::getPipelineCacheSize(DisplayVk *displayVk, size_t *pipelineCacheSizeOut)
{
    VkResult result = mPipelineCache.getCacheData(mDevice, pipelineCacheSizeOut, nullptr);
    ANGLE_VK_TRY(displayVk, result);

    return angle::Result::Continue;
}

angle::Result RendererVk::syncPipelineCacheVk(DisplayVk *displayVk, const gl::Context *context)
{
    ASSERT(mPipelineCache.valid());

    if (!mFeatures.syncMonolithicPipelinesToBlobCache.enabled)
    {
        return angle::Result::Continue;
    }

    if (--mPipelineCacheVkUpdateTimeout > 0)
    {
        return angle::Result::Continue;
    }

    mPipelineCacheVkUpdateTimeout = kPipelineCacheVkUpdatePeriod;

    size_t pipelineCacheSize = 0;
    ANGLE_TRY(getPipelineCacheSize(displayVk, &pipelineCacheSize));
    if (pipelineCacheSize <= mPipelineCacheSizeAtLastSync)
    {
        return angle::Result::Continue;
    }
    mPipelineCacheSizeAtLastSync = pipelineCacheSize;

    // Make sure we will receive enough data to hold the pipeline cache header
    // Table 7. Layout for pipeline cache header version VK_PIPELINE_CACHE_HEADER_VERSION_ONE
    const size_t kPipelineCacheHeaderSize = 16 + VK_UUID_SIZE;
    if (pipelineCacheSize < kPipelineCacheHeaderSize)
    {
        // No pipeline cache data to read, so return
        return angle::Result::Continue;
    }

    ContextVk *contextVk = vk::GetImpl(context);

    // Use worker thread pool to complete compression.
    // If the last task hasn't been finished, skip the syncing.
    if (mCompressEvent && !mCompressEvent->isReady())
    {
        ANGLE_PERF_WARNING(contextVk->getDebug(), GL_DEBUG_SEVERITY_LOW,
                           "Skip syncing pipeline cache data when the last task is not ready.");
        return angle::Result::Continue;
    }

    std::vector<uint8_t> pipelineCacheData(pipelineCacheSize);

    size_t oldPipelineCacheSize = pipelineCacheSize;
    VkResult result =
        mPipelineCache.getCacheData(mDevice, &pipelineCacheSize, pipelineCacheData.data());
    // We don't need all of the cache data, so just make sure we at least got the header
    // Vulkan Spec 9.6. Pipeline Cache
    // https://www.khronos.org/registry/vulkan/specs/1.1-extensions/html/chap9.html#pipelines-cache
    // If pDataSize is less than what is necessary to store this header, nothing will be written to
    // pData and zero will be written to pDataSize.
    // Any data written to pData is valid and can be provided as the pInitialData member of the
    // VkPipelineCacheCreateInfo structure passed to vkCreatePipelineCache.
    if (ANGLE_UNLIKELY(pipelineCacheSize < kPipelineCacheHeaderSize))
    {
        WARN() << "Not enough pipeline cache data read.";
        return angle::Result::Continue;
    }
    else if (ANGLE_UNLIKELY(result == VK_INCOMPLETE))
    {
        WARN() << "Received VK_INCOMPLETE: Old: " << oldPipelineCacheSize
               << ", New: " << pipelineCacheSize;
    }
    else
    {
        ANGLE_VK_TRY(displayVk, result);
    }

    // If vkGetPipelineCacheData ends up writing fewer bytes than requested, zero out the rest of
    // the buffer to avoid leaking garbage memory.
    ASSERT(pipelineCacheSize <= pipelineCacheData.size());
    if (pipelineCacheSize < pipelineCacheData.size())
    {
        memset(pipelineCacheData.data() + pipelineCacheSize, 0,
               pipelineCacheData.size() - pipelineCacheSize);
    }

    if (mFeatures.enableAsyncPipelineCacheCompression.enabled)
    {
        // zlib compression ratio normally ranges from 2:1 to 5:1. Set kMaxTotalSize to 64M to
        // ensure the size can fit into the 32MB blob cache limit on supported platforms.
        constexpr size_t kMaxTotalSize = 64 * 1024 * 1024;

        // Create task to compress.
        auto compressAndStorePipelineCacheTask =
            std::make_shared<CompressAndStorePipelineCacheTask>(
                displayVk, contextVk, std::move(pipelineCacheData), kMaxTotalSize);
        mCompressEvent = std::make_shared<WaitableCompressEventImpl>(
            context->getWorkerThreadPool()->postWorkerTask(compressAndStorePipelineCacheTask),
            compressAndStorePipelineCacheTask);
    }
    else
    {
        // If enableAsyncPipelineCacheCompression is disabled, to avoid the risk, set kMaxTotalSize
        // to 64k.
        constexpr size_t kMaxTotalSize = 64 * 1024;
        CompressAndStorePipelineCacheVk(mPhysicalDeviceProperties, displayVk, contextVk,
                                        pipelineCacheData, kMaxTotalSize);
    }

    return angle::Result::Continue;
}

// These functions look at the mandatory format for support, and fallback to querying the device (if
// necessary) to test the availability of the bits.
bool RendererVk::hasLinearImageFormatFeatureBits(angle::FormatID formatID,
                                                 const VkFormatFeatureFlags featureBits) const
{
    return hasFormatFeatureBits<&VkFormatProperties::linearTilingFeatures>(formatID, featureBits);
}

VkFormatFeatureFlags RendererVk::getLinearImageFormatFeatureBits(
    angle::FormatID formatID,
    const VkFormatFeatureFlags featureBits) const
{
    return getFormatFeatureBits<&VkFormatProperties::linearTilingFeatures>(formatID, featureBits);
}

VkFormatFeatureFlags RendererVk::getImageFormatFeatureBits(
    angle::FormatID formatID,
    const VkFormatFeatureFlags featureBits) const
{
    return getFormatFeatureBits<&VkFormatProperties::optimalTilingFeatures>(formatID, featureBits);
}

bool RendererVk::hasImageFormatFeatureBits(angle::FormatID formatID,
                                           const VkFormatFeatureFlags featureBits) const
{
    return hasFormatFeatureBits<&VkFormatProperties::optimalTilingFeatures>(formatID, featureBits);
}

bool RendererVk::hasBufferFormatFeatureBits(angle::FormatID formatID,
                                            const VkFormatFeatureFlags featureBits) const
{
    return hasFormatFeatureBits<&VkFormatProperties::bufferFeatures>(formatID, featureBits);
}

void RendererVk::outputVmaStatString()
{
    // Output the VMA stats string
    // This JSON string can be passed to VmaDumpVis.py to generate a visualization of the
    // allocations the VMA has performed.
    char *statsString;
    mAllocator.buildStatsString(&statsString, true);
    INFO() << std::endl << statsString << std::endl;
    mAllocator.freeStatsString(statsString);
}

angle::Result RendererVk::queueSubmitOneOff(vk::Context *context,
                                            vk::PrimaryCommandBuffer &&primary,
                                            bool hasProtectedContent,
                                            egl::ContextPriority priority,
                                            const vk::Semaphore *waitSemaphore,
                                            VkPipelineStageFlags waitSemaphoreStageMasks,
                                            const vk::Fence *fence,
                                            vk::SubmitPolicy submitPolicy,
                                            QueueSerial *queueSerialOut)
{
    ANGLE_TRACE_EVENT0("gpu.angle", "RendererVk::queueSubmitOneOff");
    // Allocate a oneoff submitQueueSerial and generate a serial and then use it and release the
    // index.
    SerialIndex queueIndex;
    Serial lastSubmittedSerial;
    ANGLE_TRY(allocateQueueSerialIndex(&queueIndex, &lastSubmittedSerial));
    QueueSerial submitQueueSerial(queueIndex, generateQueueSerial(queueIndex));

    if (isAsyncCommandQueueEnabled())
    {
        ANGLE_TRY(mCommandProcessor.queueSubmitOneOff(
            context, hasProtectedContent, priority, primary.getHandle(), waitSemaphore,
            waitSemaphoreStageMasks, fence, submitPolicy, submitQueueSerial));
    }
    else
    {
        ANGLE_TRY(mCommandQueue.queueSubmitOneOff(
            context, hasProtectedContent, priority, primary.getHandle(), waitSemaphore,
            waitSemaphoreStageMasks, fence, submitPolicy, submitQueueSerial));
    }

    // Immediately release the queue index since itis an one off use.
    releaseQueueSerialIndex(queueIndex);

    *queueSerialOut = submitQueueSerial;
    if (primary.valid())
    {
        mOneOffCommandPool.releaseCommandBuffer(submitQueueSerial, std::move(primary));
    }

    return angle::Result::Continue;
}

template <VkFormatFeatureFlags VkFormatProperties::*features>
VkFormatFeatureFlags RendererVk::getFormatFeatureBits(angle::FormatID formatID,
                                                      const VkFormatFeatureFlags featureBits) const
{
    ASSERT(formatID != angle::FormatID::NONE);
    VkFormatProperties &deviceProperties = mFormatProperties[formatID];

    if (deviceProperties.bufferFeatures == kInvalidFormatFeatureFlags)
    {
        // If we don't have the actual device features, see if the requested features are mandatory.
        // If so, there's no need to query the device.
        const VkFormatProperties &mandatoryProperties = vk::GetMandatoryFormatSupport(formatID);
        if (IsMaskFlagSet(mandatoryProperties.*features, featureBits))
        {
            return featureBits;
        }

        VkFormat vkFormat = vk::GetVkFormatFromFormatID(formatID);
        ASSERT(vkFormat != VK_FORMAT_UNDEFINED);

        // Otherwise query the format features and cache it.
        vkGetPhysicalDeviceFormatProperties(mPhysicalDevice, vkFormat, &deviceProperties);
        // Workaround for some Android devices that don't indicate filtering
        // support on D16_UNORM and they should.
        if (mFeatures.forceD16TexFilter.enabled && vkFormat == VK_FORMAT_D16_UNORM)
        {
            deviceProperties.*features |= VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT;
        }
    }

    return deviceProperties.*features & featureBits;
}

template <VkFormatFeatureFlags VkFormatProperties::*features>
bool RendererVk::hasFormatFeatureBits(angle::FormatID formatID,
                                      const VkFormatFeatureFlags featureBits) const
{
    return IsMaskFlagSet(getFormatFeatureBits<features>(formatID, featureBits), featureBits);
}

bool RendererVk::haveSameFormatFeatureBits(angle::FormatID formatID1,
                                           angle::FormatID formatID2) const
{
    if (formatID1 == angle::FormatID::NONE || formatID2 == angle::FormatID::NONE)
    {
        return false;
    }

    constexpr VkFormatFeatureFlags kImageUsageFeatureBits =
        VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT |
        VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT;

    VkFormatFeatureFlags fmt1LinearFeatureBits =
        getLinearImageFormatFeatureBits(formatID1, kImageUsageFeatureBits);
    VkFormatFeatureFlags fmt1OptimalFeatureBits =
        getImageFormatFeatureBits(formatID1, kImageUsageFeatureBits);

    return hasLinearImageFormatFeatureBits(formatID2, fmt1LinearFeatureBits) &&
           hasImageFormatFeatureBits(formatID2, fmt1OptimalFeatureBits);
}

void RendererVk::addBufferBlockToOrphanList(vk::BufferBlock *block)
{
    std::unique_lock<std::mutex> lock(mGarbageMutex);
    mOrphanedBufferBlocks.emplace_back(block);
}

void RendererVk::pruneOrphanedBufferBlocks()
{
    for (auto iter = mOrphanedBufferBlocks.begin(); iter != mOrphanedBufferBlocks.end();)
    {
        if (!(*iter)->isEmpty())
        {
            ++iter;
            continue;
        }
        (*iter)->destroy(this);
        iter = mOrphanedBufferBlocks.erase(iter);
    }
}

void RendererVk::cleanupGarbage()
{
    std::unique_lock<std::mutex> lock(mGarbageMutex);

    // Clean up general garbages
    while (!mSharedGarbage.empty())
    {
        vk::SharedGarbage &garbage = mSharedGarbage.front();
        if (!garbage.destroyIfComplete(this))
        {
            break;
        }
        mSharedGarbage.pop();
    }

    // Clean up suballocation garbages
    VkDeviceSize suballocationBytesDestroyed = 0;
    while (!mSuballocationGarbage.empty())
    {
        vk::SharedBufferSuballocationGarbage &garbage = mSuballocationGarbage.front();
        VkDeviceSize garbageSize                      = garbage.getSize();
        if (!garbage.destroyIfComplete(this))
        {
            break;
        }
        // Actually destroyed.
        mSuballocationGarbage.pop();
        suballocationBytesDestroyed += garbageSize;
    }
    mSuballocationGarbageDestroyed += suballocationBytesDestroyed;
    mSuballocationGarbageSizeInBytes -= suballocationBytesDestroyed;

    // Note: do this after clean up mSuballocationGarbage so that we will have more chances to find
    // orphaned blocks being empty.
    if (!mOrphanedBufferBlocks.empty())
    {
        pruneOrphanedBufferBlocks();
    }

    // Cache the value with atomic variable for access without mGarbageMutex lock.
    mSuballocationGarbageSizeInBytesCachedAtomic.store(mSuballocationGarbageSizeInBytes,
                                                       std::memory_order_release);
}

void RendererVk::cleanupPendingSubmissionGarbage()
{
    std::unique_lock<std::mutex> lock(mGarbageMutex);

    // Check if pending garbage is still pending. If not, move them to the garbage list.
    vk::SharedGarbageList pendingGarbage;
    while (!mPendingSubmissionGarbage.empty())
    {
        vk::SharedGarbage &garbage = mPendingSubmissionGarbage.front();
        if (!garbage.hasUnsubmittedUse(this))
        {
            mSharedGarbage.push(std::move(garbage));
        }
        else
        {
            pendingGarbage.push(std::move(garbage));
        }
        mPendingSubmissionGarbage.pop();
    }
    if (!pendingGarbage.empty())
    {
        mPendingSubmissionGarbage = std::move(pendingGarbage);
    }

    vk::SharedBufferSuballocationGarbageList pendingSuballocationGarbage;
    while (!mPendingSubmissionSuballocationGarbage.empty())
    {
        vk::SharedBufferSuballocationGarbage &suballocationGarbage =
            mPendingSubmissionSuballocationGarbage.front();
        if (!suballocationGarbage.hasUnsubmittedUse(this))
        {
            mSuballocationGarbageSizeInBytes += suballocationGarbage.getSize();
            mSuballocationGarbage.push(std::move(suballocationGarbage));
        }
        else
        {
            pendingSuballocationGarbage.push(std::move(suballocationGarbage));
        }
        mPendingSubmissionSuballocationGarbage.pop();
    }
    if (!pendingSuballocationGarbage.empty())
    {
        mPendingSubmissionSuballocationGarbage = std::move(pendingSuballocationGarbage);
    }
}

void RendererVk::onNewValidationMessage(const std::string &message)
{
    mLastValidationMessage = message;
    ++mValidationMessageCount;
}

void RendererVk::onFramebufferFetchUsed()
{
    mIsFramebufferFetchUsed = true;
}

std::string RendererVk::getAndClearLastValidationMessage(uint32_t *countSinceLastClear)
{
    *countSinceLastClear    = mValidationMessageCount;
    mValidationMessageCount = 0;

    return std::move(mLastValidationMessage);
}

uint64_t RendererVk::getMaxFenceWaitTimeNs() const
{
    constexpr uint64_t kMaxFenceWaitTimeNs = 120'000'000'000llu;

    return kMaxFenceWaitTimeNs;
}

void RendererVk::setGlobalDebugAnnotator()
{
    // Install one of two DebugAnnotator classes:
    //
    // 1) The global class enables basic ANGLE debug functionality (e.g. Vulkan validation errors
    //    will cause dEQP tests to fail).
    //
    // 2) The DebugAnnotatorVk class processes OpenGL ES commands that the application uses.  It is
    //    installed for the following purposes:
    //
    //    1) To enable calling the vkCmd*DebugUtilsLabelEXT functions in order to communicate to
    //       debuggers (e.g. AGI) the OpenGL ES commands that the application uses.  In addition to
    //       simply installing DebugAnnotatorVk, also enable calling vkCmd*DebugUtilsLabelEXT.
    //
    //    2) To enable logging to Android logcat the OpenGL ES commands that the application uses.
    bool installDebugAnnotatorVk = false;

    // Enable calling the vkCmd*DebugUtilsLabelEXT functions if the vkCmd*DebugUtilsLabelEXT
    // functions exist, and if the kEnableDebugMarkersVarName environment variable is set.
    if (vkCmdBeginDebugUtilsLabelEXT)
    {
        // Use the GetAndSet variant to improve future lookup times
        std::string enabled = angle::GetAndSetEnvironmentVarOrUnCachedAndroidProperty(
            kEnableDebugMarkersVarName, kEnableDebugMarkersPropertyName);
        if (!enabled.empty() && enabled.compare("0") != 0)
        {
            mAngleDebuggerMode      = true;
            installDebugAnnotatorVk = true;
        }
    }
#if defined(ANGLE_ENABLE_TRACE_ANDROID_LOGCAT)
    // Only install DebugAnnotatorVk to log all API commands to Android's logcat.
    installDebugAnnotatorVk = true;
#endif

    {
        std::unique_lock<std::mutex> lock(gl::GetDebugMutex());
        if (installDebugAnnotatorVk)
        {
            gl::InitializeDebugAnnotations(&mAnnotator);
        }
        else
        {
            mDisplay->setGlobalDebugAnnotator();
        }
    }
}

void RendererVk::reloadVolkIfNeeded() const
{
#if defined(ANGLE_SHARED_LIBVULKAN)
    if ((mInstance != VK_NULL_HANDLE) && (volkGetLoadedInstance() != mInstance))
    {
        volkLoadInstance(mInstance);
    }

    if ((mDevice != VK_NULL_HANDLE) && (volkGetLoadedDevice() != mDevice))
    {
        volkLoadDevice(mDevice);
    }
#endif  // defined(ANGLE_SHARED_LIBVULKAN)
}

angle::Result RendererVk::submitCommands(
    vk::Context *context,
    bool hasProtectedContent,
    egl::ContextPriority contextPriority,
    std::vector<VkSemaphore> &&waitSemaphores,
    std::vector<VkPipelineStageFlags> &&waitSemaphoreStageMasks,
    const vk::Semaphore *signalSemaphore,
    vk::SecondaryCommandPools *commandPools,
    const QueueSerial &submitQueueSerial)
{
    vk::SecondaryCommandBufferList commandBuffersToReset;
    mOutsideRenderPassCommandBufferRecycler.releaseCommandBuffersToReset(
        &commandBuffersToReset.outsideRenderPassCommandBuffers);
    mRenderPassCommandBufferRecycler.releaseCommandBuffersToReset(
        &commandBuffersToReset.renderPassCommandBuffers);

    const VkSemaphore signalVkSemaphore =
        signalSemaphore ? signalSemaphore->getHandle() : VK_NULL_HANDLE;

    if (isAsyncCommandQueueEnabled())
    {
        ANGLE_TRY(mCommandProcessor.submitCommands(
            context, hasProtectedContent, contextPriority, waitSemaphores, waitSemaphoreStageMasks,
            signalVkSemaphore, std::move(commandBuffersToReset), commandPools, submitQueueSerial));
    }
    else
    {
        ANGLE_TRY(mCommandQueue.submitCommands(
            context, hasProtectedContent, contextPriority, waitSemaphores, waitSemaphoreStageMasks,
            signalVkSemaphore, std::move(commandBuffersToReset), commandPools, submitQueueSerial));
    }

    waitSemaphores.clear();
    waitSemaphoreStageMasks.clear();

    return angle::Result::Continue;
}

void RendererVk::handleDeviceLost()
{
    if (isAsyncCommandQueueEnabled())
    {
        mCommandProcessor.handleDeviceLost(this);
    }
    else
    {
        mCommandQueue.handleDeviceLost(this);
    }
}

angle::Result RendererVk::finishResourceUse(vk::Context *context, const vk::ResourceUse &use)
{
    if (isAsyncCommandQueueEnabled())
    {
        ANGLE_TRY(mCommandProcessor.waitForResourceUseToBeSubmitted(context, use));
    }
    return mCommandQueue.finishResourceUse(context, use, getMaxFenceWaitTimeNs());
}

angle::Result RendererVk::finishQueueSerial(vk::Context *context, const QueueSerial &queueSerial)
{
    ASSERT(queueSerial.valid());
    if (isAsyncCommandQueueEnabled())
    {
        ANGLE_TRY(mCommandProcessor.waitForQueueSerialToBeSubmitted(context, queueSerial));
    }
    return mCommandQueue.finishQueueSerial(context, queueSerial, getMaxFenceWaitTimeNs());
}

angle::Result RendererVk::waitForResourceUseToFinishWithUserTimeout(vk::Context *context,
                                                                    const vk::ResourceUse &use,
                                                                    uint64_t timeout,
                                                                    VkResult *result)
{
    ANGLE_TRACE_EVENT0("gpu.angle", "RendererVk::waitForResourceUseToFinishWithUserTimeout");
    if (isAsyncCommandQueueEnabled())
    {
        ANGLE_TRY(mCommandProcessor.waitForResourceUseToBeSubmitted(context, use));
    }
    return mCommandQueue.waitForResourceUseToFinishWithUserTimeout(context, use, timeout, result);
}

angle::Result RendererVk::finish(vk::Context *context, bool hasProtectedContent)
{
    if (isAsyncCommandQueueEnabled())
    {
        ANGLE_TRY(mCommandProcessor.waitForAllWorkToBeSubmitted(context));
    }
    return mCommandQueue.waitIdle(context, getMaxFenceWaitTimeNs());
}

angle::Result RendererVk::checkCompletedCommands(vk::Context *context)
{
    return mCommandQueue.checkCompletedCommands(context);
}

angle::Result RendererVk::flushRenderPassCommands(
    vk::Context *context,
    bool hasProtectedContent,
    const vk::RenderPass &renderPass,
    vk::RenderPassCommandBufferHelper **renderPassCommands)
{
    ANGLE_TRACE_EVENT0("gpu.angle", "RendererVk::flushRenderPassCommands");
    if (isAsyncCommandQueueEnabled())
    {
        ANGLE_TRY(mCommandProcessor.flushRenderPassCommands(context, hasProtectedContent,
                                                            renderPass, renderPassCommands));
    }
    else
    {
        ANGLE_TRY(mCommandQueue.flushRenderPassCommands(context, hasProtectedContent, renderPass,
                                                        renderPassCommands));
    }

    return angle::Result::Continue;
}

angle::Result RendererVk::flushOutsideRPCommands(
    vk::Context *context,
    bool hasProtectedContent,
    vk::OutsideRenderPassCommandBufferHelper **outsideRPCommands)
{
    ANGLE_TRACE_EVENT0("gpu.angle", "RendererVk::flushOutsideRPCommands");
    if (isAsyncCommandQueueEnabled())
    {
        ANGLE_TRY(mCommandProcessor.flushOutsideRPCommands(context, hasProtectedContent,
                                                           outsideRPCommands));
    }
    else
    {
        ANGLE_TRY(
            mCommandQueue.flushOutsideRPCommands(context, hasProtectedContent, outsideRPCommands));
    }

    return angle::Result::Continue;
}

VkResult RendererVk::queuePresent(vk::Context *context,
                                  egl::ContextPriority priority,
                                  const VkPresentInfoKHR &presentInfo)
{
    VkResult result = VK_SUCCESS;
    if (isAsyncCommandQueueEnabled())
    {
        result = mCommandProcessor.queuePresent(priority, presentInfo);
    }
    else
    {
        result = mCommandQueue.queuePresent(priority, presentInfo);
    }

    if (getFeatures().logMemoryReportStats.enabled)
    {
        mMemoryReport.logMemoryReportStats();
    }

    return result;
}

template <typename CommandBufferHelperT, typename RecyclerT>
angle::Result RendererVk::getCommandBufferImpl(
    vk::Context *context,
    vk::CommandPool *commandPool,
    vk::SecondaryCommandMemoryAllocator *commandsAllocator,
    RecyclerT *recycler,
    CommandBufferHelperT **commandBufferHelperOut)
{
    return recycler->getCommandBufferHelper(context, commandPool, commandsAllocator,
                                            commandBufferHelperOut);
}

angle::Result RendererVk::getOutsideRenderPassCommandBufferHelper(
    vk::Context *context,
    vk::CommandPool *commandPool,
    vk::SecondaryCommandMemoryAllocator *commandsAllocator,
    vk::OutsideRenderPassCommandBufferHelper **commandBufferHelperOut)
{
    ANGLE_TRACE_EVENT0("gpu.angle", "RendererVk::getOutsideRenderPassCommandBufferHelper");
    return getCommandBufferImpl(context, commandPool, commandsAllocator,
                                &mOutsideRenderPassCommandBufferRecycler, commandBufferHelperOut);
}

angle::Result RendererVk::getRenderPassCommandBufferHelper(
    vk::Context *context,
    vk::CommandPool *commandPool,
    vk::SecondaryCommandMemoryAllocator *commandsAllocator,
    vk::RenderPassCommandBufferHelper **commandBufferHelperOut)
{
    ANGLE_TRACE_EVENT0("gpu.angle", "RendererVk::getRenderPassCommandBufferHelper");
    return getCommandBufferImpl(context, commandPool, commandsAllocator,
                                &mRenderPassCommandBufferRecycler, commandBufferHelperOut);
}

void RendererVk::recycleOutsideRenderPassCommandBufferHelper(
    VkDevice device,
    vk::OutsideRenderPassCommandBufferHelper **commandBuffer)
{
    ANGLE_TRACE_EVENT0("gpu.angle", "RendererVk::recycleOutsideRenderPassCommandBufferHelper");
    mOutsideRenderPassCommandBufferRecycler.recycleCommandBufferHelper(device, commandBuffer);
}

void RendererVk::recycleRenderPassCommandBufferHelper(
    VkDevice device,
    vk::RenderPassCommandBufferHelper **commandBuffer)
{
    ANGLE_TRACE_EVENT0("gpu.angle", "RendererVk::recycleRenderPassCommandBufferHelper");
    mRenderPassCommandBufferRecycler.recycleCommandBufferHelper(device, commandBuffer);
}

void RendererVk::logCacheStats() const
{
    if (!vk::kOutputCumulativePerfCounters)
    {
        return;
    }

    std::unique_lock<std::mutex> localLock(mCacheStatsMutex);

    int cacheType = 0;
    INFO() << "Vulkan object cache hit ratios: ";
    for (const CacheStats &stats : mVulkanCacheStats)
    {
        INFO() << "    CacheType " << cacheType++ << ": " << stats.getHitRatio();
    }
}

void RendererVk::logMemoryStatsOnError()
{
    checkForCurrentMemoryAllocations(this);
    logPendingMemoryAllocation(this);
    logMemoryHeapStats(this, vk::MemoryLogSeverity::WARN);
}

angle::Result RendererVk::getFormatDescriptorCountForVkFormat(ContextVk *contextVk,
                                                              VkFormat format,
                                                              uint32_t *descriptorCountOut)
{
    if (mVkFormatDescriptorCountMap.count(format) == 0)
    {
        // Query device for descriptor count with basic values for most of
        // VkPhysicalDeviceImageFormatInfo2 members.
        VkPhysicalDeviceImageFormatInfo2 imageFormatInfo = {};
        imageFormatInfo.sType  = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2;
        imageFormatInfo.format = format;
        imageFormatInfo.type   = VK_IMAGE_TYPE_2D;
        imageFormatInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageFormatInfo.usage  = VK_IMAGE_USAGE_SAMPLED_BIT;
        imageFormatInfo.flags  = 0;

        VkImageFormatProperties imageFormatProperties                            = {};
        VkSamplerYcbcrConversionImageFormatProperties ycbcrImageFormatProperties = {};
        ycbcrImageFormatProperties.sType =
            VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_IMAGE_FORMAT_PROPERTIES;

        VkImageFormatProperties2 imageFormatProperties2 = {};
        imageFormatProperties2.sType                 = VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2;
        imageFormatProperties2.pNext                 = &ycbcrImageFormatProperties;
        imageFormatProperties2.imageFormatProperties = imageFormatProperties;

        ANGLE_VK_TRY(contextVk, vkGetPhysicalDeviceImageFormatProperties2(
                                    mPhysicalDevice, &imageFormatInfo, &imageFormatProperties2));

        mVkFormatDescriptorCountMap[format] =
            ycbcrImageFormatProperties.combinedImageSamplerDescriptorCount;
    }

    ASSERT(descriptorCountOut);
    *descriptorCountOut = mVkFormatDescriptorCountMap[format];
    return angle::Result::Continue;
}

angle::Result RendererVk::getFormatDescriptorCountForExternalFormat(ContextVk *contextVk,
                                                                    uint64_t format,
                                                                    uint32_t *descriptorCountOut)
{
    ASSERT(descriptorCountOut);

    // TODO: need to query for external formats as well once spec is fixed. http://anglebug.com/6141
    ANGLE_VK_CHECK(contextVk, getFeatures().useMultipleDescriptorsForExternalFormats.enabled,
                   VK_ERROR_INCOMPATIBLE_DRIVER);

    // Vulkan spec has a gap in that there is no mechanism available to query the immutable
    // sampler descriptor count of an external format. For now, return a default value.
    constexpr uint32_t kExternalFormatDefaultDescriptorCount = 4;
    *descriptorCountOut = kExternalFormatDefaultDescriptorCount;
    return angle::Result::Continue;
}

void RendererVk::onAllocateHandle(vk::HandleType handleType)
{
    std::unique_lock<std::mutex> localLock(mActiveHandleCountsMutex);
    mActiveHandleCounts.onAllocate(handleType);
}

void RendererVk::onDeallocateHandle(vk::HandleType handleType)
{
    std::unique_lock<std::mutex> localLock(mActiveHandleCountsMutex);
    mActiveHandleCounts.onDeallocate(handleType);
}

VkDeviceSize RendererVk::getPreferedBufferBlockSize(uint32_t memoryTypeIndex) const
{
    // Try not to exceed 1/64 of heap size to begin with.
    const VkDeviceSize heapSize = getMemoryProperties().getHeapSizeForMemoryType(memoryTypeIndex);
    return std::min(heapSize / 64, mPreferredLargeHeapBlockSize);
}

angle::Result RendererVk::allocateQueueSerialIndex(SerialIndex *indexOut, Serial *serialOut)
{
    SerialIndex index = mQueueSerialIndexAllocator.allocate();
    if (index == kInvalidQueueSerialIndex)
    {
        return angle::Result::Stop;
    }
    *indexOut  = index;
    *serialOut = isAsyncCommandQueueEnabled() ? mCommandProcessor.getLastSubmittedSerial(index)
                                              : mCommandQueue.getLastSubmittedSerial(index);
    return angle::Result::Continue;
}

void RendererVk::releaseQueueSerialIndex(SerialIndex index)
{
    mQueueSerialIndexAllocator.release(index);
}

MemoryAllocationTracker::MemoryAllocationTracker(RendererVk *renderer)
    : mRenderer(renderer), mMemoryAllocationID(0)
{}

void MemoryAllocationTracker::initMemoryTrackers()
{
    // Allocation counters are initialized here to keep track of the size and count of the memory
    // allocations.
    for (uint32_t i = 0; i < mActiveMemoryAllocationsSize.size(); i++)
    {
        mActiveMemoryAllocationsSize[i]  = 0;
        mActiveMemoryAllocationsCount[i] = 0;
    }

    // Per-heap allocation counters are initialized here.
    for (size_t i = 0; i < vk::kMemoryAllocationTypeCount; i++)
    {
        mActivePerHeapMemoryAllocationsSize[i].resize(
            mRenderer->getMemoryProperties().getMemoryHeapCount());
        mActivePerHeapMemoryAllocationsCount[i].resize(
            mRenderer->getMemoryProperties().getMemoryHeapCount());
    }

    resetPendingMemoryAlloc();
}

void MemoryAllocationTracker::onMemoryAllocImpl(vk::MemoryAllocationType allocType,
                                                VkDeviceSize size,
                                                uint32_t memoryTypeIndex,
                                                void *handle)
{
    ASSERT(allocType != vk::MemoryAllocationType::InvalidEnum && size != 0);

    if (kTrackMemoryAllocationDebug)
    {
        // If enabled (debug layers), we keep more details in the memory tracker, such as handle,
        // and log the action to the output.
        std::unique_lock<std::mutex> lock(mMemoryAllocationMutex);

        uint32_t allocTypeIndex = ToUnderlying(allocType);
        uint32_t memoryHeapIndex =
            mRenderer->getMemoryProperties().getHeapIndexForMemoryType(memoryTypeIndex);
        mActiveMemoryAllocationsCount[allocTypeIndex]++;
        mActiveMemoryAllocationsSize[allocTypeIndex] += size;
        mActivePerHeapMemoryAllocationsCount[allocTypeIndex][memoryHeapIndex]++;
        mActivePerHeapMemoryAllocationsSize[allocTypeIndex][memoryHeapIndex] += size;

        // Add the new allocation to the memory tracker.
        vk::MemoryAllocationInfo memAllocLogInfo;
        memAllocLogInfo.id              = ++mMemoryAllocationID;
        memAllocLogInfo.allocType       = allocType;
        memAllocLogInfo.memoryHeapIndex = memoryHeapIndex;
        memAllocLogInfo.size            = size;
        memAllocLogInfo.handle          = handle;

        vk::MemoryAllocInfoMapKey memoryAllocInfoMapKey(memAllocLogInfo.handle);
        mMemoryAllocationRecord[angle::getBacktraceInfo()].insert(
            std::make_pair(memoryAllocInfoMapKey, memAllocLogInfo));

        INFO() << "Memory allocation: (id " << memAllocLogInfo.id << ") for object "
               << memAllocLogInfo.handle << " | Size: " << memAllocLogInfo.size
               << " | Type: " << vk::kMemoryAllocationTypeMessage[allocTypeIndex]
               << " | Heap index: " << memAllocLogInfo.memoryHeapIndex;

        resetPendingMemoryAlloc();
    }
    else if (kTrackMemoryAllocationSizes)
    {
        // Add the new allocation size to the allocation counter.
        uint32_t allocTypeIndex = ToUnderlying(allocType);
        mActiveMemoryAllocationsSize[allocTypeIndex] += size;

        {
            std::unique_lock<std::mutex> lock(mMemoryAllocationMutex);
            uint32_t memoryHeapIndex =
                mRenderer->getMemoryProperties().getHeapIndexForMemoryType(memoryTypeIndex);
            mActivePerHeapMemoryAllocationsSize[allocTypeIndex][memoryHeapIndex] += size;
        }

        resetPendingMemoryAlloc();
    }
}

void MemoryAllocationTracker::onMemoryDeallocImpl(vk::MemoryAllocationType allocType,
                                                  VkDeviceSize size,
                                                  uint32_t memoryTypeIndex,
                                                  void *handle)
{
    ASSERT(allocType != vk::MemoryAllocationType::InvalidEnum && size != 0);

    if (kTrackMemoryAllocationDebug)
    {
        // If enabled (debug layers), we keep more details in the memory tracker, such as handle,
        // and log the action to the output. The memory allocation tracker uses the backtrace info
        // as key, if available.
        for (auto &memInfoPerBacktrace : mMemoryAllocationRecord)
        {
            vk::MemoryAllocInfoMapKey memoryAllocInfoMapKey(handle);
            MemoryAllocInfoMap &memInfoMap = memInfoPerBacktrace.second;
            std::unique_lock<std::mutex> lock(mMemoryAllocationMutex);

            if (memInfoMap.find(memoryAllocInfoMapKey) != memInfoMap.end())
            {
                // Object found; remove it from the allocation tracker.
                vk::MemoryAllocationInfo *memInfoEntry = &memInfoMap[memoryAllocInfoMapKey];
                ASSERT(memInfoEntry->allocType == allocType && memInfoEntry->size == size);

                uint32_t allocTypeIndex = ToUnderlying(memInfoEntry->allocType);
                uint32_t memoryHeapIndex =
                    mRenderer->getMemoryProperties().getHeapIndexForMemoryType(memoryTypeIndex);
                ASSERT(mActiveMemoryAllocationsCount[allocTypeIndex] != 0 &&
                       mActiveMemoryAllocationsSize[allocTypeIndex] >= size);
                ASSERT(memoryHeapIndex == memInfoEntry->memoryHeapIndex &&
                       mActivePerHeapMemoryAllocationsCount[allocTypeIndex][memoryHeapIndex] != 0 &&
                       mActivePerHeapMemoryAllocationsSize[allocTypeIndex][memoryHeapIndex] >=
                           size);
                mActiveMemoryAllocationsCount[allocTypeIndex]--;
                mActiveMemoryAllocationsSize[allocTypeIndex] -= size;
                mActivePerHeapMemoryAllocationsCount[allocTypeIndex][memoryHeapIndex]--;
                mActivePerHeapMemoryAllocationsSize[allocTypeIndex][memoryHeapIndex] -= size;

                INFO() << "Memory deallocation: (id " << memInfoEntry->id << ") for object "
                       << memInfoEntry->handle << " | Size: " << memInfoEntry->size
                       << " | Type: " << vk::kMemoryAllocationTypeMessage[allocTypeIndex]
                       << " | Heap index: " << memInfoEntry->memoryHeapIndex;

                memInfoMap.erase(memoryAllocInfoMapKey);
            }
        }
    }
    else if (kTrackMemoryAllocationSizes)
    {
        // Remove the allocation size from the allocation counter.
        uint32_t allocTypeIndex = ToUnderlying(allocType);
        ASSERT(mActiveMemoryAllocationsSize[allocTypeIndex] >= size);
        mActiveMemoryAllocationsSize[allocTypeIndex] -= size;

        {
            std::unique_lock<std::mutex> lock(mMemoryAllocationMutex);
            uint32_t memoryHeapIndex =
                mRenderer->getMemoryProperties().getHeapIndexForMemoryType(memoryTypeIndex);
            ASSERT(mActivePerHeapMemoryAllocationsSize[allocTypeIndex][memoryHeapIndex] >= size);
            mActivePerHeapMemoryAllocationsSize[allocTypeIndex][memoryHeapIndex] -= size;
        }
    }
}

VkDeviceSize MemoryAllocationTracker::getActiveMemoryAllocationsSize(uint32_t allocTypeIndex) const
{
    if (!kTrackMemoryAllocationSizes)
    {
        return 0;
    }

    return mActiveMemoryAllocationsSize[allocTypeIndex];
}

VkDeviceSize MemoryAllocationTracker::getActiveHeapMemoryAllocationsSize(uint32_t allocTypeIndex,
                                                                         uint32_t heapIndex) const
{
    if (!kTrackMemoryAllocationSizes)
    {
        return 0;
    }

    return mActivePerHeapMemoryAllocationsSize[allocTypeIndex][heapIndex];
}

uint64_t MemoryAllocationTracker::getActiveMemoryAllocationsCount(uint32_t allocTypeIndex) const
{
    if (!kTrackMemoryAllocationDebug)
    {
        return 0;
    }

    return mActiveMemoryAllocationsCount[allocTypeIndex];
}

uint64_t MemoryAllocationTracker::getActiveHeapMemoryAllocationsCount(uint32_t allocTypeIndex,
                                                                      uint32_t heapIndex) const
{
    if (!kTrackMemoryAllocationDebug)
    {
        return 0;
    }

    return mActivePerHeapMemoryAllocationsCount[allocTypeIndex][heapIndex];
}

VkDeviceSize MemoryAllocationTracker::getPendingMemoryAllocationSize() const
{
    if (!kTrackMemoryAllocationSizes)
    {
        return 0;
    }

    return mPendingMemoryAllocationSize;
}

vk::MemoryAllocationType MemoryAllocationTracker::getPendingMemoryAllocationType() const
{
    if (!kTrackMemoryAllocationSizes)
    {
        return vk::MemoryAllocationType::Unspecified;
    }

    return mPendingMemoryAllocationType;
}

uint32_t MemoryAllocationTracker::getPendingMemoryTypeIndex() const
{
    if (!kTrackMemoryAllocationSizes)
    {
        return 0;
    }

    return mPendingMemoryTypeIndex;
}

void MemoryAllocationTracker::setPendingMemoryAlloc(vk::MemoryAllocationType allocType,
                                                    VkDeviceSize size,
                                                    uint32_t memoryTypeIndex)
{
    if (!kTrackMemoryAllocationSizes)
    {
        return;
    }

    ASSERT(allocType != vk::MemoryAllocationType::InvalidEnum && size != 0);
    mPendingMemoryAllocationType = allocType;
    mPendingMemoryAllocationSize = size;
    mPendingMemoryTypeIndex      = memoryTypeIndex;
}

void MemoryAllocationTracker::resetPendingMemoryAlloc()
{
    if (!kTrackMemoryAllocationSizes)
    {
        return;
    }

    mPendingMemoryAllocationType = vk::MemoryAllocationType::Unspecified;
    mPendingMemoryAllocationSize = 0;
    mPendingMemoryTypeIndex      = kInvalidMemoryTypeIndex;
}

namespace vk
{
MemoryReport::MemoryReport()
    : mCurrentTotalAllocatedMemory(0),
      mMaxTotalAllocatedMemory(0),
      mCurrentTotalImportedMemory(0),
      mMaxTotalImportedMemory(0)
{}

void MemoryReport::processCallback(const VkDeviceMemoryReportCallbackDataEXT &callbackData,
                                   bool logCallback)
{
    std::unique_lock<std::mutex> lock(mMemoryReportMutex);
    VkDeviceSize size = 0;
    std::string reportType;
    switch (callbackData.type)
    {
        case VK_DEVICE_MEMORY_REPORT_EVENT_TYPE_ALLOCATE_EXT:
            reportType = "Allocate";
            if ((mUniqueIDCounts[callbackData.memoryObjectId] += 1) > 1)
            {
                break;
            }
            size = mSizesPerType[callbackData.objectType].allocatedMemory + callbackData.size;
            mSizesPerType[callbackData.objectType].allocatedMemory = size;
            if (mSizesPerType[callbackData.objectType].allocatedMemoryMax < size)
            {
                mSizesPerType[callbackData.objectType].allocatedMemoryMax = size;
            }
            mCurrentTotalAllocatedMemory += callbackData.size;
            if (mMaxTotalAllocatedMemory < mCurrentTotalAllocatedMemory)
            {
                mMaxTotalAllocatedMemory = mCurrentTotalAllocatedMemory;
            }
            break;
        case VK_DEVICE_MEMORY_REPORT_EVENT_TYPE_FREE_EXT:
            reportType = "Free";
            ASSERT(mUniqueIDCounts[callbackData.memoryObjectId] > 0);
            mUniqueIDCounts[callbackData.memoryObjectId] -= 1;
            size = mSizesPerType[callbackData.objectType].allocatedMemory - callbackData.size;
            mSizesPerType[callbackData.objectType].allocatedMemory = size;
            mCurrentTotalAllocatedMemory -= callbackData.size;
            break;
        case VK_DEVICE_MEMORY_REPORT_EVENT_TYPE_IMPORT_EXT:
            reportType = "Import";
            if ((mUniqueIDCounts[callbackData.memoryObjectId] += 1) > 1)
            {
                break;
            }
            size = mSizesPerType[callbackData.objectType].importedMemory + callbackData.size;
            mSizesPerType[callbackData.objectType].importedMemory = size;
            if (mSizesPerType[callbackData.objectType].importedMemoryMax < size)
            {
                mSizesPerType[callbackData.objectType].importedMemoryMax = size;
            }
            mCurrentTotalImportedMemory += callbackData.size;
            if (mMaxTotalImportedMemory < mCurrentTotalImportedMemory)
            {
                mMaxTotalImportedMemory = mCurrentTotalImportedMemory;
            }
            break;
        case VK_DEVICE_MEMORY_REPORT_EVENT_TYPE_UNIMPORT_EXT:
            reportType = "Un-Import";
            ASSERT(mUniqueIDCounts[callbackData.memoryObjectId] > 0);
            mUniqueIDCounts[callbackData.memoryObjectId] -= 1;
            size = mSizesPerType[callbackData.objectType].importedMemory - callbackData.size;
            mSizesPerType[callbackData.objectType].importedMemory = size;
            mCurrentTotalImportedMemory -= callbackData.size;
            break;
        case VK_DEVICE_MEMORY_REPORT_EVENT_TYPE_ALLOCATION_FAILED_EXT:
            reportType = "allocFail";
            break;
        default:
            UNREACHABLE();
            return;
    }
    if (logCallback)
    {
        INFO() << std::right << std::setw(9) << reportType << ": size=" << std::setw(10)
               << callbackData.size << "; type=" << std::setw(15) << std::left
               << GetVkObjectTypeName(callbackData.objectType)
               << "; heapIdx=" << callbackData.heapIndex << "; id=" << std::hex
               << callbackData.memoryObjectId << "; handle=" << std::hex
               << callbackData.objectHandle << ": Total=" << std::right << std::setw(10) << std::dec
               << size;
    }
}

void MemoryReport::logMemoryReportStats() const
{
    std::unique_lock<std::mutex> lock(mMemoryReportMutex);

    INFO() << std::right << "GPU Memory Totals:       Allocated=" << std::setw(10)
           << mCurrentTotalAllocatedMemory << " (max=" << std::setw(10) << mMaxTotalAllocatedMemory
           << ");  Imported=" << std::setw(10) << mCurrentTotalImportedMemory
           << " (max=" << std::setw(10) << mMaxTotalImportedMemory << ")";
    INFO() << "Sub-Totals per type:";
    for (const auto &it : mSizesPerType)
    {
        VkObjectType objectType         = it.first;
        MemorySizes memorySizes         = it.second;
        VkDeviceSize allocatedMemory    = memorySizes.allocatedMemory;
        VkDeviceSize allocatedMemoryMax = memorySizes.allocatedMemoryMax;
        VkDeviceSize importedMemory     = memorySizes.importedMemory;
        VkDeviceSize importedMemoryMax  = memorySizes.importedMemoryMax;
        INFO() << std::right << "- Type=" << std::setw(15) << GetVkObjectTypeName(objectType)
               << ":  Allocated=" << std::setw(10) << allocatedMemory << " (max=" << std::setw(10)
               << allocatedMemoryMax << ");  Imported=" << std::setw(10) << importedMemory
               << " (max=" << std::setw(10) << importedMemoryMax << ")";
    }
}
}  // namespace vk
}  // namespace rx
