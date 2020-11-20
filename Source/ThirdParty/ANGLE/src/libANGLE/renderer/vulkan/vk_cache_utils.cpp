//
// Copyright 2018 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// vk_cache_utils.cpp:
//    Contains the classes for the Pipeline State Object cache as well as the RenderPass cache.
//    Also contains the structures for the packed descriptions for the RenderPass and Pipeline.
//

#include "libANGLE/renderer/vulkan/vk_cache_utils.h"

#include "common/aligned_memory.h"
#include "common/vulkan/vk_google_filtering_precision.h"
#include "libANGLE/BlobCache.h"
#include "libANGLE/VertexAttribute.h"
#include "libANGLE/renderer/vulkan/DisplayVk.h"
#include "libANGLE/renderer/vulkan/FramebufferVk.h"
#include "libANGLE/renderer/vulkan/ProgramVk.h"
#include "libANGLE/renderer/vulkan/RendererVk.h"
#include "libANGLE/renderer/vulkan/VertexArrayVk.h"
#include "libANGLE/renderer/vulkan/vk_format_utils.h"
#include "libANGLE/renderer/vulkan/vk_helpers.h"

#include <type_traits>

namespace rx
{
namespace vk
{

namespace
{

constexpr int32_t kDynamicScissorSentinel = std::numeric_limits<int32_t>::min();

bool IsScissorStateDynamic(const VkRect2D &scissor)
{
    return scissor.offset.x == kDynamicScissorSentinel;
}

uint8_t PackGLBlendOp(GLenum blendOp)
{
    switch (blendOp)
    {
        case GL_FUNC_ADD:
            return static_cast<uint8_t>(VK_BLEND_OP_ADD);
        case GL_FUNC_SUBTRACT:
            return static_cast<uint8_t>(VK_BLEND_OP_SUBTRACT);
        case GL_FUNC_REVERSE_SUBTRACT:
            return static_cast<uint8_t>(VK_BLEND_OP_REVERSE_SUBTRACT);
        case GL_MIN:
            return static_cast<uint8_t>(VK_BLEND_OP_MIN);
        case GL_MAX:
            return static_cast<uint8_t>(VK_BLEND_OP_MAX);
        default:
            UNREACHABLE();
            return 0;
    }
}

uint8_t PackGLBlendFactor(GLenum blendFactor)
{
    switch (blendFactor)
    {
        case GL_ZERO:
            return static_cast<uint8_t>(VK_BLEND_FACTOR_ZERO);
        case GL_ONE:
            return static_cast<uint8_t>(VK_BLEND_FACTOR_ONE);
        case GL_SRC_COLOR:
            return static_cast<uint8_t>(VK_BLEND_FACTOR_SRC_COLOR);
        case GL_DST_COLOR:
            return static_cast<uint8_t>(VK_BLEND_FACTOR_DST_COLOR);
        case GL_ONE_MINUS_SRC_COLOR:
            return static_cast<uint8_t>(VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR);
        case GL_SRC_ALPHA:
            return static_cast<uint8_t>(VK_BLEND_FACTOR_SRC_ALPHA);
        case GL_ONE_MINUS_SRC_ALPHA:
            return static_cast<uint8_t>(VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA);
        case GL_DST_ALPHA:
            return static_cast<uint8_t>(VK_BLEND_FACTOR_DST_ALPHA);
        case GL_ONE_MINUS_DST_ALPHA:
            return static_cast<uint8_t>(VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA);
        case GL_ONE_MINUS_DST_COLOR:
            return static_cast<uint8_t>(VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR);
        case GL_SRC_ALPHA_SATURATE:
            return static_cast<uint8_t>(VK_BLEND_FACTOR_SRC_ALPHA_SATURATE);
        case GL_CONSTANT_COLOR:
            return static_cast<uint8_t>(VK_BLEND_FACTOR_CONSTANT_COLOR);
        case GL_CONSTANT_ALPHA:
            return static_cast<uint8_t>(VK_BLEND_FACTOR_CONSTANT_ALPHA);
        case GL_ONE_MINUS_CONSTANT_COLOR:
            return static_cast<uint8_t>(VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR);
        case GL_ONE_MINUS_CONSTANT_ALPHA:
            return static_cast<uint8_t>(VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA);
        default:
            UNREACHABLE();
            return 0;
    }
}

VkStencilOp PackGLStencilOp(GLenum compareOp)
{
    switch (compareOp)
    {
        case GL_KEEP:
            return VK_STENCIL_OP_KEEP;
        case GL_ZERO:
            return VK_STENCIL_OP_ZERO;
        case GL_REPLACE:
            return VK_STENCIL_OP_REPLACE;
        case GL_INCR:
            return VK_STENCIL_OP_INCREMENT_AND_CLAMP;
        case GL_DECR:
            return VK_STENCIL_OP_DECREMENT_AND_CLAMP;
        case GL_INCR_WRAP:
            return VK_STENCIL_OP_INCREMENT_AND_WRAP;
        case GL_DECR_WRAP:
            return VK_STENCIL_OP_DECREMENT_AND_WRAP;
        case GL_INVERT:
            return VK_STENCIL_OP_INVERT;
        default:
            UNREACHABLE();
            return VK_STENCIL_OP_KEEP;
    }
}

VkCompareOp PackGLCompareFunc(GLenum compareFunc)
{
    switch (compareFunc)
    {
        case GL_NEVER:
            return VK_COMPARE_OP_NEVER;
        case GL_ALWAYS:
            return VK_COMPARE_OP_ALWAYS;
        case GL_LESS:
            return VK_COMPARE_OP_LESS;
        case GL_LEQUAL:
            return VK_COMPARE_OP_LESS_OR_EQUAL;
        case GL_EQUAL:
            return VK_COMPARE_OP_EQUAL;
        case GL_GREATER:
            return VK_COMPARE_OP_GREATER;
        case GL_GEQUAL:
            return VK_COMPARE_OP_GREATER_OR_EQUAL;
        case GL_NOTEQUAL:
            return VK_COMPARE_OP_NOT_EQUAL;
        default:
            UNREACHABLE();
            return VK_COMPARE_OP_NEVER;
    }
}

void UnpackAttachmentDesc(VkAttachmentDescription *desc,
                          const vk::Format &format,
                          uint8_t samples,
                          const vk::PackedAttachmentOpsDesc &ops)
{
    // We would only need this flag for duplicated attachments. Apply it conservatively.
    desc->flags   = VK_ATTACHMENT_DESCRIPTION_MAY_ALIAS_BIT;
    desc->format  = format.vkImageFormat;
    desc->samples = gl_vk::GetSamples(samples);
    desc->loadOp  = static_cast<VkAttachmentLoadOp>(ops.loadOp);
    desc->storeOp =
        ConvertRenderPassStoreOpToVkStoreOp(static_cast<RenderPassStoreOp>(ops.storeOp));
    desc->stencilLoadOp = static_cast<VkAttachmentLoadOp>(ops.stencilLoadOp);
    desc->stencilStoreOp =
        ConvertRenderPassStoreOpToVkStoreOp(static_cast<RenderPassStoreOp>(ops.stencilStoreOp));
    desc->initialLayout =
        ConvertImageLayoutToVkImageLayout(static_cast<ImageLayout>(ops.initialLayout));
    desc->finalLayout =
        ConvertImageLayoutToVkImageLayout(static_cast<ImageLayout>(ops.finalLayout));
}

void UnpackColorResolveAttachmentDesc(VkAttachmentDescription *desc,
                                      const vk::Format &format,
                                      bool usedAsInputAttachment,
                                      bool isInvalidated)
{
    // We would only need this flag for duplicated attachments. Apply it conservatively.  In
    // practice it's unlikely any application would use the same image as multiple resolve
    // attachments simultaneously, so this flag can likely be removed without any issue if it incurs
    // a performance penalty.
    desc->flags  = VK_ATTACHMENT_DESCRIPTION_MAY_ALIAS_BIT;
    desc->format = format.vkImageFormat;

    // This function is for color resolve attachments.
    const angle::Format &angleFormat = format.actualImageFormat();
    ASSERT(angleFormat.depthBits == 0 && angleFormat.stencilBits == 0);

    // Resolve attachments always have a sample count of 1.
    //
    // If the corresponding color attachment needs to take its initial value from the resolve
    // attachment (i.e. needs to be unresolved), loadOp needs to be set to LOAD, otherwise it should
    // be DONT_CARE as it gets overwritten during resolve.
    //
    // storeOp should be STORE.  If the attachment is invalidated, it is set to DONT_CARE.
    desc->samples = VK_SAMPLE_COUNT_1_BIT;
    desc->loadOp =
        usedAsInputAttachment ? VK_ATTACHMENT_LOAD_OP_LOAD : VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    desc->storeOp = isInvalidated ? VK_ATTACHMENT_STORE_OP_DONT_CARE : VK_ATTACHMENT_STORE_OP_STORE;
    desc->stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    desc->stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    desc->initialLayout  = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    desc->finalLayout    = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
}

void UnpackDepthStencilResolveAttachmentDesc(VkAttachmentDescription *desc,
                                             const vk::Format &format,
                                             bool usedAsDepthInputAttachment,
                                             bool usedAsStencilInputAttachment,
                                             bool isDepthInvalidated,
                                             bool isStencilInvalidated)
{
    // There cannot be simultaneous usages of the depth/stencil resolve image, as depth/stencil
    // resolve currently only comes from depth/stencil renderbuffers.
    desc->flags  = 0;
    desc->format = format.vkImageFormat;

    // This function is for depth/stencil resolve attachment.
    const angle::Format &angleFormat = format.intendedFormat();
    ASSERT(angleFormat.depthBits != 0 || angleFormat.stencilBits != 0);

    // Missing aspects are folded in is*Invalidated parameters, so no need to double check.
    ASSERT(angleFormat.depthBits > 0 || isDepthInvalidated);
    ASSERT(angleFormat.stencilBits > 0 || isStencilInvalidated);

    // Similarly to color resolve attachments, sample count is 1, loadOp is LOAD or DONT_CARE based
    // on whether unresolve is required, and storeOp is STORE or DONT_CARE based on whether the
    // attachment is invalidated or the aspect exists.
    desc->samples = VK_SAMPLE_COUNT_1_BIT;
    desc->loadOp =
        usedAsDepthInputAttachment ? VK_ATTACHMENT_LOAD_OP_LOAD : VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    desc->storeOp =
        isDepthInvalidated ? VK_ATTACHMENT_STORE_OP_DONT_CARE : VK_ATTACHMENT_STORE_OP_STORE;
    desc->stencilLoadOp =
        usedAsStencilInputAttachment ? VK_ATTACHMENT_LOAD_OP_LOAD : VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    desc->stencilStoreOp =
        isStencilInvalidated ? VK_ATTACHMENT_STORE_OP_DONT_CARE : VK_ATTACHMENT_STORE_OP_STORE;
    desc->initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    desc->finalLayout   = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
}

void UnpackStencilState(const vk::PackedStencilOpState &packedState,
                        uint8_t stencilReference,
                        VkStencilOpState *stateOut)
{
    stateOut->failOp      = static_cast<VkStencilOp>(packedState.ops.fail);
    stateOut->passOp      = static_cast<VkStencilOp>(packedState.ops.pass);
    stateOut->depthFailOp = static_cast<VkStencilOp>(packedState.ops.depthFail);
    stateOut->compareOp   = static_cast<VkCompareOp>(packedState.ops.compare);
    stateOut->compareMask = packedState.compareMask;
    stateOut->writeMask   = packedState.writeMask;
    stateOut->reference   = stencilReference;
}

void UnpackBlendAttachmentState(const vk::PackedColorBlendAttachmentState &packedState,
                                VkPipelineColorBlendAttachmentState *stateOut)
{
    stateOut->srcColorBlendFactor = static_cast<VkBlendFactor>(packedState.srcColorBlendFactor);
    stateOut->dstColorBlendFactor = static_cast<VkBlendFactor>(packedState.dstColorBlendFactor);
    stateOut->colorBlendOp        = static_cast<VkBlendOp>(packedState.colorBlendOp);
    stateOut->srcAlphaBlendFactor = static_cast<VkBlendFactor>(packedState.srcAlphaBlendFactor);
    stateOut->dstAlphaBlendFactor = static_cast<VkBlendFactor>(packedState.dstAlphaBlendFactor);
    stateOut->alphaBlendOp        = static_cast<VkBlendOp>(packedState.alphaBlendOp);
}

void SetPipelineShaderStageInfo(const VkStructureType type,
                                const VkShaderStageFlagBits stage,
                                const VkShaderModule module,
                                const VkSpecializationInfo &specializationInfo,
                                VkPipelineShaderStageCreateInfo *shaderStage)
{
    shaderStage->sType               = type;
    shaderStage->flags               = 0;
    shaderStage->stage               = stage;
    shaderStage->module              = module;
    shaderStage->pName               = "main";
    shaderStage->pSpecializationInfo = &specializationInfo;
}

// Defines a subpass that uses the resolve attachments as input attachments to initialize color and
// depth/stencil attachments that need to be "unresolved" at the start of the render pass.  The
// subpass will only contain the attachments that need to be unresolved to simplify the shader that
// performs the operations.
void InitializeUnresolveSubpass(
    const RenderPassDesc &desc,
    const gl::DrawBuffersVector<VkAttachmentReference> &drawSubpassColorAttachmentRefs,
    const gl::DrawBuffersVector<VkAttachmentReference> &drawSubpassResolveAttachmentRefs,
    const VkAttachmentReference &depthStencilAttachmentRef,
    const VkAttachmentReference2KHR &depthStencilResolveAttachmentRef,
    gl::DrawBuffersVector<VkAttachmentReference> *unresolveColorAttachmentRefs,
    VkAttachmentReference *unresolveDepthStencilAttachmentRef,
    FramebufferAttachmentsVector<VkAttachmentReference> *unresolveInputAttachmentRefs,
    FramebufferAttachmentsVector<uint32_t> *unresolvePreserveAttachmentRefs,
    VkSubpassDescription *subpassDesc)
{
    for (uint32_t colorIndexGL = 0; colorIndexGL < desc.colorAttachmentRange(); ++colorIndexGL)
    {
        // Assume the GL Framebuffer has the following attachments enabled:
        //
        //     GL Color 0
        //     GL Color 3
        //     GL Color 4
        //     GL Color 6
        //     GL Color 7
        //     GL Depth/Stencil
        //
        // Additionally, assume Color 0, 4 and 6 are multisampled-render-to-texture (or for any
        // other reason) have corresponding resolve attachments.  Furthermore, say Color 4 and 6
        // require an initial unresolve operation.
        //
        // In the above example, the render pass is created with the following attachments:
        //
        //     RP Attachment[0] <- corresponding to GL Color 0
        //     RP Attachment[1] <- corresponding to GL Color 3
        //     RP Attachment[2] <- corresponding to GL Color 4
        //     RP Attachment[3] <- corresponding to GL Color 6
        //     RP Attachment[4] <- corresponding to GL Color 7
        //     RP Attachment[5] <- corresponding to GL Depth/Stencil
        //     RP Attachment[6] <- corresponding to resolve attachment of GL Color 0
        //     RP Attachment[7] <- corresponding to resolve attachment of GL Color 4
        //     RP Attachment[8] <- corresponding to resolve attachment of GL Color 6
        //
        // If the depth/stencil attachment is to be resolved, the following attachment would also be
        // present:
        //
        //     RP Attachment[9] <- corresponding to resolve attachment of GL Depth/Stencil
        //
        // The subpass that takes the application draw calls has the following attachments, creating
        // the mapping from the Vulkan attachment indices (i.e. RP attachment indices) to GL indices
        // as indicated by the GL shaders:
        //
        //     Subpass[1] Color[0] -> RP Attachment[0]
        //     Subpass[1] Color[1] -> VK_ATTACHMENT_UNUSED
        //     Subpass[1] Color[2] -> VK_ATTACHMENT_UNUSED
        //     Subpass[1] Color[3] -> RP Attachment[1]
        //     Subpass[1] Color[4] -> RP Attachment[2]
        //     Subpass[1] Color[5] -> VK_ATTACHMENT_UNUSED
        //     Subpass[1] Color[6] -> RP Attachment[3]
        //     Subpass[1] Color[7] -> RP Attachment[4]
        //     Subpass[1] Depth/Stencil -> RP Attachment[5]
        //     Subpass[1] Resolve[0] -> RP Attachment[6]
        //     Subpass[1] Resolve[1] -> VK_ATTACHMENT_UNUSED
        //     Subpass[1] Resolve[2] -> VK_ATTACHMENT_UNUSED
        //     Subpass[1] Resolve[3] -> VK_ATTACHMENT_UNUSED
        //     Subpass[1] Resolve[4] -> RP Attachment[7]
        //     Subpass[1] Resolve[5] -> VK_ATTACHMENT_UNUSED
        //     Subpass[1] Resolve[6] -> RP Attachment[8]
        //     Subpass[1] Resolve[7] -> VK_ATTACHMENT_UNUSED
        //
        // With depth/stencil resolve attachment:
        //
        //     Subpass[1] Depth/Stencil Resolve -> RP Attachment[9]
        //
        // The initial subpass that's created here is (remember that in the above example Color 4
        // and 6 need to be unresolved):
        //
        //     Subpass[0] Input[0] -> RP Attachment[7] = Subpass[1] Resolve[4]
        //     Subpass[0] Input[1] -> RP Attachment[8] = Subpass[1] Resolve[6]
        //     Subpass[0] Color[0] -> RP Attachment[2] = Subpass[1] Color[4]
        //     Subpass[0] Color[1] -> RP Attachment[3] = Subpass[1] Color[6]
        //
        // The trick here therefore is to use the color attachment refs already created for the
        // application draw subpass indexed with colorIndexGL.
        //
        // If depth/stencil needs to be unresolved:
        //
        //     Subpass[0] Input[2] -> RP Attachment[9] = Subpass[1] Depth/Stencil Resolve
        //     Subpass[0] Color[2] -> RP Attachment[5] = Subpass[1] Depth/Stencil
        //
        // As an additional note, the attachments that are not used in the unresolve subpass must be
        // preserved.  That is color attachments and the depth/stencil attachment if any.  Resolve
        // attachments are rewritten by the next subpass, so they don't need to be preserved.  Note
        // that there's no need to preserve attachments whose loadOp is DONT_CARE.  For simplicity,
        // we preserve those as well.  The driver would ideally avoid preserving attachments with
        // loadOp=DONT_CARE.
        //
        // With the above example:
        //
        //     Subpass[0] Preserve[0] -> RP Attachment[0] = Subpass[1] Color[0]
        //     Subpass[0] Preserve[1] -> RP Attachment[1] = Subpass[1] Color[3]
        //     Subpass[0] Preserve[2] -> RP Attachment[4] = Subpass[1] Color[7]
        //
        // If depth/stencil is not unresolved:
        //
        //     Subpass[0] Preserve[3] -> RP Attachment[5] = Subpass[1] Depth/Stencil
        //
        // Again, the color attachment refs already created for the application draw subpass can be
        // used indexed with colorIndexGL.

        if (!desc.hasColorUnresolveAttachment(colorIndexGL))
        {
            if (desc.isColorAttachmentEnabled(colorIndexGL))
            {
                unresolvePreserveAttachmentRefs->push_back(
                    drawSubpassColorAttachmentRefs[colorIndexGL].attachment);
            }
            continue;
        }
        ASSERT(desc.isColorAttachmentEnabled(colorIndexGL));
        ASSERT(desc.hasColorResolveAttachment(colorIndexGL));
        ASSERT(drawSubpassColorAttachmentRefs[colorIndexGL].attachment != VK_ATTACHMENT_UNUSED);
        ASSERT(drawSubpassResolveAttachmentRefs[colorIndexGL].attachment != VK_ATTACHMENT_UNUSED);

        unresolveColorAttachmentRefs->push_back(drawSubpassColorAttachmentRefs[colorIndexGL]);
        unresolveInputAttachmentRefs->push_back(drawSubpassResolveAttachmentRefs[colorIndexGL]);

        // Note the input attachment layout should be shader read-only.  The subpass dependency
        // will take care of transitioning the layout of the resolve attachment to color attachment
        // automatically.
        unresolveInputAttachmentRefs->back().layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }

    if (desc.hasDepthStencilUnresolveAttachment())
    {
        ASSERT(desc.hasDepthStencilAttachment());
        ASSERT(desc.hasDepthStencilResolveAttachment());

        *unresolveDepthStencilAttachmentRef = depthStencilAttachmentRef;

        VkAttachmentReference unresolveDepthStencilInputAttachmentRef = {};
        unresolveDepthStencilInputAttachmentRef.attachment =
            depthStencilResolveAttachmentRef.attachment;
        unresolveDepthStencilInputAttachmentRef.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        unresolveInputAttachmentRefs->push_back(unresolveDepthStencilInputAttachmentRef);
    }
    else if (desc.hasDepthStencilAttachment())
    {
        // Preserve the depth/stencil attachment if not unresolved.  Again, there's no need to
        // preserve this attachment if loadOp=DONT_CARE, but we do for simplicity.
        unresolvePreserveAttachmentRefs->push_back(depthStencilAttachmentRef.attachment);
    }

    ASSERT(!unresolveColorAttachmentRefs->empty() ||
           unresolveDepthStencilAttachmentRef->attachment != VK_ATTACHMENT_UNUSED);
    ASSERT(unresolveColorAttachmentRefs->size() +
               (desc.hasDepthStencilUnresolveAttachment() ? 1 : 0) ==
           unresolveInputAttachmentRefs->size());

    subpassDesc->flags                = 0;
    subpassDesc->pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpassDesc->inputAttachmentCount = static_cast<uint32_t>(unresolveInputAttachmentRefs->size());
    subpassDesc->pInputAttachments    = unresolveInputAttachmentRefs->data();
    subpassDesc->colorAttachmentCount = static_cast<uint32_t>(unresolveColorAttachmentRefs->size());
    subpassDesc->pColorAttachments    = unresolveColorAttachmentRefs->data();
    subpassDesc->pResolveAttachments  = nullptr;
    subpassDesc->pDepthStencilAttachment = unresolveDepthStencilAttachmentRef;
    subpassDesc->preserveAttachmentCount =
        static_cast<uint32_t>(unresolvePreserveAttachmentRefs->size());
    subpassDesc->pPreserveAttachments = unresolvePreserveAttachmentRefs->data();
}

// There is normally one subpass, and occasionally another for the unresolve operation.
constexpr size_t kSubpassFastVectorSize = 2;
template <typename T>
using SubpassVector = angle::FastVector<T, kSubpassFastVectorSize>;

void InitializeUnresolveSubpassDependencies(const SubpassVector<VkSubpassDescription> &subpassDesc,
                                            bool unresolveColor,
                                            bool unresolveDepthStencil,
                                            std::vector<VkSubpassDependency> *subpassDependencies)
{
    ASSERT(subpassDesc.size() >= 2);
    ASSERT(unresolveColor || unresolveDepthStencil);

    // The unresolve subpass is the first subpass.  The application draw subpass is the next one.
    constexpr uint32_t kUnresolveSubpassIndex = 0;
    constexpr uint32_t kDrawSubpassIndex      = 1;

    // A subpass dependency is needed between the unresolve and draw subpasses.  There are two
    // hazards here:
    //
    // - Subpass 0 writes to color/depth/stencil attachments, subpass 1 writes to the same
    //   attachments.  This is a WaW hazard (color/depth/stencil write -> color/depth/stencil write)
    //   similar to when two subsequent render passes write to the same images.
    // - Subpass 0 reads from resolve attachments, subpass 1 writes to the same resolve attachments.
    //   This is a WaR hazard (fragment shader read -> color write) which only requires an execution
    //   barrier.
    //
    // Note: the DEPENDENCY_BY_REGION flag is necessary to create a "framebuffer-local" dependency,
    // as opposed to "framebuffer-global".  The latter is effectively a render pass break.  The
    // former creates a dependency per framebuffer region.  If dependency scopes correspond to
    // attachments with:
    //
    // - Same sample count: dependency is at sample granularity
    // - Different sample count: dependency is at pixel granularity
    //
    // The latter is clarified by the spec as such:
    //
    // > Practically, the pixel vs sample granularity dependency means that if an input attachment
    // > has a different number of samples than the pipeline's rasterizationSamples, then a fragment
    // > can access any sample in the input attachment's pixel even if it only uses
    // > framebuffer-local dependencies.
    //
    // The dependency for the first hazard above (attachment write -> attachment write) is on
    // same-sample attachments, so it will not allow the use of input attachments as required by the
    // unresolve subpass.  As a result, even though the second hazard seems to be subsumed by the
    // first (its src stage is earlier and its dst stage is the same), a separate dependency is
    // created for it just to obtain a pixel granularity dependency.
    //
    // Note: depth/stencil resolve is considered to be done in the color write stage:
    //
    // > Moving to the next subpass automatically performs any multisample resolve operations in the
    // > subpass being ended. End-of-subpass multisample resolves are treated as color attachment
    // > writes for the purposes of synchronization. This applies to resolve operations for both
    // > color and depth/stencil attachments. That is, they are considered to execute in the
    // > VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT pipeline stage and their writes are
    // > synchronized with VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT.

    subpassDependencies->emplace_back();
    VkSubpassDependency *dependency = &subpassDependencies->back();

    constexpr VkPipelineStageFlags kColorWriteStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    constexpr VkPipelineStageFlags kColorReadWriteStage =
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    constexpr VkAccessFlags kColorWriteFlags = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    constexpr VkAccessFlags kColorReadWriteFlags =
        kColorWriteFlags | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;

    constexpr VkPipelineStageFlags kDepthStencilWriteStage =
        VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    constexpr VkPipelineStageFlags kDepthStencilReadWriteStage =
        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    constexpr VkAccessFlags kDepthStencilWriteFlags = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    constexpr VkAccessFlags kDepthStencilReadWriteFlags =
        kDepthStencilWriteFlags | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;

    VkPipelineStageFlags attachmentWriteStages     = 0;
    VkPipelineStageFlags attachmentReadWriteStages = 0;
    VkAccessFlags attachmentWriteFlags             = 0;
    VkAccessFlags attachmentReadWriteFlags         = 0;

    if (unresolveColor)
    {
        attachmentWriteStages |= kColorWriteStage;
        attachmentReadWriteStages |= kColorReadWriteStage;
        attachmentWriteFlags |= kColorWriteFlags;
        attachmentReadWriteFlags |= kColorReadWriteFlags;
    }

    if (unresolveDepthStencil)
    {
        attachmentWriteStages |= kDepthStencilWriteStage;
        attachmentReadWriteStages |= kDepthStencilReadWriteStage;
        attachmentWriteFlags |= kDepthStencilWriteFlags;
        attachmentReadWriteFlags |= kDepthStencilReadWriteFlags;
    }

    dependency->srcSubpass      = kUnresolveSubpassIndex;
    dependency->dstSubpass      = kDrawSubpassIndex;
    dependency->srcStageMask    = attachmentWriteStages;
    dependency->dstStageMask    = attachmentReadWriteStages;
    dependency->srcAccessMask   = attachmentWriteFlags;
    dependency->dstAccessMask   = attachmentReadWriteFlags;
    dependency->dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    subpassDependencies->emplace_back();
    dependency = &subpassDependencies->back();

    // Note again that depth/stencil resolve is considered to be done in the color output stage.
    dependency->srcSubpass      = kUnresolveSubpassIndex;
    dependency->dstSubpass      = kDrawSubpassIndex;
    dependency->srcStageMask    = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dependency->dstStageMask    = kColorWriteStage;
    dependency->srcAccessMask   = VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
    dependency->dstAccessMask   = kColorWriteFlags;
    dependency->dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
}

void ToAttachmentDesciption2(const VkAttachmentDescription &desc,
                             VkAttachmentDescription2KHR *desc2Out)
{
    *desc2Out                = {};
    desc2Out->sType          = VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2_KHR;
    desc2Out->flags          = desc.flags;
    desc2Out->format         = desc.format;
    desc2Out->samples        = desc.samples;
    desc2Out->loadOp         = desc.loadOp;
    desc2Out->storeOp        = desc.storeOp;
    desc2Out->stencilLoadOp  = desc.stencilLoadOp;
    desc2Out->stencilStoreOp = desc.stencilStoreOp;
    desc2Out->initialLayout  = desc.initialLayout;
    desc2Out->finalLayout    = desc.finalLayout;
}

void ToAttachmentReference2(const VkAttachmentReference &ref,
                            VkImageAspectFlags aspectMask,
                            VkAttachmentReference2KHR *ref2Out)
{
    *ref2Out            = {};
    ref2Out->sType      = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2_KHR;
    ref2Out->attachment = ref.attachment;
    ref2Out->layout     = ref.layout;
    ref2Out->aspectMask = aspectMask;
}

void ToSubpassDescription2(const VkSubpassDescription &desc,
                           const FramebufferAttachmentsVector<VkAttachmentReference2KHR> &inputRefs,
                           const gl::DrawBuffersVector<VkAttachmentReference2KHR> &colorRefs,
                           const gl::DrawBuffersVector<VkAttachmentReference2KHR> &resolveRefs,
                           const VkAttachmentReference2KHR &depthStencilRef,
                           VkSubpassDescription2KHR *desc2Out)
{
    *desc2Out                         = {};
    desc2Out->sType                   = VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_2_KHR;
    desc2Out->flags                   = desc.flags;
    desc2Out->pipelineBindPoint       = desc.pipelineBindPoint;
    desc2Out->inputAttachmentCount    = static_cast<uint32_t>(inputRefs.size());
    desc2Out->pInputAttachments       = !inputRefs.empty() ? inputRefs.data() : nullptr;
    desc2Out->colorAttachmentCount    = static_cast<uint32_t>(colorRefs.size());
    desc2Out->pColorAttachments       = !colorRefs.empty() ? colorRefs.data() : nullptr;
    desc2Out->pResolveAttachments     = !resolveRefs.empty() ? resolveRefs.data() : nullptr;
    desc2Out->pDepthStencilAttachment = desc.pDepthStencilAttachment ? &depthStencilRef : nullptr;
    desc2Out->preserveAttachmentCount = desc.preserveAttachmentCount;
    desc2Out->pPreserveAttachments    = desc.pPreserveAttachments;
}

void ToSubpassDependency2(const VkSubpassDependency &dep, VkSubpassDependency2KHR *dep2Out)
{
    *dep2Out                 = {};
    dep2Out->sType           = VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY_2_KHR;
    dep2Out->srcSubpass      = dep.srcSubpass;
    dep2Out->dstSubpass      = dep.dstSubpass;
    dep2Out->srcStageMask    = dep.srcStageMask;
    dep2Out->dstStageMask    = dep.dstStageMask;
    dep2Out->srcAccessMask   = dep.srcAccessMask;
    dep2Out->dstAccessMask   = dep.dstAccessMask;
    dep2Out->dependencyFlags = dep.dependencyFlags;
}

angle::Result CreateRenderPass2(Context *context,
                                const VkRenderPassCreateInfo &createInfo,
                                const VkSubpassDescriptionDepthStencilResolve &depthStencilResolve,
                                bool unresolveDepth,
                                bool unresolveStencil,
                                RenderPass *renderPass)
{
    // Convert the attachments to VkAttachmentDescription2.
    FramebufferAttachmentArray<VkAttachmentDescription2KHR> attachmentDescs;
    for (uint32_t index = 0; index < createInfo.attachmentCount; ++index)
    {
        ToAttachmentDesciption2(createInfo.pAttachments[index], &attachmentDescs[index]);
    }

    // Convert subpass attachments to VkAttachmentReference2 and the subpass description to
    // VkSubpassDescription2.
    SubpassVector<FramebufferAttachmentsVector<VkAttachmentReference2KHR>>
        subpassInputAttachmentRefs(createInfo.subpassCount);
    SubpassVector<gl::DrawBuffersVector<VkAttachmentReference2KHR>> subpassColorAttachmentRefs(
        createInfo.subpassCount);
    SubpassVector<gl::DrawBuffersVector<VkAttachmentReference2KHR>> subpassResolveAttachmentRefs(
        createInfo.subpassCount);
    SubpassVector<VkAttachmentReference2KHR> subpassDepthStencilAttachmentRefs(
        createInfo.subpassCount);
    SubpassVector<VkSubpassDescription2KHR> subpassDescriptions(createInfo.subpassCount);
    for (uint32_t subpass = 0; subpass < createInfo.subpassCount; ++subpass)
    {
        const VkSubpassDescription &desc = createInfo.pSubpasses[subpass];
        FramebufferAttachmentsVector<VkAttachmentReference2KHR> &inputRefs =
            subpassInputAttachmentRefs[subpass];
        gl::DrawBuffersVector<VkAttachmentReference2KHR> &colorRefs =
            subpassColorAttachmentRefs[subpass];
        gl::DrawBuffersVector<VkAttachmentReference2KHR> &resolveRefs =
            subpassResolveAttachmentRefs[subpass];
        VkAttachmentReference2KHR &depthStencilRef = subpassDepthStencilAttachmentRefs[subpass];

        inputRefs.resize(desc.inputAttachmentCount);
        colorRefs.resize(desc.colorAttachmentCount);

        // Convert subpass attachment references.
        for (uint32_t index = 0; index < desc.inputAttachmentCount; ++index)
        {
            VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            if (index >= desc.colorAttachmentCount)
            {
                // Set the aspect of the depth/stencil input attachment (of which there can be only
                // one).
                ASSERT(index + 1 == desc.inputAttachmentCount);
                aspectMask = 0;
                if (unresolveDepth)
                {
                    aspectMask |= VK_IMAGE_ASPECT_DEPTH_BIT;
                }
                if (unresolveStencil)
                {
                    aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
                }
                ASSERT(aspectMask != 0);
            }

            ToAttachmentReference2(desc.pInputAttachments[index], aspectMask, &inputRefs[index]);
        }

        for (uint32_t index = 0; index < desc.colorAttachmentCount; ++index)
        {
            ToAttachmentReference2(desc.pColorAttachments[index], VK_IMAGE_ASPECT_COLOR_BIT,
                                   &colorRefs[index]);
        }
        if (desc.pResolveAttachments)
        {
            resolveRefs.resize(desc.colorAttachmentCount);
            for (uint32_t index = 0; index < desc.colorAttachmentCount; ++index)
            {
                ToAttachmentReference2(desc.pResolveAttachments[index], VK_IMAGE_ASPECT_COLOR_BIT,
                                       &resolveRefs[index]);
            }
        }
        if (desc.pDepthStencilAttachment)
        {
            ToAttachmentReference2(*desc.pDepthStencilAttachment,
                                   VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT,
                                   &depthStencilRef);
        }

        // Convert subpass itself.
        ToSubpassDescription2(desc, inputRefs, colorRefs, resolveRefs, depthStencilRef,
                              &subpassDescriptions[subpass]);
    }

    // Append the depth/stencil resolve attachment to the pNext chain of last subpass.
    ASSERT(depthStencilResolve.pDepthStencilResolveAttachment != nullptr);
    subpassDescriptions.back().pNext = &depthStencilResolve;

    // Convert subpass dependencies to VkSubpassDependency2.
    std::vector<VkSubpassDependency2KHR> subpassDependencies(createInfo.dependencyCount);
    for (uint32_t index = 0; index < createInfo.dependencyCount; ++index)
    {
        ToSubpassDependency2(createInfo.pDependencies[index], &subpassDependencies[index]);
    }

    // Convert CreateInfo itself
    VkRenderPassCreateInfo2KHR createInfo2 = {};
    createInfo2.sType                      = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO_2_KHR;
    createInfo2.flags                      = createInfo.flags;
    createInfo2.attachmentCount            = createInfo.attachmentCount;
    createInfo2.pAttachments               = attachmentDescs.data();
    createInfo2.subpassCount               = createInfo.subpassCount;
    createInfo2.pSubpasses                 = subpassDescriptions.data();
    createInfo2.dependencyCount            = static_cast<uint32_t>(subpassDependencies.size());
    createInfo2.pDependencies = !subpassDependencies.empty() ? subpassDependencies.data() : nullptr;

    // Initialize the render pass.
    ANGLE_VK_TRY(context, renderPass->init2(context->getDevice(), createInfo2));

    return angle::Result::Continue;
}

void UpdateRenderPassColorPerfCounters(const VkRenderPassCreateInfo &createInfo,
                                       const VkSubpassDescription &subpass,
                                       vk::RenderPassPerfCounters *countersOut)
{
    // Color resolve counters.
    if (subpass.pResolveAttachments == nullptr)
    {
        return;
    }

    for (uint32_t colorSubpassIndex = 0; colorSubpassIndex < subpass.colorAttachmentCount;
         ++colorSubpassIndex)
    {
        uint32_t resolveRenderPassIndex = subpass.pResolveAttachments[colorSubpassIndex].attachment;

        if (resolveRenderPassIndex == VK_ATTACHMENT_UNUSED)
        {
            continue;
        }

        ++countersOut->colorAttachmentResolves;
    }
}

void UpdateRenderPassDepthStencilPerfCounters(const VkRenderPassCreateInfo &createInfo,
                                              size_t renderPassIndex,
                                              vk::RenderPassPerfCounters *countersOut)
{
    ASSERT(renderPassIndex != VK_ATTACHMENT_UNUSED);

    // Depth/stencil ops counters.
    const VkAttachmentDescription &ds = createInfo.pAttachments[renderPassIndex];

    countersOut->depthClears += ds.loadOp == VK_ATTACHMENT_LOAD_OP_CLEAR ? 1 : 0;
    countersOut->depthLoads += ds.loadOp == VK_ATTACHMENT_LOAD_OP_LOAD ? 1 : 0;
    countersOut->depthStores +=
        ds.storeOp == static_cast<uint16_t>(RenderPassStoreOp::Store) ? 1 : 0;

    countersOut->stencilClears += ds.stencilLoadOp == VK_ATTACHMENT_LOAD_OP_CLEAR ? 1 : 0;
    countersOut->stencilLoads += ds.stencilLoadOp == VK_ATTACHMENT_LOAD_OP_LOAD ? 1 : 0;
    countersOut->stencilStores +=
        ds.stencilStoreOp == static_cast<uint16_t>(RenderPassStoreOp::Store) ? 1 : 0;

    // Depth/stencil read-only mode.
    countersOut->readOnlyDepthStencil +=
        ds.finalLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL ? 1 : 0;
}

void UpdateRenderPassDepthStencilResolvePerfCounters(
    const VkRenderPassCreateInfo &createInfo,
    const VkSubpassDescriptionDepthStencilResolve &depthStencilResolve,
    vk::RenderPassPerfCounters *countersOut)
{
    if (depthStencilResolve.pDepthStencilResolveAttachment == nullptr)
    {
        return;
    }

    uint32_t resolveRenderPassIndex =
        depthStencilResolve.pDepthStencilResolveAttachment->attachment;

    if (resolveRenderPassIndex == VK_ATTACHMENT_UNUSED)
    {
        return;
    }

    const VkAttachmentDescription &dsResolve = createInfo.pAttachments[resolveRenderPassIndex];

    // Resolve depth/stencil ops counters.
    countersOut->depthClears += dsResolve.loadOp == VK_ATTACHMENT_LOAD_OP_CLEAR ? 1 : 0;
    countersOut->depthLoads += dsResolve.loadOp == VK_ATTACHMENT_LOAD_OP_LOAD ? 1 : 0;
    countersOut->depthStores +=
        dsResolve.storeOp == static_cast<uint16_t>(RenderPassStoreOp::Store) ? 1 : 0;

    countersOut->stencilClears += dsResolve.stencilLoadOp == VK_ATTACHMENT_LOAD_OP_CLEAR ? 1 : 0;
    countersOut->stencilLoads += dsResolve.stencilLoadOp == VK_ATTACHMENT_LOAD_OP_LOAD ? 1 : 0;
    countersOut->stencilStores +=
        dsResolve.stencilStoreOp == static_cast<uint16_t>(RenderPassStoreOp::Store) ? 1 : 0;

    // Depth/stencil resolve counters.
    countersOut->depthAttachmentResolves +=
        depthStencilResolve.depthResolveMode != VK_RESOLVE_MODE_NONE ? 1 : 0;
    countersOut->stencilAttachmentResolves +=
        depthStencilResolve.stencilResolveMode != VK_RESOLVE_MODE_NONE ? 1 : 0;
}

void UpdateRenderPassPerfCounters(
    const RenderPassDesc &desc,
    const VkRenderPassCreateInfo &createInfo,
    const VkSubpassDescriptionDepthStencilResolve &depthStencilResolve,
    vk::RenderPassPerfCounters *countersOut)
{
    // Accumulate depth/stencil attachment indices in all subpasses to avoid double-counting
    // counters.
    vk::FramebufferAttachmentMask depthStencilAttachmentIndices;

    for (uint32_t subpassIndex = 0; subpassIndex < createInfo.subpassCount; ++subpassIndex)
    {
        const VkSubpassDescription &subpass = createInfo.pSubpasses[subpassIndex];

        // Color counters.  Note: currently there are no counters for load/store ops of color
        // attachments, so there's no risk of double counting.
        UpdateRenderPassColorPerfCounters(createInfo, subpass, countersOut);

        // Record index of depth/stencil attachment.
        if (subpass.pDepthStencilAttachment != nullptr)
        {
            uint32_t attachmentRenderPassIndex = subpass.pDepthStencilAttachment->attachment;
            if (attachmentRenderPassIndex != VK_ATTACHMENT_UNUSED)
            {
                depthStencilAttachmentIndices.set(attachmentRenderPassIndex);
            }
        }
    }

    // Depth/stencil counters.  Currently, both subpasses use the same depth/stencil attachment (if
    // any).
    ASSERT(depthStencilAttachmentIndices.count() <= 1);
    for (size_t attachmentRenderPassIndex : depthStencilAttachmentIndices)
    {
        UpdateRenderPassDepthStencilPerfCounters(createInfo, attachmentRenderPassIndex,
                                                 countersOut);
    }

    UpdateRenderPassDepthStencilResolvePerfCounters(createInfo, depthStencilResolve, countersOut);

    // Determine unresolve counters from the render pass desc, to avoid making guesses from subpass
    // count etc.
    countersOut->colorAttachmentUnresolves += desc.getColorUnresolveAttachmentMask().count();
    countersOut->depthAttachmentUnresolves += desc.hasDepthUnresolveAttachment() ? 1 : 0;
    countersOut->stencilAttachmentUnresolves += desc.hasStencilUnresolveAttachment() ? 1 : 0;
}

angle::Result InitializeRenderPassFromDesc(ContextVk *contextVk,
                                           const RenderPassDesc &desc,
                                           const AttachmentOpsArray &ops,
                                           vk::RenderPassHelper *renderPassHelper)
{
    RendererVk *renderer = contextVk->getRenderer();

    constexpr VkAttachmentReference kUnusedAttachment   = {VK_ATTACHMENT_UNUSED,
                                                         VK_IMAGE_LAYOUT_UNDEFINED};
    constexpr VkAttachmentReference2 kUnusedAttachment2 = {
        VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2_KHR, nullptr, VK_ATTACHMENT_UNUSED,
        VK_IMAGE_LAYOUT_UNDEFINED, 0};

    // Unpack the packed and split representation into the format required by Vulkan.
    gl::DrawBuffersVector<VkAttachmentReference> colorAttachmentRefs;
    gl::DrawBuffersVector<VkAttachmentReference> colorResolveAttachmentRefs;
    VkAttachmentReference depthStencilAttachmentRef            = kUnusedAttachment;
    VkAttachmentReference2KHR depthStencilResolveAttachmentRef = kUnusedAttachment2;

    // The list of attachments includes all non-resolve and resolve attachments.
    FramebufferAttachmentArray<VkAttachmentDescription> attachmentDescs;

    // Track invalidated attachments so their resolve attachments can be invalidated as well.
    // Resolve attachments can be removed in that case if the render pass has only one subpass
    // (which is the case if there are no unresolve attachments).
    gl::DrawBufferMask isColorInvalidated;
    bool isDepthInvalidated   = false;
    bool isStencilInvalidated = false;
    const bool hasUnresolveAttachments =
        desc.getColorUnresolveAttachmentMask().any() || desc.hasDepthStencilUnresolveAttachment();
    const bool canRemoveResolveAttachments = !hasUnresolveAttachments;

    // Pack color attachments
    PackedAttachmentIndex attachmentCount(0);
    for (uint32_t colorIndexGL = 0; colorIndexGL < desc.colorAttachmentRange(); ++colorIndexGL)
    {
        // Vulkan says:
        //
        // > Each element of the pColorAttachments array corresponds to an output location in the
        // > shader, i.e. if the shader declares an output variable decorated with a Location value
        // > of X, then it uses the attachment provided in pColorAttachments[X].
        //
        // This means that colorAttachmentRefs is indexed by colorIndexGL.  Where the color
        // attachment is disabled, a reference with VK_ATTACHMENT_UNUSED is given.

        if (!desc.isColorAttachmentEnabled(colorIndexGL))
        {
            colorAttachmentRefs.push_back(kUnusedAttachment);
            continue;
        }

        angle::FormatID formatID = desc[colorIndexGL];
        ASSERT(formatID != angle::FormatID::NONE);
        const vk::Format &format = renderer->getFormat(formatID);

        VkAttachmentReference colorRef;
        colorRef.attachment = attachmentCount.get();
        colorRef.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        colorAttachmentRefs.push_back(colorRef);

        UnpackAttachmentDesc(&attachmentDescs[attachmentCount.get()], format, desc.samples(),
                             ops[attachmentCount]);

        isColorInvalidated.set(colorIndexGL, ops[attachmentCount].isInvalidated);

        ++attachmentCount;
    }

    // Pack depth/stencil attachment, if any
    if (desc.hasDepthStencilAttachment())
    {
        uint32_t depthStencilIndexGL = static_cast<uint32_t>(desc.depthStencilAttachmentIndex());

        angle::FormatID formatID = desc[depthStencilIndexGL];
        ASSERT(formatID != angle::FormatID::NONE);
        const vk::Format &format = renderer->getFormat(formatID);

        depthStencilAttachmentRef.attachment = attachmentCount.get();
        depthStencilAttachmentRef.layout     = ConvertImageLayoutToVkImageLayout(
            static_cast<ImageLayout>(ops[attachmentCount].initialLayout));

        UnpackAttachmentDesc(&attachmentDescs[attachmentCount.get()], format, desc.samples(),
                             ops[attachmentCount]);

        isDepthInvalidated   = ops[attachmentCount].isInvalidated;
        isStencilInvalidated = ops[attachmentCount].isStencilInvalidated;

        ++attachmentCount;
    }

    // Pack color resolve attachments
    const uint32_t nonResolveAttachmentCount = attachmentCount.get();
    for (uint32_t colorIndexGL = 0; colorIndexGL < desc.colorAttachmentRange(); ++colorIndexGL)
    {
        if (!desc.hasColorResolveAttachment(colorIndexGL))
        {
            colorResolveAttachmentRefs.push_back(kUnusedAttachment);
            continue;
        }

        ASSERT(desc.isColorAttachmentEnabled(colorIndexGL));

        const vk::Format &format = renderer->getFormat(desc[colorIndexGL]);

        VkAttachmentReference colorRef;
        colorRef.attachment = attachmentCount.get();
        colorRef.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        // If color attachment is invalidated, try to remove its resolve attachment altogether.
        if (canRemoveResolveAttachments && isColorInvalidated.test(colorIndexGL))
        {
            colorResolveAttachmentRefs.push_back(kUnusedAttachment);
        }
        else
        {
            colorResolveAttachmentRefs.push_back(colorRef);
        }

        UnpackColorResolveAttachmentDesc(&attachmentDescs[attachmentCount.get()], format,
                                         desc.hasColorUnresolveAttachment(colorIndexGL),
                                         isColorInvalidated.test(colorIndexGL));

        ++attachmentCount;
    }

    // Pack depth/stencil resolve attachment, if any
    if (desc.hasDepthStencilResolveAttachment())
    {
        ASSERT(desc.hasDepthStencilAttachment());

        uint32_t depthStencilIndexGL = static_cast<uint32_t>(desc.depthStencilAttachmentIndex());

        const vk::Format &format         = renderer->getFormat(desc[depthStencilIndexGL]);
        const angle::Format &angleFormat = format.intendedFormat();

        // Treat missing aspect as invalidated for the purpose of the resolve attachment.
        if (angleFormat.depthBits == 0)
        {
            isDepthInvalidated = true;
        }
        if (angleFormat.stencilBits == 0)
        {
            isStencilInvalidated = true;
        }

        depthStencilResolveAttachmentRef.attachment = attachmentCount.get();
        depthStencilResolveAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        depthStencilResolveAttachmentRef.aspectMask = 0;

        if (!isDepthInvalidated)
        {
            depthStencilResolveAttachmentRef.aspectMask |= VK_IMAGE_ASPECT_DEPTH_BIT;
        }
        if (!isStencilInvalidated)
        {
            depthStencilResolveAttachmentRef.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
        }

        UnpackDepthStencilResolveAttachmentDesc(
            &attachmentDescs[attachmentCount.get()], format, desc.hasDepthUnresolveAttachment(),
            desc.hasStencilUnresolveAttachment(), isDepthInvalidated, isStencilInvalidated);

        ++attachmentCount;
    }

    SubpassVector<VkSubpassDescription> subpassDesc;

    // If any attachment needs to be unresolved, create an initial subpass for this purpose.  Note
    // that the following arrays are used in initializing a VkSubpassDescription in subpassDesc,
    // which is in turn used in VkRenderPassCreateInfo below.  That is why they are declared in the
    // same scope.
    gl::DrawBuffersVector<VkAttachmentReference> unresolveColorAttachmentRefs;
    VkAttachmentReference unresolveDepthStencilAttachmentRef = kUnusedAttachment;
    FramebufferAttachmentsVector<VkAttachmentReference> unresolveInputAttachmentRefs;
    FramebufferAttachmentsVector<uint32_t> unresolvePreserveAttachmentRefs;
    if (hasUnresolveAttachments)
    {
        subpassDesc.push_back({});
        InitializeUnresolveSubpass(
            desc, colorAttachmentRefs, colorResolveAttachmentRefs, depthStencilAttachmentRef,
            depthStencilResolveAttachmentRef, &unresolveColorAttachmentRefs,
            &unresolveDepthStencilAttachmentRef, &unresolveInputAttachmentRefs,
            &unresolvePreserveAttachmentRefs, &subpassDesc.back());
    }

    subpassDesc.push_back({});
    VkSubpassDescription *applicationSubpass = &subpassDesc.back();

    applicationSubpass->flags                = 0;
    applicationSubpass->pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS;
    applicationSubpass->inputAttachmentCount = 0;
    applicationSubpass->pInputAttachments    = nullptr;
    applicationSubpass->colorAttachmentCount = static_cast<uint32_t>(colorAttachmentRefs.size());
    applicationSubpass->pColorAttachments    = colorAttachmentRefs.data();
    applicationSubpass->pResolveAttachments  = attachmentCount.get() > nonResolveAttachmentCount
                                                  ? colorResolveAttachmentRefs.data()
                                                  : nullptr;
    applicationSubpass->pDepthStencilAttachment =
        (depthStencilAttachmentRef.attachment != VK_ATTACHMENT_UNUSED ? &depthStencilAttachmentRef
                                                                      : nullptr);
    applicationSubpass->preserveAttachmentCount = 0;
    applicationSubpass->pPreserveAttachments    = nullptr;

    // If depth/stencil is to be resolved, add a VkSubpassDescriptionDepthStencilResolve to the
    // pNext chain of the subpass description.  Note that we need a VkSubpassDescription2KHR to have
    // a pNext pointer.  CreateRenderPass2 is called to convert the data structures here to those
    // specified by VK_KHR_create_renderpass2 for this purpose.
    VkSubpassDescriptionDepthStencilResolve depthStencilResolve = {};
    if (desc.hasDepthStencilResolveAttachment())
    {
        depthStencilResolve.sType = VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_DEPTH_STENCIL_RESOLVE;
        depthStencilResolve.depthResolveMode = desc.hasDepthResolveAttachment()
                                                   ? VK_RESOLVE_MODE_SAMPLE_ZERO_BIT
                                                   : VK_RESOLVE_MODE_NONE;
        depthStencilResolve.stencilResolveMode = desc.hasStencilResolveAttachment()
                                                     ? VK_RESOLVE_MODE_SAMPLE_ZERO_BIT
                                                     : VK_RESOLVE_MODE_NONE;

        // If depth/stencil attachment is invalidated, try to remove its resolve attachment
        // altogether.
        const bool removeDepthStencilResolve =
            canRemoveResolveAttachments && isDepthInvalidated && isStencilInvalidated;
        if (!removeDepthStencilResolve)
        {
            depthStencilResolve.pDepthStencilResolveAttachment = &depthStencilResolveAttachmentRef;
        }
    }

    std::vector<VkSubpassDependency> subpassDependencies;
    if (hasUnresolveAttachments)
    {
        InitializeUnresolveSubpassDependencies(
            subpassDesc, desc.getColorUnresolveAttachmentMask().any(),
            desc.hasDepthStencilUnresolveAttachment(), &subpassDependencies);
    }

    VkRenderPassCreateInfo createInfo = {};
    createInfo.sType                  = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    createInfo.flags                  = 0;
    createInfo.attachmentCount        = attachmentCount.get();
    createInfo.pAttachments           = attachmentDescs.data();
    createInfo.subpassCount           = static_cast<uint32_t>(subpassDesc.size());
    createInfo.pSubpasses             = subpassDesc.data();
    createInfo.dependencyCount        = 0;
    createInfo.pDependencies          = nullptr;

    if (!subpassDependencies.empty())
    {
        createInfo.dependencyCount = static_cast<uint32_t>(subpassDependencies.size());
        createInfo.pDependencies   = subpassDependencies.data();
    }

    // If depth/stencil resolve is used, we need to create the render pass with
    // vkCreateRenderPass2KHR.
    if (depthStencilResolve.pDepthStencilResolveAttachment != nullptr)
    {
        ANGLE_TRY(CreateRenderPass2(
            contextVk, createInfo, depthStencilResolve, desc.hasDepthUnresolveAttachment(),
            desc.hasStencilUnresolveAttachment(), &renderPassHelper->getRenderPass()));
    }
    else
    {
        ANGLE_VK_TRY(contextVk,
                     renderPassHelper->getRenderPass().init(contextVk->getDevice(), createInfo));
    }

    // Calculate perf counters associated with this render pass, such as load/store ops, unresolve
    // and resolve operations etc.  This information is taken out of the render pass create info.
    // Depth/stencil resolve attachment uses RenderPass2 structures, so it's passed in separately.
    UpdateRenderPassPerfCounters(desc, createInfo, depthStencilResolve,
                                 &renderPassHelper->getPerfCounters());

    return angle::Result::Continue;
}

void GetRenderPassAndUpdateCounters(ContextVk *contextVk,
                                    bool updatePerfCounters,
                                    vk::RenderPassHelper *renderPassHelper,
                                    vk::RenderPass **renderPassOut)
{
    *renderPassOut = &renderPassHelper->getRenderPass();
    if (updatePerfCounters)
    {
        PerfCounters &counters                   = contextVk->getPerfCounters();
        const RenderPassPerfCounters &rpCounters = renderPassHelper->getPerfCounters();

        counters.depthClears += rpCounters.depthClears;
        counters.depthLoads += rpCounters.depthLoads;
        counters.depthStores += rpCounters.depthStores;
        counters.stencilClears += rpCounters.stencilClears;
        counters.stencilLoads += rpCounters.stencilLoads;
        counters.stencilStores += rpCounters.stencilStores;
        counters.colorAttachmentUnresolves += rpCounters.colorAttachmentUnresolves;
        counters.colorAttachmentResolves += rpCounters.colorAttachmentResolves;
        counters.depthAttachmentUnresolves += rpCounters.depthAttachmentUnresolves;
        counters.depthAttachmentResolves += rpCounters.depthAttachmentResolves;
        counters.stencilAttachmentUnresolves += rpCounters.stencilAttachmentUnresolves;
        counters.stencilAttachmentResolves += rpCounters.stencilAttachmentResolves;
        counters.readOnlyDepthStencilRenderPasses += rpCounters.readOnlyDepthStencil;
    }
}

void InitializeSpecializationInfo(
    const vk::SpecializationConstants &specConsts,
    vk::SpecializationConstantMap<VkSpecializationMapEntry> *specializationEntriesOut,
    VkSpecializationInfo *specializationInfoOut)
{
    // Collect specialization constants.
    for (const sh::vk::SpecializationConstantId id :
         angle::AllEnums<sh::vk::SpecializationConstantId>())
    {
        (*specializationEntriesOut)[id].constantID = static_cast<uint32_t>(id);
        switch (id)
        {
            case sh::vk::SpecializationConstantId::LineRasterEmulation:
                (*specializationEntriesOut)[id].offset =
                    offsetof(vk::SpecializationConstants, lineRasterEmulation);
                (*specializationEntriesOut)[id].size = sizeof(specConsts.lineRasterEmulation);
                break;
            case sh::vk::SpecializationConstantId::SurfaceRotation:
                (*specializationEntriesOut)[id].offset =
                    offsetof(vk::SpecializationConstants, surfaceRotation);
                (*specializationEntriesOut)[id].size = sizeof(specConsts.surfaceRotation);
                break;
            default:
                UNREACHABLE();
                break;
        }
    }

    specializationInfoOut->mapEntryCount = static_cast<uint32_t>(specializationEntriesOut->size());
    specializationInfoOut->pMapEntries   = specializationEntriesOut->data();
    specializationInfoOut->dataSize      = sizeof(specConsts);
    specializationInfoOut->pData         = &specConsts;
}

// Utility for setting a value on a packed 4-bit integer array.
template <typename SrcT>
void Int4Array_Set(uint8_t *arrayBytes, uint32_t arrayIndex, SrcT value)
{
    uint32_t byteIndex = arrayIndex >> 1;
    ASSERT(value < 16);

    if ((arrayIndex & 1) == 0)
    {
        arrayBytes[byteIndex] &= 0xF0;
        arrayBytes[byteIndex] |= static_cast<uint8_t>(value);
    }
    else
    {
        arrayBytes[byteIndex] &= 0x0F;
        arrayBytes[byteIndex] |= static_cast<uint8_t>(value) << 4;
    }
}

// Utility for getting a value from a packed 4-bit integer array.
template <typename DestT>
DestT Int4Array_Get(const uint8_t *arrayBytes, uint32_t arrayIndex)
{
    uint32_t byteIndex = arrayIndex >> 1;

    if ((arrayIndex & 1) == 0)
    {
        return static_cast<DestT>(arrayBytes[byteIndex] & 0xF);
    }
    else
    {
        return static_cast<DestT>(arrayBytes[byteIndex] >> 4);
    }
}

// When converting a byte number to a transition bit index we can shift instead of divide.
constexpr size_t kTransitionByteShift = Log2(kGraphicsPipelineDirtyBitBytes);

// When converting a number of bits offset to a transition bit index we can also shift.
constexpr size_t kBitsPerByte        = 8;
constexpr size_t kTransitionBitShift = kTransitionByteShift + Log2(kBitsPerByte);

// Helper macro to map from a PipelineDesc struct and field to a dirty bit index.
// Uses the 'offsetof' macro to compute the offset 'Member' within the PipelineDesc
// and the offset of 'Field' within 'Member'. We can optimize the dirty bit setting by computing
// the shifted dirty bit at compile time instead of calling "set".
#define ANGLE_GET_TRANSITION_BIT(Member, Field)                                      \
    ((offsetof(GraphicsPipelineDesc, Member) + offsetof(decltype(Member), Field)) >> \
     kTransitionByteShift)

// Indexed dirty bits cannot be entirely computed at compile time since the index is passed to
// the update function.
#define ANGLE_GET_INDEXED_TRANSITION_BIT(Member, Field, Index, BitWidth) \
    (((BitWidth * Index) >> kTransitionBitShift) + ANGLE_GET_TRANSITION_BIT(Member, Field))

constexpr angle::PackedEnumMap<gl::ComponentType, VkFormat> kMismatchedComponentTypeMap = {{
    {gl::ComponentType::Float, VK_FORMAT_R32G32B32A32_SFLOAT},
    {gl::ComponentType::Int, VK_FORMAT_R32G32B32A32_SINT},
    {gl::ComponentType::UnsignedInt, VK_FORMAT_R32G32B32A32_UINT},
}};
}  // anonymous namespace

// RenderPassDesc implementation.
RenderPassDesc::RenderPassDesc()
{
    memset(this, 0, sizeof(RenderPassDesc));
}

RenderPassDesc::~RenderPassDesc() = default;

RenderPassDesc::RenderPassDesc(const RenderPassDesc &other)
{
    memcpy(this, &other, sizeof(RenderPassDesc));
}

void RenderPassDesc::setSamples(GLint samples)
{
    SetBitField(mLogSamples, PackSampleCount(samples));
}

void RenderPassDesc::packColorAttachment(size_t colorIndexGL, angle::FormatID formatID)
{
    ASSERT(colorIndexGL < mAttachmentFormats.size());
    static_assert(angle::kNumANGLEFormats < std::numeric_limits<uint8_t>::max(),
                  "Too many ANGLE formats to fit in uint8_t");
    // Force the user to pack the depth/stencil attachment last.
    ASSERT(mHasDepthStencilAttachment == false);
    // This function should only be called for enabled GL color attachments.
    ASSERT(formatID != angle::FormatID::NONE);

    uint8_t &packedFormat = mAttachmentFormats[colorIndexGL];
    SetBitField(packedFormat, formatID);

    // Set color attachment range such that it covers the range from index 0 through last active
    // index.  Additionally, a few bits at the end of the array are used for other purposes, so we
    // need the last format to use only a few bits.  These are the reasons why we need depth/stencil
    // to be packed last.
    SetBitField(mColorAttachmentRange, std::max<size_t>(mColorAttachmentRange, colorIndexGL + 1));
}

void RenderPassDesc::packColorAttachmentGap(size_t colorIndexGL)
{
    ASSERT(colorIndexGL < mAttachmentFormats.size());
    static_assert(angle::kNumANGLEFormats < std::numeric_limits<uint8_t>::max(),
                  "Too many ANGLE formats to fit in uint8_t");
    // Force the user to pack the depth/stencil attachment last.
    ASSERT(mHasDepthStencilAttachment == false);

    // Use NONE as a flag for gaps in GL color attachments.
    uint8_t &packedFormat = mAttachmentFormats[colorIndexGL];
    SetBitField(packedFormat, angle::FormatID::NONE);
}

void RenderPassDesc::packDepthStencilAttachment(angle::FormatID formatID)
{
    // Though written as Count, there is only ever a single depth/stencil attachment.
    ASSERT(mHasDepthStencilAttachment == false);

    // 3 bits are used to store the depth/stencil attachment format.
    ASSERT(static_cast<uint8_t>(formatID) <= kDepthStencilFormatStorageMask);

    size_t index = depthStencilAttachmentIndex();
    ASSERT(index < mAttachmentFormats.size());

    uint8_t &packedFormat = mAttachmentFormats[index];
    SetBitField(packedFormat, formatID);

    mHasDepthStencilAttachment = true;
}

void RenderPassDesc::packColorResolveAttachment(size_t colorIndexGL)
{
    ASSERT(isColorAttachmentEnabled(colorIndexGL));
    ASSERT(!mColorResolveAttachmentMask.test(colorIndexGL));
    ASSERT(mLogSamples > 0);
    mColorResolveAttachmentMask.set(colorIndexGL);
}

void RenderPassDesc::removeColorResolveAttachment(size_t colorIndexGL)
{
    ASSERT(mColorResolveAttachmentMask.test(colorIndexGL));
    mColorResolveAttachmentMask.reset(colorIndexGL);
}

void RenderPassDesc::packColorUnresolveAttachment(size_t colorIndexGL)
{
    mColorUnresolveAttachmentMask.set(colorIndexGL);
}

void RenderPassDesc::removeColorUnresolveAttachment(size_t colorIndexGL)
{
    mColorUnresolveAttachmentMask.reset(colorIndexGL);
}

void RenderPassDesc::packDepthStencilResolveAttachment(bool resolveDepth, bool resolveStencil)
{
    ASSERT(hasDepthStencilAttachment());
    ASSERT(!hasDepthStencilResolveAttachment());

    static_assert((kDepthStencilFormatStorageMask & (kResolveDepthFlag | kResolveStencilFlag)) == 0,
                  "Collision in depth/stencil format and flag bits");

    if (resolveDepth)
    {
        mAttachmentFormats.back() |= kResolveDepthFlag;
    }
    if (resolveStencil)
    {
        mAttachmentFormats.back() |= kResolveStencilFlag;
    }
}

void RenderPassDesc::packDepthStencilUnresolveAttachment(bool unresolveDepth, bool unresolveStencil)
{
    ASSERT(hasDepthStencilAttachment());

    static_assert(
        (kDepthStencilFormatStorageMask & (kUnresolveDepthFlag | kUnresolveStencilFlag)) == 0,
        "Collision in depth/stencil format and flag bits");

    if (unresolveDepth)
    {
        mAttachmentFormats.back() |= kUnresolveDepthFlag;
    }
    if (unresolveStencil)
    {
        mAttachmentFormats.back() |= kUnresolveStencilFlag;
    }
}

void RenderPassDesc::removeDepthStencilUnresolveAttachment()
{
    mAttachmentFormats.back() &= ~(kUnresolveDepthFlag | kUnresolveStencilFlag);
}

RenderPassDesc &RenderPassDesc::operator=(const RenderPassDesc &other)
{
    memcpy(this, &other, sizeof(RenderPassDesc));
    return *this;
}

size_t RenderPassDesc::hash() const
{
    return angle::ComputeGenericHash(*this);
}

bool RenderPassDesc::isColorAttachmentEnabled(size_t colorIndexGL) const
{
    angle::FormatID formatID = operator[](colorIndexGL);
    return formatID != angle::FormatID::NONE;
}

size_t RenderPassDesc::attachmentCount() const
{
    size_t colorAttachmentCount = 0;
    for (size_t i = 0; i < mColorAttachmentRange; ++i)
    {
        colorAttachmentCount += isColorAttachmentEnabled(i);
    }

    // Note that there are no gaps in depth/stencil attachments.  In fact there is a maximum of 1 of
    // it + 1 for its resolve attachment.
    size_t depthStencilCount        = hasDepthStencilAttachment() ? 1 : 0;
    size_t depthStencilResolveCount = hasDepthStencilResolveAttachment() ? 1 : 0;
    return colorAttachmentCount + mColorResolveAttachmentMask.count() + depthStencilCount +
           depthStencilResolveCount;
}

bool operator==(const RenderPassDesc &lhs, const RenderPassDesc &rhs)
{
    return (memcmp(&lhs, &rhs, sizeof(RenderPassDesc)) == 0);
}

// GraphicsPipelineDesc implementation.
// Use aligned allocation and free so we can use the alignas keyword.
void *GraphicsPipelineDesc::operator new(std::size_t size)
{
    return angle::AlignedAlloc(size, 32);
}

void GraphicsPipelineDesc::operator delete(void *ptr)
{
    return angle::AlignedFree(ptr);
}

GraphicsPipelineDesc::GraphicsPipelineDesc()
{
    memset(this, 0, sizeof(GraphicsPipelineDesc));
}

GraphicsPipelineDesc::~GraphicsPipelineDesc() = default;

GraphicsPipelineDesc::GraphicsPipelineDesc(const GraphicsPipelineDesc &other)
{
    memcpy(this, &other, sizeof(GraphicsPipelineDesc));
}

GraphicsPipelineDesc &GraphicsPipelineDesc::operator=(const GraphicsPipelineDesc &other)
{
    memcpy(this, &other, sizeof(GraphicsPipelineDesc));
    return *this;
}

size_t GraphicsPipelineDesc::hash() const
{
    return angle::ComputeGenericHash(*this);
}

bool GraphicsPipelineDesc::operator==(const GraphicsPipelineDesc &other) const
{
    return (memcmp(this, &other, sizeof(GraphicsPipelineDesc)) == 0);
}

// TODO(jmadill): We should prefer using Packed GLenums. http://anglebug.com/2169

// Initialize PSO states, it is consistent with initial value of gl::State
void GraphicsPipelineDesc::initDefaults()
{
    // Set all vertex input attributes to default, the default format is Float
    angle::FormatID defaultFormat = GetCurrentValueFormatID(gl::VertexAttribType::Float);
    for (PackedAttribDesc &packedAttrib : mVertexInputAttribs.attribs)
    {
        SetBitField(packedAttrib.stride, 0);
        SetBitField(packedAttrib.divisor, 0);
        SetBitField(packedAttrib.format, defaultFormat);
        SetBitField(packedAttrib.compressed, 0);
        SetBitField(packedAttrib.offset, 0);
    }

    mRasterizationAndMultisampleStateInfo.bits.subpass                    = 0;
    mRasterizationAndMultisampleStateInfo.bits.depthClampEnable           = 0;
    mRasterizationAndMultisampleStateInfo.bits.rasterizationDiscardEnable = 0;
    SetBitField(mRasterizationAndMultisampleStateInfo.bits.polygonMode, VK_POLYGON_MODE_FILL);
    SetBitField(mRasterizationAndMultisampleStateInfo.bits.cullMode, VK_CULL_MODE_BACK_BIT);
    SetBitField(mRasterizationAndMultisampleStateInfo.bits.frontFace,
                VK_FRONT_FACE_COUNTER_CLOCKWISE);
    mRasterizationAndMultisampleStateInfo.bits.depthBiasEnable    = 0;
    mRasterizationAndMultisampleStateInfo.depthBiasConstantFactor = 0.0f;
    mRasterizationAndMultisampleStateInfo.depthBiasClamp          = 0.0f;
    mRasterizationAndMultisampleStateInfo.depthBiasSlopeFactor    = 0.0f;
    mRasterizationAndMultisampleStateInfo.lineWidth               = 1.0f;

    mRasterizationAndMultisampleStateInfo.bits.rasterizationSamples = 1;
    mRasterizationAndMultisampleStateInfo.bits.sampleShadingEnable  = 0;
    mRasterizationAndMultisampleStateInfo.minSampleShading          = 1.0f;
    for (uint32_t &sampleMask : mRasterizationAndMultisampleStateInfo.sampleMask)
    {
        sampleMask = 0xFFFFFFFF;
    }
    mRasterizationAndMultisampleStateInfo.bits.alphaToCoverageEnable = 0;
    mRasterizationAndMultisampleStateInfo.bits.alphaToOneEnable      = 0;

    mDepthStencilStateInfo.enable.depthTest  = 0;
    mDepthStencilStateInfo.enable.depthWrite = 1;
    SetBitField(mDepthStencilStateInfo.depthCompareOpAndSurfaceRotation.depthCompareOp,
                VK_COMPARE_OP_LESS);
    mDepthStencilStateInfo.enable.depthBoundsTest = 0;
    mDepthStencilStateInfo.enable.stencilTest     = 0;
    mDepthStencilStateInfo.minDepthBounds         = 0.0f;
    mDepthStencilStateInfo.maxDepthBounds         = 0.0f;
    SetBitField(mDepthStencilStateInfo.front.ops.fail, VK_STENCIL_OP_KEEP);
    SetBitField(mDepthStencilStateInfo.front.ops.pass, VK_STENCIL_OP_KEEP);
    SetBitField(mDepthStencilStateInfo.front.ops.depthFail, VK_STENCIL_OP_KEEP);
    SetBitField(mDepthStencilStateInfo.front.ops.compare, VK_COMPARE_OP_ALWAYS);
    SetBitField(mDepthStencilStateInfo.front.compareMask, 0xFF);
    SetBitField(mDepthStencilStateInfo.front.writeMask, 0xFF);
    mDepthStencilStateInfo.frontStencilReference = 0;
    SetBitField(mDepthStencilStateInfo.back.ops.fail, VK_STENCIL_OP_KEEP);
    SetBitField(mDepthStencilStateInfo.back.ops.pass, VK_STENCIL_OP_KEEP);
    SetBitField(mDepthStencilStateInfo.back.ops.depthFail, VK_STENCIL_OP_KEEP);
    SetBitField(mDepthStencilStateInfo.back.ops.compare, VK_COMPARE_OP_ALWAYS);
    SetBitField(mDepthStencilStateInfo.back.compareMask, 0xFF);
    SetBitField(mDepthStencilStateInfo.back.writeMask, 0xFF);
    mDepthStencilStateInfo.backStencilReference = 0;

    mDepthStencilStateInfo.depthCompareOpAndSurfaceRotation.surfaceRotation =
        static_cast<uint8_t>(SurfaceRotation::Identity);

    PackedInputAssemblyAndColorBlendStateInfo &inputAndBlend = mInputAssemblyAndColorBlendStateInfo;
    inputAndBlend.logic.opEnable                             = 0;
    inputAndBlend.logic.op          = static_cast<uint32_t>(VK_LOGIC_OP_CLEAR);
    inputAndBlend.blendEnableMask   = 0;
    inputAndBlend.blendConstants[0] = 0.0f;
    inputAndBlend.blendConstants[1] = 0.0f;
    inputAndBlend.blendConstants[2] = 0.0f;
    inputAndBlend.blendConstants[3] = 0.0f;

    VkFlags allColorBits = (VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                            VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT);

    for (uint32_t colorIndexGL = 0; colorIndexGL < gl::IMPLEMENTATION_MAX_DRAW_BUFFERS;
         ++colorIndexGL)
    {
        Int4Array_Set(inputAndBlend.colorWriteMaskBits, colorIndexGL, allColorBits);
    }

    PackedColorBlendAttachmentState blendAttachmentState;
    SetBitField(blendAttachmentState.srcColorBlendFactor, VK_BLEND_FACTOR_ONE);
    SetBitField(blendAttachmentState.dstColorBlendFactor, VK_BLEND_FACTOR_ZERO);
    SetBitField(blendAttachmentState.colorBlendOp, VK_BLEND_OP_ADD);
    SetBitField(blendAttachmentState.srcAlphaBlendFactor, VK_BLEND_FACTOR_ONE);
    SetBitField(blendAttachmentState.dstAlphaBlendFactor, VK_BLEND_FACTOR_ZERO);
    SetBitField(blendAttachmentState.alphaBlendOp, VK_BLEND_OP_ADD);

    std::fill(&inputAndBlend.attachments[0],
              &inputAndBlend.attachments[gl::IMPLEMENTATION_MAX_DRAW_BUFFERS],
              blendAttachmentState);

    inputAndBlend.primitive.topology = static_cast<uint16_t>(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    inputAndBlend.primitive.restartEnable = 0;

    // Viewport and scissor will be set to valid values when framebuffer being binded
    mViewport.x        = 0.0f;
    mViewport.y        = 0.0f;
    mViewport.width    = 0.0f;
    mViewport.height   = 0.0f;
    mViewport.minDepth = 0.0f;
    mViewport.maxDepth = 1.0f;

    mScissor.offset.x      = 0;
    mScissor.offset.y      = 0;
    mScissor.extent.width  = 0;
    mScissor.extent.height = 0;
}

angle::Result GraphicsPipelineDesc::initializePipeline(
    ContextVk *contextVk,
    const vk::PipelineCache &pipelineCacheVk,
    const RenderPass &compatibleRenderPass,
    const PipelineLayout &pipelineLayout,
    const gl::AttributesMask &activeAttribLocationsMask,
    const gl::ComponentTypeMask &programAttribsTypeMask,
    const ShaderModule *vertexModule,
    const ShaderModule *fragmentModule,
    const ShaderModule *geometryModule,
    const vk::SpecializationConstants specConsts,
    Pipeline *pipelineOut) const
{
    angle::FixedVector<VkPipelineShaderStageCreateInfo, 3> shaderStages;
    VkPipelineVertexInputStateCreateInfo vertexInputState     = {};
    VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = {};
    VkPipelineViewportStateCreateInfo viewportState           = {};
    VkPipelineRasterizationStateCreateInfo rasterState        = {};
    VkPipelineMultisampleStateCreateInfo multisampleState     = {};
    VkPipelineDepthStencilStateCreateInfo depthStencilState   = {};
    gl::DrawBuffersArray<VkPipelineColorBlendAttachmentState> blendAttachmentState;
    VkPipelineColorBlendStateCreateInfo blendState = {};
    VkSpecializationInfo specializationInfo        = {};
    VkGraphicsPipelineCreateInfo createInfo        = {};

    vk::SpecializationConstantMap<VkSpecializationMapEntry> specializationEntries;
    InitializeSpecializationInfo(specConsts, &specializationEntries, &specializationInfo);

    // Vertex shader is always expected to be present.
    ASSERT(vertexModule != nullptr);
    VkPipelineShaderStageCreateInfo vertexStage = {};
    SetPipelineShaderStageInfo(VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                               VK_SHADER_STAGE_VERTEX_BIT, vertexModule->getHandle(),
                               specializationInfo, &vertexStage);
    shaderStages.push_back(vertexStage);

    if (geometryModule)
    {
        VkPipelineShaderStageCreateInfo geometryStage = {};
        SetPipelineShaderStageInfo(VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                                   VK_SHADER_STAGE_GEOMETRY_BIT, geometryModule->getHandle(),
                                   specializationInfo, &geometryStage);
        shaderStages.push_back(geometryStage);
    }

    // Fragment shader is optional.
    // anglebug.com/3509 - Don't compile the fragment shader if rasterizationDiscardEnable = true
    if (fragmentModule && !mRasterizationAndMultisampleStateInfo.bits.rasterizationDiscardEnable)
    {
        VkPipelineShaderStageCreateInfo fragmentStage = {};
        SetPipelineShaderStageInfo(VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                                   VK_SHADER_STAGE_FRAGMENT_BIT, fragmentModule->getHandle(),
                                   specializationInfo, &fragmentStage);
        shaderStages.push_back(fragmentStage);
    }

    // TODO(jmadill): Possibly use different path for ES 3.1 split bindings/attribs.
    gl::AttribArray<VkVertexInputBindingDescription> bindingDescs;
    gl::AttribArray<VkVertexInputAttributeDescription> attributeDescs;

    uint32_t vertexAttribCount = 0;

    size_t unpackedSize = sizeof(shaderStages) + sizeof(vertexInputState) +
                          sizeof(inputAssemblyState) + sizeof(viewportState) + sizeof(rasterState) +
                          sizeof(multisampleState) + sizeof(depthStencilState) +
                          sizeof(blendAttachmentState) + sizeof(blendState) + sizeof(bindingDescs) +
                          sizeof(attributeDescs);
    ANGLE_UNUSED_VARIABLE(unpackedSize);

    gl::AttribArray<VkVertexInputBindingDivisorDescriptionEXT> divisorDesc;
    VkPipelineVertexInputDivisorStateCreateInfoEXT divisorState = {};
    divisorState.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_DIVISOR_STATE_CREATE_INFO_EXT;
    divisorState.pVertexBindingDivisors = divisorDesc.data();
    for (size_t attribIndexSizeT : activeAttribLocationsMask)
    {
        const uint32_t attribIndex = static_cast<uint32_t>(attribIndexSizeT);

        VkVertexInputBindingDescription &bindingDesc  = bindingDescs[vertexAttribCount];
        VkVertexInputAttributeDescription &attribDesc = attributeDescs[vertexAttribCount];
        const PackedAttribDesc &packedAttrib          = mVertexInputAttribs.attribs[attribIndex];

        bindingDesc.binding = attribIndex;
        bindingDesc.stride  = static_cast<uint32_t>(packedAttrib.stride);
        if (packedAttrib.divisor != 0)
        {
            bindingDesc.inputRate = static_cast<VkVertexInputRate>(VK_VERTEX_INPUT_RATE_INSTANCE);
            divisorDesc[divisorState.vertexBindingDivisorCount].binding = bindingDesc.binding;
            divisorDesc[divisorState.vertexBindingDivisorCount].divisor = packedAttrib.divisor;
            ++divisorState.vertexBindingDivisorCount;
        }
        else
        {
            bindingDesc.inputRate = static_cast<VkVertexInputRate>(VK_VERTEX_INPUT_RATE_VERTEX);
        }

        // Get the corresponding VkFormat for the attrib's format.
        angle::FormatID formatID         = static_cast<angle::FormatID>(packedAttrib.format);
        const vk::Format &format         = contextVk->getRenderer()->getFormat(formatID);
        const angle::Format &angleFormat = format.intendedFormat();
        VkFormat vkFormat =
            packedAttrib.compressed ? format.vkCompressedBufferFormat : format.vkBufferFormat;

        gl::ComponentType attribType =
            GetVertexAttributeComponentType(angleFormat.isPureInt(), angleFormat.vertexAttribType);
        gl::ComponentType programAttribType =
            gl::GetComponentTypeMask(programAttribsTypeMask, attribIndex);

        if (attribType != programAttribType)
        {
            // Override the format with a compatible one.
            vkFormat = kMismatchedComponentTypeMap[programAttribType];

            bindingDesc.stride = 0;  // Prevent out-of-bounds accesses.
        }

        // The binding index could become more dynamic in ES 3.1.
        attribDesc.binding  = attribIndex;
        attribDesc.format   = vkFormat;
        attribDesc.location = static_cast<uint32_t>(attribIndex);
        attribDesc.offset   = packedAttrib.offset;

        vertexAttribCount++;
    }

    // The binding descriptions are filled in at draw time.
    vertexInputState.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputState.flags = 0;
    vertexInputState.vertexBindingDescriptionCount   = vertexAttribCount;
    vertexInputState.pVertexBindingDescriptions      = bindingDescs.data();
    vertexInputState.vertexAttributeDescriptionCount = vertexAttribCount;
    vertexInputState.pVertexAttributeDescriptions    = attributeDescs.data();
    if (divisorState.vertexBindingDivisorCount)
        vertexInputState.pNext = &divisorState;

    // Primitive topology is filled in at draw time.
    inputAssemblyState.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssemblyState.flags = 0;
    inputAssemblyState.topology =
        static_cast<VkPrimitiveTopology>(mInputAssemblyAndColorBlendStateInfo.primitive.topology);
    // http://anglebug.com/3832
    // We currently hit a VK Validation here where VUID
    // VUID-VkPipelineInputAssemblyStateCreateInfo-topology-00428 is flagged because we allow
    // primitiveRestartEnable to be true for topologies VK_PRIMITIVE_TOPOLOGY_POINT_LIST,
    // VK_PRIMITIVE_TOPOLOGY_LINE_LIST, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST
    // VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY,
    // VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY and VK_PRIMITIVE_TOPOLOGY_PATCH_LIST
    // However if we force primiteRestartEnable to FALSE we fail tests.
    // Need to identify alternate fix.
    inputAssemblyState.primitiveRestartEnable =
        static_cast<VkBool32>(mInputAssemblyAndColorBlendStateInfo.primitive.restartEnable);

    // Set initial viewport and scissor state.

    // 0-sized viewports are invalid in Vulkan.  We always use a scissor that at least matches the
    // requested viewport, so it's safe to adjust the viewport size here.
    VkViewport viewport = mViewport;
    if (viewport.width == 0)
    {
        viewport.width = 1;
    }
    if (viewport.height == 0)
    {
        viewport.height = 1;
    }

    viewportState.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.flags         = 0;
    viewportState.viewportCount = 1;
    viewportState.pViewports    = &viewport;
    viewportState.scissorCount  = 1;
    viewportState.pScissors     = IsScissorStateDynamic(mScissor) ? nullptr : &mScissor;

    const PackedRasterizationAndMultisampleStateInfo &rasterAndMS =
        mRasterizationAndMultisampleStateInfo;

    // Rasterizer state.
    rasterState.sType            = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterState.flags            = 0;
    rasterState.depthClampEnable = static_cast<VkBool32>(rasterAndMS.bits.depthClampEnable);
    rasterState.rasterizerDiscardEnable =
        static_cast<VkBool32>(rasterAndMS.bits.rasterizationDiscardEnable);
    rasterState.polygonMode             = static_cast<VkPolygonMode>(rasterAndMS.bits.polygonMode);
    rasterState.cullMode                = static_cast<VkCullModeFlags>(rasterAndMS.bits.cullMode);
    rasterState.frontFace               = static_cast<VkFrontFace>(rasterAndMS.bits.frontFace);
    rasterState.depthBiasEnable         = static_cast<VkBool32>(rasterAndMS.bits.depthBiasEnable);
    rasterState.depthBiasConstantFactor = rasterAndMS.depthBiasConstantFactor;
    rasterState.depthBiasClamp          = rasterAndMS.depthBiasClamp;
    rasterState.depthBiasSlopeFactor    = rasterAndMS.depthBiasSlopeFactor;
    rasterState.lineWidth               = rasterAndMS.lineWidth;
    const void **pNextPtr               = &rasterState.pNext;

    VkPipelineRasterizationLineStateCreateInfoEXT rasterLineState = {};
    rasterLineState.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_LINE_STATE_CREATE_INFO_EXT;
    // Enable Bresenham line rasterization if available and not multisampling.
    if (rasterAndMS.bits.rasterizationSamples <= 1 &&
        contextVk->getFeatures().bresenhamLineRasterization.enabled)
    {
        rasterLineState.lineRasterizationMode = VK_LINE_RASTERIZATION_MODE_BRESENHAM_EXT;
        *pNextPtr                             = &rasterLineState;
        pNextPtr                              = &rasterLineState.pNext;
    }

    VkPipelineRasterizationProvokingVertexStateCreateInfoEXT provokingVertexState = {};
    provokingVertexState.sType =
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_PROVOKING_VERTEX_STATE_CREATE_INFO_EXT;
    // Always set provoking vertex mode to last if available.
    if (contextVk->getFeatures().provokingVertex.enabled)
    {
        provokingVertexState.provokingVertexMode = VK_PROVOKING_VERTEX_MODE_LAST_VERTEX_EXT;
        *pNextPtr                                = &provokingVertexState;
        pNextPtr                                 = &provokingVertexState.pNext;
    }
    VkPipelineRasterizationStateStreamCreateInfoEXT rasterStreamState = {};
    rasterStreamState.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_STREAM_CREATE_INFO_EXT;
    if (contextVk->getFeatures().supportsTransformFeedbackExtension.enabled)
    {
        rasterStreamState.rasterizationStream = 0;
        rasterState.pNext                     = &rasterLineState;
    }

    // Multisample state.
    multisampleState.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampleState.flags = 0;
    multisampleState.rasterizationSamples =
        gl_vk::GetSamples(rasterAndMS.bits.rasterizationSamples);
    multisampleState.sampleShadingEnable =
        static_cast<VkBool32>(rasterAndMS.bits.sampleShadingEnable);
    multisampleState.minSampleShading = rasterAndMS.minSampleShading;
    multisampleState.pSampleMask      = rasterAndMS.sampleMask;
    multisampleState.alphaToCoverageEnable =
        static_cast<VkBool32>(rasterAndMS.bits.alphaToCoverageEnable);
    multisampleState.alphaToOneEnable = static_cast<VkBool32>(rasterAndMS.bits.alphaToOneEnable);

    // Depth/stencil state.
    depthStencilState.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencilState.flags = 0;
    depthStencilState.depthTestEnable =
        static_cast<VkBool32>(mDepthStencilStateInfo.enable.depthTest);
    depthStencilState.depthWriteEnable =
        static_cast<VkBool32>(mDepthStencilStateInfo.enable.depthWrite);
    depthStencilState.depthCompareOp = static_cast<VkCompareOp>(
        mDepthStencilStateInfo.depthCompareOpAndSurfaceRotation.depthCompareOp);
    depthStencilState.depthBoundsTestEnable =
        static_cast<VkBool32>(mDepthStencilStateInfo.enable.depthBoundsTest);
    depthStencilState.stencilTestEnable =
        static_cast<VkBool32>(mDepthStencilStateInfo.enable.stencilTest);
    UnpackStencilState(mDepthStencilStateInfo.front, mDepthStencilStateInfo.frontStencilReference,
                       &depthStencilState.front);
    UnpackStencilState(mDepthStencilStateInfo.back, mDepthStencilStateInfo.backStencilReference,
                       &depthStencilState.back);
    depthStencilState.minDepthBounds = mDepthStencilStateInfo.minDepthBounds;
    depthStencilState.maxDepthBounds = mDepthStencilStateInfo.maxDepthBounds;

    const PackedInputAssemblyAndColorBlendStateInfo &inputAndBlend =
        mInputAssemblyAndColorBlendStateInfo;

    blendState.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    blendState.flags           = 0;
    blendState.logicOpEnable   = static_cast<VkBool32>(inputAndBlend.logic.opEnable);
    blendState.logicOp         = static_cast<VkLogicOp>(inputAndBlend.logic.op);
    blendState.attachmentCount = static_cast<uint32_t>(mRenderPassDesc.colorAttachmentRange());
    blendState.pAttachments    = blendAttachmentState.data();

    // If this graphics pipeline is for the unresolve operation, correct the color attachment count
    // for that subpass.
    if ((mRenderPassDesc.getColorUnresolveAttachmentMask().any() ||
         mRenderPassDesc.hasDepthStencilUnresolveAttachment()) &&
        mRasterizationAndMultisampleStateInfo.bits.subpass == 0)
    {
        blendState.attachmentCount =
            static_cast<uint32_t>(mRenderPassDesc.getColorUnresolveAttachmentMask().count());
    }

    for (int i = 0; i < 4; i++)
    {
        blendState.blendConstants[i] = inputAndBlend.blendConstants[i];
    }

    const gl::DrawBufferMask blendEnableMask(inputAndBlend.blendEnableMask);

    // Zero-init all states.
    blendAttachmentState = {};

    for (uint32_t colorIndexGL = 0; colorIndexGL < blendState.attachmentCount; ++colorIndexGL)
    {
        VkPipelineColorBlendAttachmentState &state = blendAttachmentState[colorIndexGL];

        if (blendEnableMask[colorIndexGL])
        {
            // To avoid triggering valid usage error, blending must be disabled for formats that do
            // not have VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT feature bit set.
            // From OpenGL ES clients, this means disabling blending for integer formats.
            if (!angle::Format::Get(mRenderPassDesc[colorIndexGL]).isInt())
            {
                ASSERT(!contextVk->getRenderer()
                            ->getFormat(mRenderPassDesc[colorIndexGL])
                            .actualImageFormat()
                            .isInt());
                state.blendEnable = VK_TRUE;
                UnpackBlendAttachmentState(inputAndBlend.attachments[colorIndexGL], &state);
            }
        }
        state.colorWriteMask =
            Int4Array_Get<VkColorComponentFlags>(inputAndBlend.colorWriteMaskBits, colorIndexGL);
    }

    // Dynamic state
    angle::FixedVector<VkDynamicState, 1> dynamicStateList;
    if (IsScissorStateDynamic(mScissor))
    {
        dynamicStateList.push_back(VK_DYNAMIC_STATE_SCISSOR);
    }

    VkPipelineDynamicStateCreateInfo dynamicState = {};
    dynamicState.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStateList.size());
    dynamicState.pDynamicStates    = dynamicStateList.data();

    createInfo.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    createInfo.flags               = 0;
    createInfo.stageCount          = static_cast<uint32_t>(shaderStages.size());
    createInfo.pStages             = shaderStages.data();
    createInfo.pVertexInputState   = &vertexInputState;
    createInfo.pInputAssemblyState = &inputAssemblyState;
    createInfo.pTessellationState  = nullptr;
    createInfo.pViewportState      = &viewportState;
    createInfo.pRasterizationState = &rasterState;
    createInfo.pMultisampleState   = &multisampleState;
    createInfo.pDepthStencilState  = &depthStencilState;
    createInfo.pColorBlendState    = &blendState;
    createInfo.pDynamicState       = dynamicStateList.empty() ? nullptr : &dynamicState;
    createInfo.layout              = pipelineLayout.getHandle();
    createInfo.renderPass          = compatibleRenderPass.getHandle();
    createInfo.subpass             = mRasterizationAndMultisampleStateInfo.bits.subpass;
    createInfo.basePipelineHandle  = VK_NULL_HANDLE;
    createInfo.basePipelineIndex   = 0;

    ANGLE_VK_TRY(contextVk,
                 pipelineOut->initGraphics(contextVk->getDevice(), createInfo, pipelineCacheVk));
    return angle::Result::Continue;
}

void GraphicsPipelineDesc::updateVertexInput(GraphicsPipelineTransitionBits *transition,
                                             uint32_t attribIndex,
                                             GLuint stride,
                                             GLuint divisor,
                                             angle::FormatID format,
                                             bool compressed,
                                             GLuint relativeOffset)
{
    vk::PackedAttribDesc &packedAttrib = mVertexInputAttribs.attribs[attribIndex];

    SetBitField(packedAttrib.stride, stride);
    SetBitField(packedAttrib.divisor, divisor);

    if (format == angle::FormatID::NONE)
    {
        UNIMPLEMENTED();
    }

    SetBitField(packedAttrib.format, format);
    SetBitField(packedAttrib.compressed, compressed);
    SetBitField(packedAttrib.offset, relativeOffset);

    constexpr size_t kAttribBits = kPackedAttribDescSize * kBitsPerByte;
    const size_t kBit =
        ANGLE_GET_INDEXED_TRANSITION_BIT(mVertexInputAttribs, attribs, attribIndex, kAttribBits);

    // Cover the next dirty bit conservatively. Because each attribute is 6 bytes.
    transition->set(kBit);
    transition->set(kBit + 1);
}

void GraphicsPipelineDesc::updateTopology(GraphicsPipelineTransitionBits *transition,
                                          gl::PrimitiveMode drawMode)
{
    VkPrimitiveTopology vkTopology = gl_vk::GetPrimitiveTopology(drawMode);
    SetBitField(mInputAssemblyAndColorBlendStateInfo.primitive.topology, vkTopology);

    transition->set(ANGLE_GET_TRANSITION_BIT(mInputAssemblyAndColorBlendStateInfo, primitive));
}

void GraphicsPipelineDesc::updatePrimitiveRestartEnabled(GraphicsPipelineTransitionBits *transition,
                                                         bool primitiveRestartEnabled)
{
    mInputAssemblyAndColorBlendStateInfo.primitive.restartEnable =
        static_cast<uint16_t>(primitiveRestartEnabled);
    transition->set(ANGLE_GET_TRANSITION_BIT(mInputAssemblyAndColorBlendStateInfo, primitive));
}

void GraphicsPipelineDesc::setCullMode(VkCullModeFlagBits cullMode)
{
    SetBitField(mRasterizationAndMultisampleStateInfo.bits.cullMode, cullMode);
}

void GraphicsPipelineDesc::updateCullMode(GraphicsPipelineTransitionBits *transition,
                                          const gl::RasterizerState &rasterState)
{
    setCullMode(gl_vk::GetCullMode(rasterState));
    transition->set(ANGLE_GET_TRANSITION_BIT(mRasterizationAndMultisampleStateInfo, bits));
}

void GraphicsPipelineDesc::updateFrontFace(GraphicsPipelineTransitionBits *transition,
                                           const gl::RasterizerState &rasterState,
                                           bool invertFrontFace)
{
    mRasterizationAndMultisampleStateInfo.bits.frontFace =
        static_cast<uint16_t>(gl_vk::GetFrontFace(rasterState.frontFace, invertFrontFace));
    transition->set(ANGLE_GET_TRANSITION_BIT(mRasterizationAndMultisampleStateInfo, bits));
}

void GraphicsPipelineDesc::updateLineWidth(GraphicsPipelineTransitionBits *transition,
                                           float lineWidth)
{
    mRasterizationAndMultisampleStateInfo.lineWidth = lineWidth;
    transition->set(ANGLE_GET_TRANSITION_BIT(mRasterizationAndMultisampleStateInfo, lineWidth));
}

void GraphicsPipelineDesc::updateRasterizerDiscardEnabled(
    GraphicsPipelineTransitionBits *transition,
    bool rasterizerDiscardEnabled)
{
    mRasterizationAndMultisampleStateInfo.bits.rasterizationDiscardEnable =
        static_cast<uint32_t>(rasterizerDiscardEnabled);
    transition->set(ANGLE_GET_TRANSITION_BIT(mRasterizationAndMultisampleStateInfo, bits));
}

uint32_t GraphicsPipelineDesc::getRasterizationSamples() const
{
    return mRasterizationAndMultisampleStateInfo.bits.rasterizationSamples;
}

void GraphicsPipelineDesc::setRasterizationSamples(uint32_t rasterizationSamples)
{
    mRasterizationAndMultisampleStateInfo.bits.rasterizationSamples = rasterizationSamples;
}

void GraphicsPipelineDesc::updateRasterizationSamples(GraphicsPipelineTransitionBits *transition,
                                                      uint32_t rasterizationSamples)
{
    setRasterizationSamples(rasterizationSamples);
    transition->set(ANGLE_GET_TRANSITION_BIT(mRasterizationAndMultisampleStateInfo, bits));
}

void GraphicsPipelineDesc::updateAlphaToCoverageEnable(GraphicsPipelineTransitionBits *transition,
                                                       bool enable)
{
    mRasterizationAndMultisampleStateInfo.bits.alphaToCoverageEnable = enable;
    transition->set(ANGLE_GET_TRANSITION_BIT(mRasterizationAndMultisampleStateInfo, bits));
}

void GraphicsPipelineDesc::updateAlphaToOneEnable(GraphicsPipelineTransitionBits *transition,
                                                  bool enable)
{
    mRasterizationAndMultisampleStateInfo.bits.alphaToOneEnable = enable;
    transition->set(ANGLE_GET_TRANSITION_BIT(mRasterizationAndMultisampleStateInfo, bits));
}

void GraphicsPipelineDesc::updateSampleMask(GraphicsPipelineTransitionBits *transition,
                                            uint32_t maskNumber,
                                            uint32_t mask)
{
    ASSERT(maskNumber < gl::MAX_SAMPLE_MASK_WORDS);
    mRasterizationAndMultisampleStateInfo.sampleMask[maskNumber] = mask;

    constexpr size_t kMaskBits =
        sizeof(mRasterizationAndMultisampleStateInfo.sampleMask[0]) * kBitsPerByte;
    transition->set(ANGLE_GET_INDEXED_TRANSITION_BIT(mRasterizationAndMultisampleStateInfo,
                                                     sampleMask, maskNumber, kMaskBits));
}

void GraphicsPipelineDesc::updateSampleShading(GraphicsPipelineTransitionBits *transition,
                                               bool enable,
                                               float value)
{
    mRasterizationAndMultisampleStateInfo.bits.sampleShadingEnable = enable;
    mRasterizationAndMultisampleStateInfo.minSampleShading         = (enable ? value : 1.0f);

    transition->set(ANGLE_GET_TRANSITION_BIT(mRasterizationAndMultisampleStateInfo, bits));
    transition->set(
        ANGLE_GET_TRANSITION_BIT(mRasterizationAndMultisampleStateInfo, minSampleShading));
}

void GraphicsPipelineDesc::updateBlendColor(GraphicsPipelineTransitionBits *transition,
                                            const gl::ColorF &color)
{
    mInputAssemblyAndColorBlendStateInfo.blendConstants[0] = color.red;
    mInputAssemblyAndColorBlendStateInfo.blendConstants[1] = color.green;
    mInputAssemblyAndColorBlendStateInfo.blendConstants[2] = color.blue;
    mInputAssemblyAndColorBlendStateInfo.blendConstants[3] = color.alpha;
    constexpr size_t kSize = sizeof(mInputAssemblyAndColorBlendStateInfo.blendConstants[0]) * 8;

    for (int index = 0; index < 4; ++index)
    {
        const size_t kBit = ANGLE_GET_INDEXED_TRANSITION_BIT(mInputAssemblyAndColorBlendStateInfo,
                                                             blendConstants, index, kSize);
        transition->set(kBit);
    }
}

void GraphicsPipelineDesc::updateBlendEnabled(GraphicsPipelineTransitionBits *transition,
                                              gl::DrawBufferMask blendEnabledMask)
{
    mInputAssemblyAndColorBlendStateInfo.blendEnableMask =
        static_cast<uint8_t>(blendEnabledMask.bits());
    transition->set(
        ANGLE_GET_TRANSITION_BIT(mInputAssemblyAndColorBlendStateInfo, blendEnableMask));
}

void GraphicsPipelineDesc::updateBlendEquations(GraphicsPipelineTransitionBits *transition,
                                                const gl::BlendStateExt &blendStateExt)
{
    constexpr size_t kSize = sizeof(PackedColorBlendAttachmentState) * 8;

    for (size_t attachmentIndex = 0; attachmentIndex < blendStateExt.mMaxDrawBuffers;
         ++attachmentIndex)
    {
        PackedColorBlendAttachmentState &blendAttachmentState =
            mInputAssemblyAndColorBlendStateInfo.attachments[attachmentIndex];
        blendAttachmentState.colorBlendOp =
            PackGLBlendOp(blendStateExt.getEquationColorIndexed(attachmentIndex));
        blendAttachmentState.alphaBlendOp =
            PackGLBlendOp(blendStateExt.getEquationAlphaIndexed(attachmentIndex));
        transition->set(ANGLE_GET_INDEXED_TRANSITION_BIT(mInputAssemblyAndColorBlendStateInfo,
                                                         attachments, attachmentIndex, kSize));
    }
}

void GraphicsPipelineDesc::updateBlendFuncs(GraphicsPipelineTransitionBits *transition,
                                            const gl::BlendStateExt &blendStateExt)
{
    constexpr size_t kSize = sizeof(PackedColorBlendAttachmentState) * 8;
    for (size_t attachmentIndex = 0; attachmentIndex < blendStateExt.mMaxDrawBuffers;
         ++attachmentIndex)
    {
        PackedColorBlendAttachmentState &blendAttachmentState =
            mInputAssemblyAndColorBlendStateInfo.attachments[attachmentIndex];
        blendAttachmentState.srcColorBlendFactor =
            PackGLBlendFactor(blendStateExt.getSrcColorIndexed(attachmentIndex));
        blendAttachmentState.dstColorBlendFactor =
            PackGLBlendFactor(blendStateExt.getDstColorIndexed(attachmentIndex));
        blendAttachmentState.srcAlphaBlendFactor =
            PackGLBlendFactor(blendStateExt.getSrcAlphaIndexed(attachmentIndex));
        blendAttachmentState.dstAlphaBlendFactor =
            PackGLBlendFactor(blendStateExt.getDstAlphaIndexed(attachmentIndex));
        transition->set(ANGLE_GET_INDEXED_TRANSITION_BIT(mInputAssemblyAndColorBlendStateInfo,
                                                         attachments, attachmentIndex, kSize));
    }
}

void GraphicsPipelineDesc::setColorWriteMasks(gl::BlendStateExt::ColorMaskStorage::Type colorMasks,
                                              const gl::DrawBufferMask &alphaMask,
                                              const gl::DrawBufferMask &enabledDrawBuffers)
{
    PackedInputAssemblyAndColorBlendStateInfo &inputAndBlend = mInputAssemblyAndColorBlendStateInfo;

    for (uint32_t colorIndexGL = 0; colorIndexGL < gl::IMPLEMENTATION_MAX_DRAW_BUFFERS;
         colorIndexGL++)
    {
        uint8_t colorMask =
            gl::BlendStateExt::ColorMaskStorage::GetValueIndexed(colorIndexGL, colorMasks);

        uint8_t mask = 0;
        if (enabledDrawBuffers.test(colorIndexGL))
        {
            mask = alphaMask[colorIndexGL] ? (colorMask & ~VK_COLOR_COMPONENT_A_BIT) : colorMask;
        }
        Int4Array_Set(inputAndBlend.colorWriteMaskBits, colorIndexGL, mask);
    }
}

void GraphicsPipelineDesc::setSingleColorWriteMask(uint32_t colorIndexGL,
                                                   VkColorComponentFlags colorComponentFlags)
{
    PackedInputAssemblyAndColorBlendStateInfo &inputAndBlend = mInputAssemblyAndColorBlendStateInfo;
    uint8_t colorMask = static_cast<uint8_t>(colorComponentFlags);
    Int4Array_Set(inputAndBlend.colorWriteMaskBits, colorIndexGL, colorMask);
}

void GraphicsPipelineDesc::updateColorWriteMasks(
    GraphicsPipelineTransitionBits *transition,
    gl::BlendStateExt::ColorMaskStorage::Type colorMasks,
    const gl::DrawBufferMask &alphaMask,
    const gl::DrawBufferMask &enabledDrawBuffers)
{
    setColorWriteMasks(colorMasks, alphaMask, enabledDrawBuffers);

    for (size_t colorIndexGL = 0; colorIndexGL < gl::IMPLEMENTATION_MAX_DRAW_BUFFERS;
         colorIndexGL++)
    {
        transition->set(ANGLE_GET_INDEXED_TRANSITION_BIT(mInputAssemblyAndColorBlendStateInfo,
                                                         colorWriteMaskBits, colorIndexGL, 4));
    }
}

void GraphicsPipelineDesc::setDepthTestEnabled(bool enabled)
{
    mDepthStencilStateInfo.enable.depthTest = enabled;
}

void GraphicsPipelineDesc::setDepthWriteEnabled(bool enabled)
{
    mDepthStencilStateInfo.enable.depthWrite = enabled;
}

void GraphicsPipelineDesc::setDepthFunc(VkCompareOp op)
{
    SetBitField(mDepthStencilStateInfo.depthCompareOpAndSurfaceRotation.depthCompareOp, op);
}

void GraphicsPipelineDesc::setDepthClampEnabled(bool enabled)
{
    mRasterizationAndMultisampleStateInfo.bits.depthClampEnable = enabled;
}

void GraphicsPipelineDesc::setStencilTestEnabled(bool enabled)
{
    mDepthStencilStateInfo.enable.stencilTest = enabled;
}

void GraphicsPipelineDesc::setStencilFrontFuncs(uint8_t reference,
                                                VkCompareOp compareOp,
                                                uint8_t compareMask)
{
    mDepthStencilStateInfo.frontStencilReference = reference;
    mDepthStencilStateInfo.front.compareMask     = compareMask;
    SetBitField(mDepthStencilStateInfo.front.ops.compare, compareOp);
}

void GraphicsPipelineDesc::setStencilBackFuncs(uint8_t reference,
                                               VkCompareOp compareOp,
                                               uint8_t compareMask)
{
    mDepthStencilStateInfo.backStencilReference = reference;
    mDepthStencilStateInfo.back.compareMask     = compareMask;
    SetBitField(mDepthStencilStateInfo.back.ops.compare, compareOp);
}

void GraphicsPipelineDesc::setStencilFrontOps(VkStencilOp failOp,
                                              VkStencilOp passOp,
                                              VkStencilOp depthFailOp)
{
    SetBitField(mDepthStencilStateInfo.front.ops.fail, failOp);
    SetBitField(mDepthStencilStateInfo.front.ops.pass, passOp);
    SetBitField(mDepthStencilStateInfo.front.ops.depthFail, depthFailOp);
}

void GraphicsPipelineDesc::setStencilBackOps(VkStencilOp failOp,
                                             VkStencilOp passOp,
                                             VkStencilOp depthFailOp)
{
    SetBitField(mDepthStencilStateInfo.back.ops.fail, failOp);
    SetBitField(mDepthStencilStateInfo.back.ops.pass, passOp);
    SetBitField(mDepthStencilStateInfo.back.ops.depthFail, depthFailOp);
}

void GraphicsPipelineDesc::setStencilFrontWriteMask(uint8_t mask)
{
    mDepthStencilStateInfo.front.writeMask = mask;
}

void GraphicsPipelineDesc::setStencilBackWriteMask(uint8_t mask)
{
    mDepthStencilStateInfo.back.writeMask = mask;
}

void GraphicsPipelineDesc::updateDepthTestEnabled(GraphicsPipelineTransitionBits *transition,
                                                  const gl::DepthStencilState &depthStencilState,
                                                  const gl::Framebuffer *drawFramebuffer)
{
    // Only enable the depth test if the draw framebuffer has a depth buffer.  It's possible that
    // we're emulating a stencil-only buffer with a depth-stencil buffer
    setDepthTestEnabled(depthStencilState.depthTest && drawFramebuffer->hasDepth());
    transition->set(ANGLE_GET_TRANSITION_BIT(mDepthStencilStateInfo, enable));
}

void GraphicsPipelineDesc::updateDepthFunc(GraphicsPipelineTransitionBits *transition,
                                           const gl::DepthStencilState &depthStencilState)
{
    setDepthFunc(PackGLCompareFunc(depthStencilState.depthFunc));
    transition->set(
        ANGLE_GET_TRANSITION_BIT(mDepthStencilStateInfo, depthCompareOpAndSurfaceRotation));
}

void GraphicsPipelineDesc::updateSurfaceRotation(GraphicsPipelineTransitionBits *transition,
                                                 const SurfaceRotation surfaceRotation)
{
    SetBitField(mDepthStencilStateInfo.depthCompareOpAndSurfaceRotation.surfaceRotation,
                surfaceRotation);
    transition->set(
        ANGLE_GET_TRANSITION_BIT(mDepthStencilStateInfo, depthCompareOpAndSurfaceRotation));
}

void GraphicsPipelineDesc::updateDepthWriteEnabled(GraphicsPipelineTransitionBits *transition,
                                                   const gl::DepthStencilState &depthStencilState,
                                                   const gl::Framebuffer *drawFramebuffer)
{
    // Don't write to depth buffers that should not exist
    setDepthWriteEnabled(drawFramebuffer->hasDepth() ? depthStencilState.depthMask : false);
    transition->set(ANGLE_GET_TRANSITION_BIT(mDepthStencilStateInfo, enable));
}

void GraphicsPipelineDesc::updateStencilTestEnabled(GraphicsPipelineTransitionBits *transition,
                                                    const gl::DepthStencilState &depthStencilState,
                                                    const gl::Framebuffer *drawFramebuffer)
{
    // Only enable the stencil test if the draw framebuffer has a stencil buffer.  It's possible
    // that we're emulating a depth-only buffer with a depth-stencil buffer
    setStencilTestEnabled(depthStencilState.stencilTest && drawFramebuffer->hasStencil());
    transition->set(ANGLE_GET_TRANSITION_BIT(mDepthStencilStateInfo, enable));
}

void GraphicsPipelineDesc::updateStencilFrontFuncs(GraphicsPipelineTransitionBits *transition,
                                                   GLint ref,
                                                   const gl::DepthStencilState &depthStencilState)
{
    setStencilFrontFuncs(static_cast<uint8_t>(ref),
                         PackGLCompareFunc(depthStencilState.stencilFunc),
                         static_cast<uint8_t>(depthStencilState.stencilMask));
    transition->set(ANGLE_GET_TRANSITION_BIT(mDepthStencilStateInfo, front));
    transition->set(ANGLE_GET_TRANSITION_BIT(mDepthStencilStateInfo, frontStencilReference));
}

void GraphicsPipelineDesc::updateStencilBackFuncs(GraphicsPipelineTransitionBits *transition,
                                                  GLint ref,
                                                  const gl::DepthStencilState &depthStencilState)
{
    setStencilBackFuncs(static_cast<uint8_t>(ref),
                        PackGLCompareFunc(depthStencilState.stencilBackFunc),
                        static_cast<uint8_t>(depthStencilState.stencilBackMask));
    transition->set(ANGLE_GET_TRANSITION_BIT(mDepthStencilStateInfo, back));
    transition->set(ANGLE_GET_TRANSITION_BIT(mDepthStencilStateInfo, backStencilReference));
}

void GraphicsPipelineDesc::updateStencilFrontOps(GraphicsPipelineTransitionBits *transition,
                                                 const gl::DepthStencilState &depthStencilState)
{
    setStencilFrontOps(PackGLStencilOp(depthStencilState.stencilFail),
                       PackGLStencilOp(depthStencilState.stencilPassDepthPass),
                       PackGLStencilOp(depthStencilState.stencilPassDepthFail));
    transition->set(ANGLE_GET_TRANSITION_BIT(mDepthStencilStateInfo, front));
}

void GraphicsPipelineDesc::updateStencilBackOps(GraphicsPipelineTransitionBits *transition,
                                                const gl::DepthStencilState &depthStencilState)
{
    setStencilBackOps(PackGLStencilOp(depthStencilState.stencilBackFail),
                      PackGLStencilOp(depthStencilState.stencilBackPassDepthPass),
                      PackGLStencilOp(depthStencilState.stencilBackPassDepthFail));
    transition->set(ANGLE_GET_TRANSITION_BIT(mDepthStencilStateInfo, back));
}

void GraphicsPipelineDesc::updateStencilFrontWriteMask(
    GraphicsPipelineTransitionBits *transition,
    const gl::DepthStencilState &depthStencilState,
    const gl::Framebuffer *drawFramebuffer)
{
    // Don't write to stencil buffers that should not exist
    setStencilFrontWriteMask(static_cast<uint8_t>(
        drawFramebuffer->hasStencil() ? depthStencilState.stencilWritemask : 0));
    transition->set(ANGLE_GET_TRANSITION_BIT(mDepthStencilStateInfo, front));
}

void GraphicsPipelineDesc::updateStencilBackWriteMask(
    GraphicsPipelineTransitionBits *transition,
    const gl::DepthStencilState &depthStencilState,
    const gl::Framebuffer *drawFramebuffer)
{
    // Don't write to stencil buffers that should not exist
    setStencilBackWriteMask(static_cast<uint8_t>(
        drawFramebuffer->hasStencil() ? depthStencilState.stencilBackWritemask : 0));
    transition->set(ANGLE_GET_TRANSITION_BIT(mDepthStencilStateInfo, back));
}

void GraphicsPipelineDesc::updatePolygonOffsetFillEnabled(
    GraphicsPipelineTransitionBits *transition,
    bool enabled)
{
    mRasterizationAndMultisampleStateInfo.bits.depthBiasEnable = enabled;
    transition->set(ANGLE_GET_TRANSITION_BIT(mRasterizationAndMultisampleStateInfo, bits));
}

void GraphicsPipelineDesc::updatePolygonOffset(GraphicsPipelineTransitionBits *transition,
                                               const gl::RasterizerState &rasterState)
{
    mRasterizationAndMultisampleStateInfo.depthBiasSlopeFactor    = rasterState.polygonOffsetFactor;
    mRasterizationAndMultisampleStateInfo.depthBiasConstantFactor = rasterState.polygonOffsetUnits;
    transition->set(
        ANGLE_GET_TRANSITION_BIT(mRasterizationAndMultisampleStateInfo, depthBiasSlopeFactor));
    transition->set(
        ANGLE_GET_TRANSITION_BIT(mRasterizationAndMultisampleStateInfo, depthBiasConstantFactor));
}

void GraphicsPipelineDesc::setRenderPassDesc(const RenderPassDesc &renderPassDesc)
{
    mRenderPassDesc = renderPassDesc;
}

void GraphicsPipelineDesc::setViewport(const VkViewport &viewport)
{
    mViewport = viewport;
}

void GraphicsPipelineDesc::updateViewport(GraphicsPipelineTransitionBits *transition,
                                          const VkViewport &viewport)
{
    mViewport = viewport;
    transition->set(ANGLE_GET_TRANSITION_BIT(mViewport, x));
    transition->set(ANGLE_GET_TRANSITION_BIT(mViewport, y));
    transition->set(ANGLE_GET_TRANSITION_BIT(mViewport, width));
    transition->set(ANGLE_GET_TRANSITION_BIT(mViewport, height));
    transition->set(ANGLE_GET_TRANSITION_BIT(mViewport, minDepth));
    transition->set(ANGLE_GET_TRANSITION_BIT(mViewport, maxDepth));
}

void GraphicsPipelineDesc::updateDepthRange(GraphicsPipelineTransitionBits *transition,
                                            float nearPlane,
                                            float farPlane)
{
    // GLES2.0 Section 2.12.1: Each of n and f are clamped to lie within [0, 1], as are all
    // arguments of type clampf.
    ASSERT(nearPlane >= 0.0f && nearPlane <= 1.0f);
    ASSERT(farPlane >= 0.0f && farPlane <= 1.0f);
    mViewport.minDepth = nearPlane;
    mViewport.maxDepth = farPlane;
    transition->set(ANGLE_GET_TRANSITION_BIT(mViewport, minDepth));
    transition->set(ANGLE_GET_TRANSITION_BIT(mViewport, maxDepth));
}

void GraphicsPipelineDesc::setDynamicScissor()
{
    mScissor.offset.x      = kDynamicScissorSentinel;
    mScissor.offset.y      = 0;
    mScissor.extent.width  = 0;
    mScissor.extent.height = 0;
}

void GraphicsPipelineDesc::setScissor(const VkRect2D &scissor)
{
    mScissor = scissor;
}

void GraphicsPipelineDesc::updateScissor(GraphicsPipelineTransitionBits *transition,
                                         const VkRect2D &scissor)
{
    mScissor = scissor;
    transition->set(ANGLE_GET_TRANSITION_BIT(mScissor, offset.x));
    transition->set(ANGLE_GET_TRANSITION_BIT(mScissor, offset.y));
    transition->set(ANGLE_GET_TRANSITION_BIT(mScissor, extent.width));
    transition->set(ANGLE_GET_TRANSITION_BIT(mScissor, extent.height));
}

void GraphicsPipelineDesc::updateSubpass(GraphicsPipelineTransitionBits *transition,
                                         uint32_t subpass)
{
    if (mRasterizationAndMultisampleStateInfo.bits.subpass != subpass)
    {
        SetBitField(mRasterizationAndMultisampleStateInfo.bits.subpass, subpass);
        transition->set(ANGLE_GET_TRANSITION_BIT(mRasterizationAndMultisampleStateInfo, bits));
    }
}

void GraphicsPipelineDesc::resetSubpass(GraphicsPipelineTransitionBits *transition)
{
    updateSubpass(transition, 0);
}

void GraphicsPipelineDesc::nextSubpass(GraphicsPipelineTransitionBits *transition)
{
    updateSubpass(transition, mRasterizationAndMultisampleStateInfo.bits.subpass + 1);
}

void GraphicsPipelineDesc::setSubpass(uint32_t subpass)
{
    SetBitField(mRasterizationAndMultisampleStateInfo.bits.subpass, subpass);
}

uint32_t GraphicsPipelineDesc::getSubpass() const
{
    return mRasterizationAndMultisampleStateInfo.bits.subpass;
}

void GraphicsPipelineDesc::updateRenderPassDesc(GraphicsPipelineTransitionBits *transition,
                                                const RenderPassDesc &renderPassDesc)
{
    setRenderPassDesc(renderPassDesc);

    // The RenderPass is a special case where it spans multiple bits but has no member.
    constexpr size_t kFirstBit =
        offsetof(GraphicsPipelineDesc, mRenderPassDesc) >> kTransitionByteShift;
    constexpr size_t kBitCount = kRenderPassDescSize >> kTransitionByteShift;
    for (size_t bit = 0; bit < kBitCount; ++bit)
    {
        transition->set(kFirstBit + bit);
    }
}

// AttachmentOpsArray implementation.
AttachmentOpsArray::AttachmentOpsArray()
{
    memset(&mOps, 0, sizeof(PackedAttachmentOpsDesc) * mOps.size());
}

AttachmentOpsArray::~AttachmentOpsArray() = default;

AttachmentOpsArray::AttachmentOpsArray(const AttachmentOpsArray &other)
{
    memcpy(&mOps, &other.mOps, sizeof(PackedAttachmentOpsDesc) * mOps.size());
}

AttachmentOpsArray &AttachmentOpsArray::operator=(const AttachmentOpsArray &other)
{
    memcpy(&mOps, &other.mOps, sizeof(PackedAttachmentOpsDesc) * mOps.size());
    return *this;
}

const PackedAttachmentOpsDesc &AttachmentOpsArray::operator[](PackedAttachmentIndex index) const
{
    return mOps[index.get()];
}

PackedAttachmentOpsDesc &AttachmentOpsArray::operator[](PackedAttachmentIndex index)
{
    return mOps[index.get()];
}

void AttachmentOpsArray::initWithLoadStore(PackedAttachmentIndex index,
                                           ImageLayout initialLayout,
                                           ImageLayout finalLayout)
{
    setLayouts(index, initialLayout, finalLayout);
    setOps(index, VK_ATTACHMENT_LOAD_OP_LOAD, vk::RenderPassStoreOp::Store);
    setStencilOps(index, VK_ATTACHMENT_LOAD_OP_LOAD, RenderPassStoreOp::Store);
}

void AttachmentOpsArray::setLayouts(PackedAttachmentIndex index,
                                    ImageLayout initialLayout,
                                    ImageLayout finalLayout)
{
    PackedAttachmentOpsDesc &ops = mOps[index.get()];
    SetBitField(ops.initialLayout, initialLayout);
    SetBitField(ops.finalLayout, finalLayout);
}

void AttachmentOpsArray::setOps(PackedAttachmentIndex index,
                                VkAttachmentLoadOp loadOp,
                                RenderPassStoreOp storeOp)
{
    PackedAttachmentOpsDesc &ops = mOps[index.get()];
    SetBitField(ops.loadOp, loadOp);
    SetBitField(ops.storeOp, storeOp);
    ops.isInvalidated = false;
}

void AttachmentOpsArray::setStencilOps(PackedAttachmentIndex index,
                                       VkAttachmentLoadOp loadOp,
                                       RenderPassStoreOp storeOp)
{
    PackedAttachmentOpsDesc &ops = mOps[index.get()];
    SetBitField(ops.stencilLoadOp, loadOp);
    SetBitField(ops.stencilStoreOp, storeOp);
    ops.isStencilInvalidated = false;
}

void AttachmentOpsArray::setClearOp(PackedAttachmentIndex index)
{
    PackedAttachmentOpsDesc &ops = mOps[index.get()];
    SetBitField(ops.loadOp, VK_ATTACHMENT_LOAD_OP_CLEAR);
}

void AttachmentOpsArray::setClearStencilOp(PackedAttachmentIndex index)
{
    PackedAttachmentOpsDesc &ops = mOps[index.get()];
    SetBitField(ops.stencilLoadOp, VK_ATTACHMENT_LOAD_OP_CLEAR);
}

size_t AttachmentOpsArray::hash() const
{
    return angle::ComputeGenericHash(mOps);
}

bool operator==(const AttachmentOpsArray &lhs, const AttachmentOpsArray &rhs)
{
    return (memcmp(&lhs, &rhs, sizeof(AttachmentOpsArray)) == 0);
}

// DescriptorSetLayoutDesc implementation.
DescriptorSetLayoutDesc::DescriptorSetLayoutDesc() : mPackedDescriptorSetLayout{} {}

DescriptorSetLayoutDesc::~DescriptorSetLayoutDesc() = default;

DescriptorSetLayoutDesc::DescriptorSetLayoutDesc(const DescriptorSetLayoutDesc &other) = default;

DescriptorSetLayoutDesc &DescriptorSetLayoutDesc::operator=(const DescriptorSetLayoutDesc &other) =
    default;

size_t DescriptorSetLayoutDesc::hash() const
{
    return angle::ComputeGenericHash(mPackedDescriptorSetLayout);
}

bool DescriptorSetLayoutDesc::operator==(const DescriptorSetLayoutDesc &other) const
{
    return (memcmp(&mPackedDescriptorSetLayout, &other.mPackedDescriptorSetLayout,
                   sizeof(mPackedDescriptorSetLayout)) == 0);
}

void DescriptorSetLayoutDesc::update(uint32_t bindingIndex,
                                     VkDescriptorType type,
                                     uint32_t count,
                                     VkShaderStageFlags stages,
                                     const vk::Sampler *immutableSampler)
{
    ASSERT(static_cast<size_t>(type) < std::numeric_limits<uint16_t>::max());
    ASSERT(count < std::numeric_limits<uint16_t>::max());

    PackedDescriptorSetBinding &packedBinding = mPackedDescriptorSetLayout[bindingIndex];

    SetBitField(packedBinding.type, type);
    SetBitField(packedBinding.count, count);
    SetBitField(packedBinding.stages, stages);
    packedBinding.immutableSampler = VK_NULL_HANDLE;
    packedBinding.pad              = 0;

    if (immutableSampler)
    {
        ASSERT(count == 1);
        packedBinding.immutableSampler = immutableSampler->getHandle();
    }
}

void DescriptorSetLayoutDesc::unpackBindings(DescriptorSetLayoutBindingVector *bindings,
                                             std::vector<VkSampler> *immutableSamplers) const
{
    for (uint32_t bindingIndex = 0; bindingIndex < kMaxDescriptorSetLayoutBindings; ++bindingIndex)
    {
        const PackedDescriptorSetBinding &packedBinding = mPackedDescriptorSetLayout[bindingIndex];
        if (packedBinding.count == 0)
            continue;

        VkDescriptorSetLayoutBinding binding = {};
        binding.binding                      = bindingIndex;
        binding.descriptorCount              = packedBinding.count;
        binding.descriptorType               = static_cast<VkDescriptorType>(packedBinding.type);
        binding.stageFlags = static_cast<VkShaderStageFlags>(packedBinding.stages);
        if (packedBinding.immutableSampler != VK_NULL_HANDLE)
        {
            ASSERT(packedBinding.count == 1);
            immutableSamplers->push_back(packedBinding.immutableSampler);
            binding.pImmutableSamplers = reinterpret_cast<const VkSampler *>(angle::DirtyPointer);
        }

        bindings->push_back(binding);
    }
    if (!immutableSamplers->empty())
    {
        // Patch up pImmutableSampler addresses now that the vector is stable
        int immutableIndex = 0;
        for (VkDescriptorSetLayoutBinding &binding : *bindings)
        {
            if (binding.pImmutableSamplers)
            {
                binding.pImmutableSamplers = &(*immutableSamplers)[immutableIndex];
                immutableIndex++;
            }
        }
    }
}

// PipelineLayoutDesc implementation.
PipelineLayoutDesc::PipelineLayoutDesc() : mDescriptorSetLayouts{}, mPushConstantRanges{} {}

PipelineLayoutDesc::~PipelineLayoutDesc() = default;

PipelineLayoutDesc::PipelineLayoutDesc(const PipelineLayoutDesc &other) = default;

PipelineLayoutDesc &PipelineLayoutDesc::operator=(const PipelineLayoutDesc &rhs)
{
    mDescriptorSetLayouts = rhs.mDescriptorSetLayouts;
    mPushConstantRanges   = rhs.mPushConstantRanges;
    return *this;
}

size_t PipelineLayoutDesc::hash() const
{
    return angle::ComputeGenericHash(*this);
}

bool PipelineLayoutDesc::operator==(const PipelineLayoutDesc &other) const
{
    return memcmp(this, &other, sizeof(PipelineLayoutDesc)) == 0;
}

void PipelineLayoutDesc::updateDescriptorSetLayout(DescriptorSetIndex setIndex,
                                                   const DescriptorSetLayoutDesc &desc)
{
    ASSERT(ToUnderlying(setIndex) < mDescriptorSetLayouts.size());
    mDescriptorSetLayouts[ToUnderlying(setIndex)] = desc;
}

void PipelineLayoutDesc::updatePushConstantRange(gl::ShaderType shaderType,
                                                 uint32_t offset,
                                                 uint32_t size)
{
    ASSERT(shaderType == gl::ShaderType::Vertex || shaderType == gl::ShaderType::Fragment ||
           shaderType == gl::ShaderType::Geometry || shaderType == gl::ShaderType::Compute);
    PackedPushConstantRange &packed = mPushConstantRanges[shaderType];
    packed.offset                   = offset;
    packed.size                     = size;
}

const PushConstantRangeArray<PackedPushConstantRange> &PipelineLayoutDesc::getPushConstantRanges()
    const
{
    return mPushConstantRanges;
}

// PipelineHelper implementation.
PipelineHelper::PipelineHelper() = default;

PipelineHelper::~PipelineHelper() = default;

void PipelineHelper::destroy(VkDevice device)
{
    mPipeline.destroy(device);
}

void PipelineHelper::addTransition(GraphicsPipelineTransitionBits bits,
                                   const GraphicsPipelineDesc *desc,
                                   PipelineHelper *pipeline)
{
    mTransitions.emplace_back(bits, desc, pipeline);
}

TextureDescriptorDesc::TextureDescriptorDesc() : mMaxIndex(0)
{
    mSerials.fill({kInvalidImageViewSubresourceSerial, kInvalidSamplerSerial});
}

TextureDescriptorDesc::~TextureDescriptorDesc()                                  = default;
TextureDescriptorDesc::TextureDescriptorDesc(const TextureDescriptorDesc &other) = default;
TextureDescriptorDesc &TextureDescriptorDesc::operator=(const TextureDescriptorDesc &other) =
    default;

void TextureDescriptorDesc::update(size_t index,
                                   ImageViewSubresourceSerial imageViewSerial,
                                   SamplerSerial samplerSerial)
{
    if (index >= mMaxIndex)
    {
        mMaxIndex = static_cast<uint32_t>(index + 1);
    }

    mSerials[index].imageView = imageViewSerial;
    mSerials[index].sampler   = samplerSerial;
}

size_t TextureDescriptorDesc::hash() const
{
    return angle::ComputeGenericHash(&mSerials, sizeof(TexUnitSerials) * mMaxIndex);
}

void TextureDescriptorDesc::reset()
{
    memset(mSerials.data(), 0, sizeof(mSerials[0]) * mMaxIndex);
    mMaxIndex = 0;
}

bool TextureDescriptorDesc::operator==(const TextureDescriptorDesc &other) const
{
    if (mMaxIndex != other.mMaxIndex)
        return false;

    if (mMaxIndex == 0)
        return true;

    return memcmp(mSerials.data(), other.mSerials.data(), sizeof(TexUnitSerials) * mMaxIndex) == 0;
}

// UniformsAndXfbDesc implementation.
UniformsAndXfbDesc::UniformsAndXfbDesc()
{
    reset();
}

UniformsAndXfbDesc::~UniformsAndXfbDesc()                               = default;
UniformsAndXfbDesc::UniformsAndXfbDesc(const UniformsAndXfbDesc &other) = default;
UniformsAndXfbDesc &UniformsAndXfbDesc::operator=(const UniformsAndXfbDesc &other) = default;

size_t UniformsAndXfbDesc::hash() const
{
    return angle::ComputeGenericHash(&mBufferSerials, sizeof(BufferSerial) * mBufferCount);
}

void UniformsAndXfbDesc::reset()
{
    mBufferCount = 0;
    memset(&mBufferSerials, 0, sizeof(BufferSerial) * kMaxBufferCount);
}

bool UniformsAndXfbDesc::operator==(const UniformsAndXfbDesc &other) const
{
    if (mBufferCount != other.mBufferCount)
    {
        return false;
    }

    return memcmp(&mBufferSerials, &other.mBufferSerials, sizeof(BufferSerial) * mBufferCount) == 0;
}

// FramebufferDesc implementation.

FramebufferDesc::FramebufferDesc()
{
    reset();
}

FramebufferDesc::~FramebufferDesc()                            = default;
FramebufferDesc::FramebufferDesc(const FramebufferDesc &other) = default;
FramebufferDesc &FramebufferDesc::operator=(const FramebufferDesc &other) = default;

void FramebufferDesc::update(uint32_t index, ImageViewSubresourceSerial serial)
{
    static_assert(kMaxFramebufferAttachments + 1 < std::numeric_limits<uint8_t>::max(),
                  "mMaxIndex size is too small");
    ASSERT(index < kMaxFramebufferAttachments);
    mSerials[index] = serial;
    if (serial.imageViewSerial.valid())
    {
        mMaxIndex = std::max(mMaxIndex, static_cast<uint16_t>(index + 1));
    }
}

void FramebufferDesc::updateColor(uint32_t index, ImageViewSubresourceSerial serial)
{
    update(kFramebufferDescColorIndexOffset + index, serial);
}

void FramebufferDesc::updateColorResolve(uint32_t index, ImageViewSubresourceSerial serial)
{
    update(kFramebufferDescColorResolveIndexOffset + index, serial);
}

void FramebufferDesc::updateUnresolveMask(FramebufferNonResolveAttachmentMask unresolveMask)
{
    mUnresolveAttachmentMask = unresolveMask;
}

void FramebufferDesc::updateDepthStencil(ImageViewSubresourceSerial serial)
{
    update(kFramebufferDescDepthStencilIndex, serial);
}

void FramebufferDesc::updateDepthStencilResolve(ImageViewSubresourceSerial serial)
{
    update(kFramebufferDescDepthStencilResolveIndexOffset, serial);
}

size_t FramebufferDesc::hash() const
{
    return angle::ComputeGenericHash(&mSerials, sizeof(mSerials[0]) * mMaxIndex) ^
           mUnresolveAttachmentMask.bits();
}

void FramebufferDesc::reset()
{
    mMaxIndex = 0;
    mUnresolveAttachmentMask.reset();
    memset(&mSerials, 0, sizeof(mSerials));
}

bool FramebufferDesc::operator==(const FramebufferDesc &other) const
{
    if (mMaxIndex != other.mMaxIndex || mUnresolveAttachmentMask != other.mUnresolveAttachmentMask)
    {
        return false;
    }

    size_t validRegionSize = sizeof(mSerials[0]) * mMaxIndex;
    return memcmp(&mSerials, &other.mSerials, validRegionSize) == 0;
}

uint32_t FramebufferDesc::attachmentCount() const
{
    uint32_t count = 0;
    for (const ImageViewSubresourceSerial &serial : mSerials)
    {
        if (serial.imageViewSerial.valid())
        {
            count++;
        }
    }
    return count;
}

FramebufferNonResolveAttachmentMask FramebufferDesc::getUnresolveAttachmentMask() const
{
    return mUnresolveAttachmentMask;
}

// SamplerDesc implementation.
SamplerDesc::SamplerDesc()
{
    reset();
}

SamplerDesc::~SamplerDesc() = default;

SamplerDesc::SamplerDesc(const SamplerDesc &other) = default;

SamplerDesc &SamplerDesc::operator=(const SamplerDesc &rhs) = default;

SamplerDesc::SamplerDesc(const angle::FeaturesVk &featuresVk,
                         const gl::SamplerState &samplerState,
                         bool stencilMode,
                         uint64_t externalFormat)
{
    update(featuresVk, samplerState, stencilMode, externalFormat);
}

void SamplerDesc::reset()
{
    mMipLodBias     = 0.0f;
    mMaxAnisotropy  = 0.0f;
    mMinLod         = 0.0f;
    mMaxLod         = 0.0f;
    mExternalFormat = 0;
    mMagFilter      = 0;
    mMinFilter      = 0;
    mMipmapMode     = 0;
    mAddressModeU   = 0;
    mAddressModeV   = 0;
    mAddressModeW   = 0;
    mCompareEnabled = 0;
    mCompareOp      = 0;
    mReserved[0]    = 0;
    mReserved[1]    = 0;
    mReserved[2]    = 0;
}

void SamplerDesc::update(const angle::FeaturesVk &featuresVk,
                         const gl::SamplerState &samplerState,
                         bool stencilMode,
                         uint64_t externalFormat)
{
    mMipLodBias = 0.0f;

    mMaxAnisotropy = samplerState.getMaxAnisotropy();
    mMinLod        = samplerState.getMinLod();
    mMaxLod        = samplerState.getMaxLod();

    // GL has no notion of external format, this must be provided from metadata from the image
    mExternalFormat = externalFormat;

    bool compareEnable    = samplerState.getCompareMode() == GL_COMPARE_REF_TO_TEXTURE;
    VkCompareOp compareOp = gl_vk::GetCompareOp(samplerState.getCompareFunc());
    // When sampling from stencil, deqp tests expect texture compare to have no effect
    // dEQP - GLES31.functional.stencil_texturing.misc.compare_mode_effect
    // states: NOTE: Texture compare mode has no effect when reading stencil values.
    if (stencilMode)
    {
        compareEnable = VK_FALSE;
        compareOp     = VK_COMPARE_OP_ALWAYS;
    }

    GLenum magFilter = samplerState.getMagFilter();
    GLenum minFilter = samplerState.getMinFilter();

    SetBitField(mMagFilter, gl_vk::GetFilter(magFilter));
    SetBitField(mMinFilter, gl_vk::GetFilter(minFilter));
    SetBitField(mMipmapMode, gl_vk::GetSamplerMipmapMode(samplerState.getMinFilter()));
    SetBitField(mAddressModeU, gl_vk::GetSamplerAddressMode(samplerState.getWrapS()));
    SetBitField(mAddressModeV, gl_vk::GetSamplerAddressMode(samplerState.getWrapT()));
    SetBitField(mAddressModeW, gl_vk::GetSamplerAddressMode(samplerState.getWrapR()));
    SetBitField(mCompareEnabled, compareEnable);
    SetBitField(mCompareOp, compareOp);

    if (!gl::IsMipmapFiltered(minFilter))
    {
        // Per the Vulkan spec, GL_NEAREST and GL_LINEAR do not map directly to Vulkan, so
        // they must be emulated (See "Mapping of OpenGL to Vulkan filter modes")
        SetBitField(mMipmapMode, VK_SAMPLER_MIPMAP_MODE_NEAREST);
        mMinLod = 0.0f;
        mMaxLod = 0.25f;
    }

    mReserved[0] = 0;
    mReserved[1] = 0;
    mReserved[2] = 0;
}

angle::Result SamplerDesc::init(ContextVk *contextVk, vk::Sampler *sampler) const
{
    const gl::Extensions &extensions = contextVk->getExtensions();

    bool anisotropyEnable = extensions.textureFilterAnisotropic && mMaxAnisotropy > 1.0f;

    VkSamplerCreateInfo createInfo     = {};
    createInfo.sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    createInfo.flags                   = 0;
    createInfo.magFilter               = static_cast<VkFilter>(mMagFilter);
    createInfo.minFilter               = static_cast<VkFilter>(mMinFilter);
    createInfo.mipmapMode              = static_cast<VkSamplerMipmapMode>(mMipmapMode);
    createInfo.addressModeU            = static_cast<VkSamplerAddressMode>(mAddressModeU);
    createInfo.addressModeV            = static_cast<VkSamplerAddressMode>(mAddressModeV);
    createInfo.addressModeW            = static_cast<VkSamplerAddressMode>(mAddressModeW);
    createInfo.mipLodBias              = mMipLodBias;
    createInfo.anisotropyEnable        = anisotropyEnable;
    createInfo.maxAnisotropy           = mMaxAnisotropy;
    createInfo.compareEnable           = mCompareEnabled ? VK_TRUE : VK_FALSE;
    createInfo.compareOp               = static_cast<VkCompareOp>(mCompareOp);
    createInfo.minLod                  = mMinLod;
    createInfo.maxLod                  = mMaxLod;
    createInfo.borderColor             = VK_BORDER_COLOR_INT_TRANSPARENT_BLACK;
    createInfo.unnormalizedCoordinates = VK_FALSE;

    // Note: because we don't detect changes to this hint (no dirty bit), if a sampler is created
    // with the hint enabled, and then the hint gets disabled, the next render will do so with the
    // hint enabled.
    VkSamplerFilteringPrecisionGOOGLE filteringInfo = {};
    GLenum hint = contextVk->getState().getTextureFilteringHint();
    if (hint == GL_NICEST)
    {
        ASSERT(extensions.textureFilteringCHROMIUM);
        filteringInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_FILTERING_PRECISION_GOOGLE;
        filteringInfo.samplerFilteringPrecisionMode =
            VK_SAMPLER_FILTERING_PRECISION_MODE_HIGH_GOOGLE;
        vk::AddToPNextChain(&createInfo, &filteringInfo);
    }

    VkSamplerYcbcrConversionInfo yuvConversionInfo = {};
    if (mExternalFormat)
    {
        ASSERT((contextVk->getRenderer()->getFeatures().supportsYUVSamplerConversion.enabled));
        yuvConversionInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_INFO;
        yuvConversionInfo.pNext = nullptr;
        yuvConversionInfo.conversion =
            contextVk->getRenderer()->getYuvConversionCache().getYuvConversionFromExternalFormat(
                mExternalFormat);
        vk::AddToPNextChain(&createInfo, &yuvConversionInfo);

        // Vulkan spec requires these settings:
        createInfo.addressModeU            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        createInfo.addressModeV            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        createInfo.addressModeW            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        createInfo.anisotropyEnable        = VK_FALSE;
        createInfo.unnormalizedCoordinates = VK_FALSE;
    }

    ANGLE_VK_TRY(contextVk, sampler->init(contextVk->getDevice(), createInfo));

    return angle::Result::Continue;
}

size_t SamplerDesc::hash() const
{
    return angle::ComputeGenericHash(*this);
}

bool SamplerDesc::operator==(const SamplerDesc &other) const
{
    return (memcmp(this, &other, sizeof(SamplerDesc)) == 0);
}

// SamplerHelper implementation.
SamplerHelper::SamplerHelper(ContextVk *contextVk)
    : mSamplerSerial(contextVk->getRenderer()->getResourceSerialFactory().generateSamplerSerial())
{}

SamplerHelper::~SamplerHelper() {}

SamplerHelper::SamplerHelper(SamplerHelper &&samplerHelper)
{
    *this = std::move(samplerHelper);
}

SamplerHelper &SamplerHelper::operator=(SamplerHelper &&rhs)
{
    std::swap(mSampler, rhs.mSampler);
    std::swap(mSamplerSerial, rhs.mSamplerSerial);
    return *this;
}

// RenderPassHelper implementation.
RenderPassHelper::RenderPassHelper() : mPerfCounters{} {}

RenderPassHelper::~RenderPassHelper() = default;

RenderPassHelper::RenderPassHelper(RenderPassHelper &&other)
{
    *this = std::move(other);
}

RenderPassHelper &RenderPassHelper::operator=(RenderPassHelper &&other)
{
    mRenderPass   = std::move(other.mRenderPass);
    mPerfCounters = std::move(other.mPerfCounters);
    return *this;
}

void RenderPassHelper::destroy(VkDevice device)
{
    mRenderPass.destroy(device);
}

const RenderPass &RenderPassHelper::getRenderPass() const
{
    return mRenderPass;
}

RenderPass &RenderPassHelper::getRenderPass()
{
    return mRenderPass;
}

const RenderPassPerfCounters &RenderPassHelper::getPerfCounters() const
{
    return mPerfCounters;
}

RenderPassPerfCounters &RenderPassHelper::getPerfCounters()
{
    return mPerfCounters;
}
}  // namespace vk

// RenderPassCache implementation.
RenderPassCache::RenderPassCache() = default;

RenderPassCache::~RenderPassCache()
{
    ASSERT(mPayload.empty());
}

void RenderPassCache::destroy(VkDevice device)
{
    for (auto &outerIt : mPayload)
    {
        for (auto &innerIt : outerIt.second)
        {
            innerIt.second.destroy(device);
        }
    }
    mPayload.clear();
}

angle::Result RenderPassCache::addRenderPass(ContextVk *contextVk,
                                             const vk::RenderPassDesc &desc,
                                             vk::RenderPass **renderPassOut)
{
    // Insert some placeholder attachment ops.  Note that render passes with different ops are still
    // compatible. The load/store values are not important as they are aren't used for real RPs.
    //
    // It would be nice to pre-populate the cache in the Renderer so we rarely miss here.
    vk::AttachmentOpsArray ops;

    vk::PackedAttachmentIndex colorIndexVk(0);
    for (uint32_t colorIndexGL = 0; colorIndexGL < desc.colorAttachmentRange(); ++colorIndexGL)
    {
        if (!desc.isColorAttachmentEnabled(colorIndexGL))
        {
            continue;
        }

        ops.initWithLoadStore(colorIndexVk, vk::ImageLayout::ColorAttachment,
                              vk::ImageLayout::ColorAttachment);
        ++colorIndexVk;
    }

    if (desc.hasDepthStencilAttachment())
    {
        // This API is only called by getCompatibleRenderPass(). What we need here is to create a
        // compatible renderpass with the desc. Vulkan spec says image layout are not counted toward
        // render pass compatibility: "Two render passes are compatible if their corresponding
        // color, input, resolve, and depth/stencil attachment references are compatible and if they
        // are otherwise identical except for: Initial and final image layout in attachment
        // descriptions; Image layout in attachment references". We pick the most used layout here
        // since it doesn't matter.
        vk::ImageLayout imageLayout = vk::ImageLayout::DepthStencilAttachment;
        ops.initWithLoadStore(colorIndexVk, imageLayout, imageLayout);
    }

    return getRenderPassWithOpsImpl(contextVk, desc, ops, false, renderPassOut);
}

angle::Result RenderPassCache::getRenderPassWithOps(ContextVk *contextVk,
                                                    const vk::RenderPassDesc &desc,
                                                    const vk::AttachmentOpsArray &attachmentOps,
                                                    vk::RenderPass **renderPassOut)
{
    return getRenderPassWithOpsImpl(contextVk, desc, attachmentOps, true, renderPassOut);
}

angle::Result RenderPassCache::getRenderPassWithOpsImpl(ContextVk *contextVk,
                                                        const vk::RenderPassDesc &desc,
                                                        const vk::AttachmentOpsArray &attachmentOps,
                                                        bool updatePerfCounters,
                                                        vk::RenderPass **renderPassOut)
{
    auto outerIt = mPayload.find(desc);
    if (outerIt != mPayload.end())
    {
        InnerCache &innerCache = outerIt->second;

        auto innerIt = innerCache.find(attachmentOps);
        if (innerIt != innerCache.end())
        {
            // TODO(jmadill): Could possibly use an MRU cache here.
            vk::GetRenderPassAndUpdateCounters(contextVk, updatePerfCounters, &innerIt->second,
                                               renderPassOut);
            return angle::Result::Continue;
        }
    }
    else
    {
        auto emplaceResult = mPayload.emplace(desc, InnerCache());
        outerIt            = emplaceResult.first;
    }

    vk::RenderPassHelper newRenderPass;
    ANGLE_TRY(vk::InitializeRenderPassFromDesc(contextVk, desc, attachmentOps, &newRenderPass));

    InnerCache &innerCache = outerIt->second;
    auto insertPos         = innerCache.emplace(attachmentOps, std::move(newRenderPass));
    vk::GetRenderPassAndUpdateCounters(contextVk, updatePerfCounters, &insertPos.first->second,
                                       renderPassOut);

    // TODO(jmadill): Trim cache, and pre-populate with the most common RPs on startup.
    return angle::Result::Continue;
}

// GraphicsPipelineCache implementation.
GraphicsPipelineCache::GraphicsPipelineCache() = default;

GraphicsPipelineCache::~GraphicsPipelineCache()
{
    ASSERT(mPayload.empty());
}

void GraphicsPipelineCache::destroy(VkDevice device)
{
    for (auto &item : mPayload)
    {
        vk::PipelineHelper &pipeline = item.second;
        pipeline.destroy(device);
    }

    mPayload.clear();
}

void GraphicsPipelineCache::release(ContextVk *context)
{
    for (auto &item : mPayload)
    {
        vk::PipelineHelper &pipeline = item.second;
        context->addGarbage(&pipeline.getPipeline());
    }

    mPayload.clear();
}

angle::Result GraphicsPipelineCache::insertPipeline(
    ContextVk *contextVk,
    const vk::PipelineCache &pipelineCacheVk,
    const vk::RenderPass &compatibleRenderPass,
    const vk::PipelineLayout &pipelineLayout,
    const gl::AttributesMask &activeAttribLocationsMask,
    const gl::ComponentTypeMask &programAttribsTypeMask,
    const vk::ShaderModule *vertexModule,
    const vk::ShaderModule *fragmentModule,
    const vk::ShaderModule *geometryModule,
    const vk::SpecializationConstants specConsts,
    const vk::GraphicsPipelineDesc &desc,
    const vk::GraphicsPipelineDesc **descPtrOut,
    vk::PipelineHelper **pipelineOut)
{
    vk::Pipeline newPipeline;

    // This "if" is left here for the benefit of VulkanPipelineCachePerfTest.
    if (contextVk != nullptr)
    {
        contextVk->getRenderer()->onNewGraphicsPipeline();
        ANGLE_TRY(desc.initializePipeline(contextVk, pipelineCacheVk, compatibleRenderPass,
                                          pipelineLayout, activeAttribLocationsMask,
                                          programAttribsTypeMask, vertexModule, fragmentModule,
                                          geometryModule, specConsts, &newPipeline));
    }

    // The Serial will be updated outside of this query.
    auto insertedItem = mPayload.emplace(desc, std::move(newPipeline));
    *descPtrOut       = &insertedItem.first->first;
    *pipelineOut      = &insertedItem.first->second;

    return angle::Result::Continue;
}

void GraphicsPipelineCache::populate(const vk::GraphicsPipelineDesc &desc, vk::Pipeline &&pipeline)
{
    auto item = mPayload.find(desc);
    if (item != mPayload.end())
    {
        return;
    }

    mPayload.emplace(desc, std::move(pipeline));
}

// DescriptorSetLayoutCache implementation.
DescriptorSetLayoutCache::DescriptorSetLayoutCache() = default;

DescriptorSetLayoutCache::~DescriptorSetLayoutCache()
{
    ASSERT(mPayload.empty());
}

void DescriptorSetLayoutCache::destroy(VkDevice device)
{
    for (auto &item : mPayload)
    {
        vk::RefCountedDescriptorSetLayout &layout = item.second;
        ASSERT(!layout.isReferenced());
        layout.get().destroy(device);
    }

    mPayload.clear();
}

angle::Result DescriptorSetLayoutCache::getDescriptorSetLayout(
    vk::Context *context,
    const vk::DescriptorSetLayoutDesc &desc,
    vk::BindingPointer<vk::DescriptorSetLayout> *descriptorSetLayoutOut)
{
    auto iter = mPayload.find(desc);
    if (iter != mPayload.end())
    {
        vk::RefCountedDescriptorSetLayout &layout = iter->second;
        descriptorSetLayoutOut->set(&layout);
        return angle::Result::Continue;
    }

    // We must unpack the descriptor set layout description.
    vk::DescriptorSetLayoutBindingVector bindingVector;
    std::vector<VkSampler> immutableSamplers;
    desc.unpackBindings(&bindingVector, &immutableSamplers);

    VkDescriptorSetLayoutCreateInfo createInfo = {};
    createInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    createInfo.flags        = 0;
    createInfo.bindingCount = static_cast<uint32_t>(bindingVector.size());
    createInfo.pBindings    = bindingVector.data();

    vk::DescriptorSetLayout newLayout;
    ANGLE_VK_TRY(context, newLayout.init(context->getDevice(), createInfo));

    auto insertedItem =
        mPayload.emplace(desc, vk::RefCountedDescriptorSetLayout(std::move(newLayout)));
    vk::RefCountedDescriptorSetLayout &insertedLayout = insertedItem.first->second;
    descriptorSetLayoutOut->set(&insertedLayout);

    return angle::Result::Continue;
}

// PipelineLayoutCache implementation.
PipelineLayoutCache::PipelineLayoutCache() = default;

PipelineLayoutCache::~PipelineLayoutCache()
{
    ASSERT(mPayload.empty());
}

void PipelineLayoutCache::destroy(VkDevice device)
{
    for (auto &item : mPayload)
    {
        vk::RefCountedPipelineLayout &layout = item.second;
        layout.get().destroy(device);
    }

    mPayload.clear();
}

angle::Result PipelineLayoutCache::getPipelineLayout(
    vk::Context *context,
    const vk::PipelineLayoutDesc &desc,
    const vk::DescriptorSetLayoutPointerArray &descriptorSetLayouts,
    vk::BindingPointer<vk::PipelineLayout> *pipelineLayoutOut)
{
    auto iter = mPayload.find(desc);
    if (iter != mPayload.end())
    {
        vk::RefCountedPipelineLayout &layout = iter->second;
        pipelineLayoutOut->set(&layout);
        return angle::Result::Continue;
    }

    // Note this does not handle gaps in descriptor set layouts gracefully.
    angle::FixedVector<VkDescriptorSetLayout, vk::kMaxDescriptorSetLayouts> setLayoutHandles;
    for (const vk::BindingPointer<vk::DescriptorSetLayout> &layoutPtr : descriptorSetLayouts)
    {
        if (layoutPtr.valid())
        {
            VkDescriptorSetLayout setLayout = layoutPtr.get().getHandle();
            if (setLayout != VK_NULL_HANDLE)
            {
                setLayoutHandles.push_back(setLayout);
            }
        }
    }

    const vk::PushConstantRangeArray<vk::PackedPushConstantRange> &descPushConstantRanges =
        desc.getPushConstantRanges();

    gl::ShaderVector<VkPushConstantRange> pushConstantRanges;

    for (const gl::ShaderType shaderType : gl::AllShaderTypes())
    {
        const vk::PackedPushConstantRange &pushConstantDesc = descPushConstantRanges[shaderType];
        if (pushConstantDesc.size > 0)
        {
            VkPushConstantRange range;
            range.stageFlags = gl_vk::kShaderStageMap[shaderType];
            range.offset     = pushConstantDesc.offset;
            range.size       = pushConstantDesc.size;

            pushConstantRanges.push_back(range);
        }
    }

    // No pipeline layout found. We must create a new one.
    VkPipelineLayoutCreateInfo createInfo = {};
    createInfo.sType                      = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    createInfo.flags                      = 0;
    createInfo.setLayoutCount             = static_cast<uint32_t>(setLayoutHandles.size());
    createInfo.pSetLayouts                = setLayoutHandles.data();
    createInfo.pushConstantRangeCount     = static_cast<uint32_t>(pushConstantRanges.size());
    createInfo.pPushConstantRanges        = pushConstantRanges.data();

    vk::PipelineLayout newLayout;
    ANGLE_VK_TRY(context, newLayout.init(context->getDevice(), createInfo));

    auto insertedItem = mPayload.emplace(desc, vk::RefCountedPipelineLayout(std::move(newLayout)));
    vk::RefCountedPipelineLayout &insertedLayout = insertedItem.first->second;
    pipelineLayoutOut->set(&insertedLayout);

    return angle::Result::Continue;
}

// YuvConversionCache implementation
SamplerYcbcrConversionCache::SamplerYcbcrConversionCache() = default;

SamplerYcbcrConversionCache::~SamplerYcbcrConversionCache()
{
    ASSERT(mPayload.empty());
}

void SamplerYcbcrConversionCache::destroy(RendererVk *renderer)
{
    VkDevice device = renderer->getDevice();

    for (auto &iter : mPayload)
    {
        vk::RefCountedSamplerYcbcrConversion &yuvSampler = iter.second;
        ASSERT(!yuvSampler.isReferenced());
        yuvSampler.get().destroy(device);

        renderer->getActiveHandleCounts().onDeallocate(vk::HandleType::SamplerYcbcrConversion);
    }

    mPayload.clear();
}

angle::Result SamplerYcbcrConversionCache::getYuvConversion(
    vk::Context *context,
    uint64_t externalFormat,
    const VkSamplerYcbcrConversionCreateInfo &yuvConversionCreateInfo,
    vk::BindingPointer<vk::SamplerYcbcrConversion> *yuvConversionOut)
{
    const auto iter = mPayload.find(externalFormat);
    if (iter != mPayload.end())
    {
        vk::RefCountedSamplerYcbcrConversion &yuvConversion = iter->second;
        yuvConversionOut->set(&yuvConversion);
        return angle::Result::Continue;
    }

    vk::SamplerYcbcrConversion wrappedYuvConversion;
    ANGLE_VK_TRY(context, wrappedYuvConversion.init(context->getDevice(), yuvConversionCreateInfo));

    auto insertedItem = mPayload.emplace(
        externalFormat, vk::RefCountedSamplerYcbcrConversion(std::move(wrappedYuvConversion)));
    vk::RefCountedSamplerYcbcrConversion &insertedYuvConversion = insertedItem.first->second;
    yuvConversionOut->set(&insertedYuvConversion);

    context->getRenderer()->getActiveHandleCounts().onAllocate(
        vk::HandleType::SamplerYcbcrConversion);

    return angle::Result::Continue;
}

VkSamplerYcbcrConversion SamplerYcbcrConversionCache::getYuvConversionFromExternalFormat(
    uint64_t externalFormat) const
{
    const auto iter = mPayload.find(externalFormat);
    if (iter != mPayload.end())
    {
        const vk::RefCountedSamplerYcbcrConversion &yuvConversion = iter->second;
        return yuvConversion.get().getHandle();
    }

    // Should never get here if we have a valid externalFormat.
    UNREACHABLE();
    return VK_NULL_HANDLE;
}

// SamplerCache implementation.
SamplerCache::SamplerCache() = default;

SamplerCache::~SamplerCache()
{
    ASSERT(mPayload.empty());
}

void SamplerCache::destroy(RendererVk *renderer)
{
    VkDevice device = renderer->getDevice();

    for (auto &iter : mPayload)
    {
        vk::RefCountedSampler &sampler = iter.second;
        ASSERT(!sampler.isReferenced());
        sampler.get().get().destroy(device);

        renderer->getActiveHandleCounts().onDeallocate(vk::HandleType::Sampler);
    }

    mPayload.clear();
}

angle::Result SamplerCache::getSampler(ContextVk *contextVk,
                                       const vk::SamplerDesc &desc,
                                       vk::SamplerBinding *samplerOut)
{
    auto iter = mPayload.find(desc);
    if (iter != mPayload.end())
    {
        vk::RefCountedSampler &sampler = iter->second;
        samplerOut->set(&sampler);
        return angle::Result::Continue;
    }

    vk::SamplerHelper samplerHelper(contextVk);
    ANGLE_TRY(desc.init(contextVk, &samplerHelper.get()));

    vk::RefCountedSampler newSampler(std::move(samplerHelper));
    auto insertedItem                      = mPayload.emplace(desc, std::move(newSampler));
    vk::RefCountedSampler &insertedSampler = insertedItem.first->second;
    samplerOut->set(&insertedSampler);

    contextVk->getRenderer()->getActiveHandleCounts().onAllocate(vk::HandleType::Sampler);

    return angle::Result::Continue;
}
}  // namespace rx
