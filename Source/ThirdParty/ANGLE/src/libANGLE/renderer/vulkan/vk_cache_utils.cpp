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
#include "common/system_utils.h"
#include "common/vulkan/vk_google_filtering_precision.h"
#include "libANGLE/BlobCache.h"
#include "libANGLE/VertexAttribute.h"
#include "libANGLE/renderer/vulkan/DisplayVk.h"
#include "libANGLE/renderer/vulkan/FramebufferVk.h"
#include "libANGLE/renderer/vulkan/ProgramVk.h"
#include "libANGLE/renderer/vulkan/RendererVk.h"
#include "libANGLE/renderer/vulkan/TextureVk.h"
#include "libANGLE/renderer/vulkan/VertexArrayVk.h"
#include "libANGLE/renderer/vulkan/vk_format_utils.h"
#include "libANGLE/renderer/vulkan/vk_helpers.h"

#include <type_traits>

namespace rx
{
constexpr bool kDumpPipelineCacheGraph = false;

namespace vk
{

namespace
{
static_assert(static_cast<uint32_t>(RenderPassLoadOp::Load) == VK_ATTACHMENT_LOAD_OP_LOAD,
              "ConvertRenderPassLoadOpToVkLoadOp must be updated");
static_assert(static_cast<uint32_t>(RenderPassLoadOp::Clear) == VK_ATTACHMENT_LOAD_OP_CLEAR,
              "ConvertRenderPassLoadOpToVkLoadOp must be updated");
static_assert(static_cast<uint32_t>(RenderPassLoadOp::DontCare) == VK_ATTACHMENT_LOAD_OP_DONT_CARE,
              "ConvertRenderPassLoadOpToVkLoadOp must be updated");
static_assert(static_cast<uint32_t>(RenderPassLoadOp::None) == 3,
              "ConvertRenderPassLoadOpToVkLoadOp must be updated");

static_assert(static_cast<uint32_t>(RenderPassStoreOp::Store) == VK_ATTACHMENT_STORE_OP_STORE,
              "ConvertRenderPassStoreOpToVkStoreOp must be updated");
static_assert(static_cast<uint32_t>(RenderPassStoreOp::DontCare) ==
                  VK_ATTACHMENT_STORE_OP_DONT_CARE,
              "ConvertRenderPassStoreOpToVkStoreOp must be updated");
static_assert(static_cast<uint32_t>(RenderPassStoreOp::None) == 2,
              "ConvertRenderPassStoreOpToVkStoreOp must be updated");

constexpr uint16_t kMinSampleShadingScale = angle::BitMask<uint16_t>(8);

VkAttachmentLoadOp ConvertRenderPassLoadOpToVkLoadOp(RenderPassLoadOp loadOp)
{
    return loadOp == RenderPassLoadOp::None ? VK_ATTACHMENT_LOAD_OP_NONE_EXT
                                            : static_cast<VkAttachmentLoadOp>(loadOp);
}
VkAttachmentStoreOp ConvertRenderPassStoreOpToVkStoreOp(RenderPassStoreOp storeOp)
{
    return storeOp == RenderPassStoreOp::None ? VK_ATTACHMENT_STORE_OP_NONE_EXT
                                              : static_cast<VkAttachmentStoreOp>(storeOp);
}

constexpr size_t TransitionBits(size_t size)
{
    return size / kGraphicsPipelineDirtyBitBytes;
}

constexpr size_t kPipelineShadersDescOffset = 0;
constexpr size_t kPipelineShadersDescSize =
    kGraphicsPipelineShadersStateSize + kGraphicsPipelineSharedNonVertexInputStateSize;

constexpr size_t kPipelineFragmentOutputDescOffset = kGraphicsPipelineShadersStateSize;
constexpr size_t kPipelineFragmentOutputDescSize =
    kGraphicsPipelineSharedNonVertexInputStateSize + kGraphicsPipelineFragmentOutputStateSize;

constexpr size_t kPipelineVertexInputDescOffset =
    kGraphicsPipelineShadersStateSize + kPipelineFragmentOutputDescSize;
constexpr size_t kPipelineVertexInputDescSize = kGraphicsPipelineVertexInputStateSize;

static_assert(kPipelineShadersDescOffset % kGraphicsPipelineDirtyBitBytes == 0);
static_assert(kPipelineShadersDescSize % kGraphicsPipelineDirtyBitBytes == 0);

static_assert(kPipelineFragmentOutputDescOffset % kGraphicsPipelineDirtyBitBytes == 0);
static_assert(kPipelineFragmentOutputDescSize % kGraphicsPipelineDirtyBitBytes == 0);

static_assert(kPipelineVertexInputDescOffset % kGraphicsPipelineDirtyBitBytes == 0);
static_assert(kPipelineVertexInputDescSize % kGraphicsPipelineDirtyBitBytes == 0);

constexpr GraphicsPipelineTransitionBits kPipelineShadersTransitionBitsMask =
    GraphicsPipelineTransitionBits::Mask(TransitionBits(kPipelineShadersDescSize) +
                                         TransitionBits(kPipelineShadersDescOffset)) &
    ~GraphicsPipelineTransitionBits::Mask(TransitionBits(kPipelineShadersDescOffset));

constexpr GraphicsPipelineTransitionBits kPipelineFragmentOutputTransitionBitsMask =
    GraphicsPipelineTransitionBits::Mask(TransitionBits(kPipelineFragmentOutputDescSize) +
                                         TransitionBits(kPipelineFragmentOutputDescOffset)) &
    ~GraphicsPipelineTransitionBits::Mask(TransitionBits(kPipelineFragmentOutputDescOffset));

constexpr GraphicsPipelineTransitionBits kPipelineVertexInputTransitionBitsMask =
    GraphicsPipelineTransitionBits::Mask(TransitionBits(kPipelineVertexInputDescSize) +
                                         TransitionBits(kPipelineVertexInputDescOffset)) &
    ~GraphicsPipelineTransitionBits::Mask(TransitionBits(kPipelineVertexInputDescOffset));

bool GraphicsPipelineHasVertexInput(GraphicsPipelineSubset subset)
{
    return subset == GraphicsPipelineSubset::Complete ||
           subset == GraphicsPipelineSubset::VertexInput;
}

bool GraphicsPipelineHasShaders(GraphicsPipelineSubset subset)
{
    return subset == GraphicsPipelineSubset::Complete || subset == GraphicsPipelineSubset::Shaders;
}

bool GraphicsPipelineHasShadersOrFragmentOutput(GraphicsPipelineSubset subset)
{
    return subset != GraphicsPipelineSubset::VertexInput;
}

bool GraphicsPipelineHasFragmentOutput(GraphicsPipelineSubset subset)
{
    return subset == GraphicsPipelineSubset::Complete ||
           subset == GraphicsPipelineSubset::FragmentOutput;
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
        case GL_MULTIPLY_KHR:
            return static_cast<uint8_t>(VK_BLEND_OP_MULTIPLY_EXT - VK_BLEND_OP_ZERO_EXT);
        case GL_SCREEN_KHR:
            return static_cast<uint8_t>(VK_BLEND_OP_SCREEN_EXT - VK_BLEND_OP_ZERO_EXT);
        case GL_OVERLAY_KHR:
            return static_cast<uint8_t>(VK_BLEND_OP_OVERLAY_EXT - VK_BLEND_OP_ZERO_EXT);
        case GL_DARKEN_KHR:
            return static_cast<uint8_t>(VK_BLEND_OP_DARKEN_EXT - VK_BLEND_OP_ZERO_EXT);
        case GL_LIGHTEN_KHR:
            return static_cast<uint8_t>(VK_BLEND_OP_LIGHTEN_EXT - VK_BLEND_OP_ZERO_EXT);
        case GL_COLORDODGE_KHR:
            return static_cast<uint8_t>(VK_BLEND_OP_COLORDODGE_EXT - VK_BLEND_OP_ZERO_EXT);
        case GL_COLORBURN_KHR:
            return static_cast<uint8_t>(VK_BLEND_OP_COLORBURN_EXT - VK_BLEND_OP_ZERO_EXT);
        case GL_HARDLIGHT_KHR:
            return static_cast<uint8_t>(VK_BLEND_OP_HARDLIGHT_EXT - VK_BLEND_OP_ZERO_EXT);
        case GL_SOFTLIGHT_KHR:
            return static_cast<uint8_t>(VK_BLEND_OP_SOFTLIGHT_EXT - VK_BLEND_OP_ZERO_EXT);
        case GL_DIFFERENCE_KHR:
            return static_cast<uint8_t>(VK_BLEND_OP_DIFFERENCE_EXT - VK_BLEND_OP_ZERO_EXT);
        case GL_EXCLUSION_KHR:
            return static_cast<uint8_t>(VK_BLEND_OP_EXCLUSION_EXT - VK_BLEND_OP_ZERO_EXT);
        case GL_HSL_HUE_KHR:
            return static_cast<uint8_t>(VK_BLEND_OP_HSL_HUE_EXT - VK_BLEND_OP_ZERO_EXT);
        case GL_HSL_SATURATION_KHR:
            return static_cast<uint8_t>(VK_BLEND_OP_HSL_SATURATION_EXT - VK_BLEND_OP_ZERO_EXT);
        case GL_HSL_COLOR_KHR:
            return static_cast<uint8_t>(VK_BLEND_OP_HSL_COLOR_EXT - VK_BLEND_OP_ZERO_EXT);
        case GL_HSL_LUMINOSITY_KHR:
            return static_cast<uint8_t>(VK_BLEND_OP_HSL_LUMINOSITY_EXT - VK_BLEND_OP_ZERO_EXT);
        default:
            UNREACHABLE();
            return 0;
    }
}

VkBlendOp UnpackBlendOp(uint8_t packedBlendOp)
{
    if (packedBlendOp <= VK_BLEND_OP_MAX)
    {
        return static_cast<VkBlendOp>(packedBlendOp);
    }
    return static_cast<VkBlendOp>(packedBlendOp + VK_BLEND_OP_ZERO_EXT);
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
        case GL_SRC1_COLOR_EXT:
            return static_cast<uint8_t>(VK_BLEND_FACTOR_SRC1_COLOR);
        case GL_SRC1_ALPHA_EXT:
            return static_cast<uint8_t>(VK_BLEND_FACTOR_SRC1_ALPHA);
        case GL_ONE_MINUS_SRC1_COLOR_EXT:
            return static_cast<uint8_t>(VK_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR);
        case GL_ONE_MINUS_SRC1_ALPHA_EXT:
            return static_cast<uint8_t>(VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA);
        default:
            UNREACHABLE();
            return 0;
    }
}

void UnpackAttachmentDesc(Context *context,
                          VkAttachmentDescription2 *desc,
                          angle::FormatID formatID,
                          uint8_t samples,
                          const PackedAttachmentOpsDesc &ops)
{
    *desc         = {};
    desc->sType   = VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2;
    desc->format  = GetVkFormatFromFormatID(formatID);
    desc->samples = gl_vk::GetSamples(samples);
    desc->loadOp  = ConvertRenderPassLoadOpToVkLoadOp(static_cast<RenderPassLoadOp>(ops.loadOp));
    desc->storeOp =
        ConvertRenderPassStoreOpToVkStoreOp(static_cast<RenderPassStoreOp>(ops.storeOp));
    desc->stencilLoadOp =
        ConvertRenderPassLoadOpToVkLoadOp(static_cast<RenderPassLoadOp>(ops.stencilLoadOp));
    desc->stencilStoreOp =
        ConvertRenderPassStoreOpToVkStoreOp(static_cast<RenderPassStoreOp>(ops.stencilStoreOp));
    desc->initialLayout =
        ConvertImageLayoutToVkImageLayout(context, static_cast<ImageLayout>(ops.initialLayout));
    desc->finalLayout =
        ConvertImageLayoutToVkImageLayout(context, static_cast<ImageLayout>(ops.finalLayout));
}

void UnpackColorResolveAttachmentDesc(VkAttachmentDescription2 *desc,
                                      angle::FormatID formatID,
                                      bool usedAsInputAttachment,
                                      bool isInvalidated)
{
    *desc        = {};
    desc->sType  = VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2;
    desc->format = GetVkFormatFromFormatID(formatID);

    // This function is for color resolve attachments.
    const angle::Format &angleFormat = angle::Format::Get(formatID);
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

void UnpackDepthStencilResolveAttachmentDesc(VkAttachmentDescription2 *desc,
                                             angle::FormatID formatID,
                                             bool usedAsDepthInputAttachment,
                                             bool usedAsStencilInputAttachment,
                                             bool isDepthInvalidated,
                                             bool isStencilInvalidated)
{
    // There cannot be simultaneous usages of the depth/stencil resolve image, as depth/stencil
    // resolve currently only comes from depth/stencil renderbuffers.
    *desc        = {};
    desc->sType  = VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2;
    desc->format = GetVkFormatFromFormatID(formatID);

    // This function is for depth/stencil resolve attachment.
    const angle::Format &angleFormat = angle::Format::Get(formatID);
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

void UnpackStencilState(const PackedStencilOpState &packedState,
                        VkStencilOpState *stateOut,
                        bool writeMaskWorkaround)
{
    // Any non-zero value works for the purposes of the useNonZeroStencilWriteMaskStaticState driver
    // bug workaround.
    constexpr uint32_t kNonZeroWriteMaskForWorkaround = 1;

    stateOut->failOp      = static_cast<VkStencilOp>(packedState.fail);
    stateOut->passOp      = static_cast<VkStencilOp>(packedState.pass);
    stateOut->depthFailOp = static_cast<VkStencilOp>(packedState.depthFail);
    stateOut->compareOp   = static_cast<VkCompareOp>(packedState.compare);
    stateOut->compareMask = 0;
    stateOut->writeMask   = writeMaskWorkaround ? kNonZeroWriteMaskForWorkaround : 0;
    stateOut->reference   = 0;
}

void UnpackBlendAttachmentState(const PackedColorBlendAttachmentState &packedState,
                                VkPipelineColorBlendAttachmentState *stateOut)
{
    stateOut->srcColorBlendFactor = static_cast<VkBlendFactor>(packedState.srcColorBlendFactor);
    stateOut->dstColorBlendFactor = static_cast<VkBlendFactor>(packedState.dstColorBlendFactor);
    stateOut->colorBlendOp        = UnpackBlendOp(packedState.colorBlendOp);
    stateOut->srcAlphaBlendFactor = static_cast<VkBlendFactor>(packedState.srcAlphaBlendFactor);
    stateOut->dstAlphaBlendFactor = static_cast<VkBlendFactor>(packedState.dstAlphaBlendFactor);
    stateOut->alphaBlendOp        = UnpackBlendOp(packedState.alphaBlendOp);
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
    const gl::DrawBuffersVector<VkAttachmentReference2> &drawSubpassColorAttachmentRefs,
    const gl::DrawBuffersVector<VkAttachmentReference2> &drawSubpassResolveAttachmentRefs,
    const VkAttachmentReference2 &depthStencilAttachmentRef,
    const VkAttachmentReference2 &depthStencilResolveAttachmentRef,
    gl::DrawBuffersVector<VkAttachmentReference2> *unresolveColorAttachmentRefs,
    VkAttachmentReference2 *unresolveDepthStencilAttachmentRef,
    FramebufferAttachmentsVector<VkAttachmentReference2> *unresolveInputAttachmentRefs,
    FramebufferAttachmentsVector<uint32_t> *unresolvePreserveAttachmentRefs,
    VkSubpassDescription2 *subpassDesc)
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
    // Additionally, assume Color 0, 4 and 6 are multisampled-render-to-texture (or for any other
    // reason) have corresponding resolve attachments.  Furthermore, say Color 4 and 6 require an
    // initial unresolve operation.
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
    // The subpass that takes the application draw calls has the following attachments, creating the
    // mapping from the Vulkan attachment indices (i.e. RP attachment indices) to GL indices as
    // indicated by the GL shaders:
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
    // The initial subpass that's created here is (remember that in the above example Color 4 and 6
    // need to be unresolved):
    //
    //     Subpass[0] Input[0] -> RP Attachment[7] = Subpass[1] Resolve[4]
    //     Subpass[0] Input[1] -> RP Attachment[8] = Subpass[1] Resolve[6]
    //     Subpass[0] Color[0] -> RP Attachment[2] = Subpass[1] Color[4]
    //     Subpass[0] Color[1] -> RP Attachment[3] = Subpass[1] Color[6]
    //
    // The trick here therefore is to use the color attachment refs already created for the
    // application draw subpass indexed with colorIndexGL.
    //
    // If depth/stencil needs to be unresolved (note that as input attachment, it's inserted before
    // the color attachments.  See UtilsVk::unresolve()):
    //
    //     Subpass[0] Input[0]      -> RP Attachment[9] = Subpass[1] Depth/Stencil Resolve
    //     Subpass[0] Depth/Stencil -> RP Attachment[5] = Subpass[1] Depth/Stencil
    //
    // As an additional note, the attachments that are not used in the unresolve subpass must be
    // preserved.  That is color attachments and the depth/stencil attachment if any.  Resolve
    // attachments are rewritten by the next subpass, so they don't need to be preserved.  Note that
    // there's no need to preserve attachments whose loadOp is DONT_CARE.  For simplicity, we
    // preserve those as well.  The driver would ideally avoid preserving attachments with
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
    // Again, the color attachment refs already created for the application draw subpass can be used
    // indexed with colorIndexGL.
    if (desc.hasDepthStencilUnresolveAttachment())
    {
        ASSERT(desc.hasDepthStencilAttachment());
        ASSERT(desc.hasDepthStencilResolveAttachment());

        *unresolveDepthStencilAttachmentRef = depthStencilAttachmentRef;

        VkAttachmentReference2 unresolveDepthStencilInputAttachmentRef = {};
        unresolveDepthStencilInputAttachmentRef.sType = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2;
        unresolveDepthStencilInputAttachmentRef.attachment =
            depthStencilResolveAttachmentRef.attachment;
        unresolveDepthStencilInputAttachmentRef.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        unresolveDepthStencilInputAttachmentRef.aspectMask = 0;
        if (desc.hasDepthUnresolveAttachment())
        {
            unresolveDepthStencilInputAttachmentRef.aspectMask |= VK_IMAGE_ASPECT_DEPTH_BIT;
        }
        if (desc.hasStencilUnresolveAttachment())
        {
            unresolveDepthStencilInputAttachmentRef.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
        }

        unresolveInputAttachmentRefs->push_back(unresolveDepthStencilInputAttachmentRef);
    }
    else if (desc.hasDepthStencilAttachment())
    {
        // Preserve the depth/stencil attachment if not unresolved.  Again, there's no need to
        // preserve this attachment if loadOp=DONT_CARE, but we do for simplicity.
        unresolvePreserveAttachmentRefs->push_back(depthStencilAttachmentRef.attachment);
    }

    for (uint32_t colorIndexGL = 0; colorIndexGL < desc.colorAttachmentRange(); ++colorIndexGL)
    {
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

    ASSERT(!unresolveColorAttachmentRefs->empty() ||
           unresolveDepthStencilAttachmentRef->attachment != VK_ATTACHMENT_UNUSED);
    ASSERT(unresolveColorAttachmentRefs->size() +
               (desc.hasDepthStencilUnresolveAttachment() ? 1 : 0) ==
           unresolveInputAttachmentRefs->size());

    subpassDesc->sType                = VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_2;
    subpassDesc->pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpassDesc->inputAttachmentCount = static_cast<uint32_t>(unresolveInputAttachmentRefs->size());
    subpassDesc->pInputAttachments    = unresolveInputAttachmentRefs->data();
    subpassDesc->colorAttachmentCount = static_cast<uint32_t>(unresolveColorAttachmentRefs->size());
    subpassDesc->pColorAttachments    = unresolveColorAttachmentRefs->data();
    subpassDesc->pDepthStencilAttachment = unresolveDepthStencilAttachmentRef;
    subpassDesc->preserveAttachmentCount =
        static_cast<uint32_t>(unresolvePreserveAttachmentRefs->size());
    subpassDesc->pPreserveAttachments = unresolvePreserveAttachmentRefs->data();
}

// There is normally one subpass, and occasionally another for the unresolve operation.
constexpr size_t kSubpassFastVectorSize = 2;
template <typename T>
using SubpassVector = angle::FastVector<T, kSubpassFastVectorSize>;

void InitializeUnresolveSubpassDependencies(const SubpassVector<VkSubpassDescription2> &subpassDesc,
                                            bool unresolveColor,
                                            bool unresolveDepthStencil,
                                            std::vector<VkSubpassDependency2> *subpassDependencies)
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

    subpassDependencies->push_back({});
    VkSubpassDependency2 *dependency = &subpassDependencies->back();

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

    dependency->sType           = VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY_2;
    dependency->srcSubpass      = kUnresolveSubpassIndex;
    dependency->dstSubpass      = kDrawSubpassIndex;
    dependency->srcStageMask    = attachmentWriteStages;
    dependency->dstStageMask    = attachmentReadWriteStages;
    dependency->srcAccessMask   = attachmentWriteFlags;
    dependency->dstAccessMask   = attachmentReadWriteFlags;
    dependency->dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    subpassDependencies->push_back({});
    dependency = &subpassDependencies->back();

    // Note again that depth/stencil resolve is considered to be done in the color output stage.
    dependency->sType           = VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY_2;
    dependency->srcSubpass      = kUnresolveSubpassIndex;
    dependency->dstSubpass      = kDrawSubpassIndex;
    dependency->srcStageMask    = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dependency->dstStageMask    = kColorWriteStage;
    dependency->srcAccessMask   = VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
    dependency->dstAccessMask   = kColorWriteFlags;
    dependency->dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
}

// glFramebufferFetchBarrierEXT and glBlendBarrierKHR require a pipeline barrier to be inserted in
// the render pass.  This requires a subpass self-dependency.
//
// For framebuffer fetch:
//
//     srcStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
//     dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
//     srcAccess = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
//     dstAccess = VK_ACCESS_INPUT_ATTACHMENT_READ_BIT
//
// For advanced blend:
//
//     srcStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
//     dstStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
//     srcAccess = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
//     dstAccess = VK_ACCESS_COLOR_ATTACHMENT_READ_NONCOHERENT_BIT_EXT
//
// Subpass dependencies cannot be added after the fact at the end of the render pass due to render
// pass compatibility rules.  ANGLE specifies a subpass self-dependency with the above stage/access
// masks in preparation of potential framebuffer fetch and advanced blend barriers.  This is known
// not to add any overhead on any hardware we have been able to gather information from.
void InitializeDefaultSubpassSelfDependencies(
    Context *context,
    const RenderPassDesc &desc,
    uint32_t subpassIndex,
    std::vector<VkSubpassDependency2> *subpassDependencies)
{
    RendererVk *renderer = context->getRenderer();
    const bool hasRasterizationOrderAttachmentAccess =
        renderer->getFeatures().supportsRasterizationOrderAttachmentAccess.enabled;
    const bool hasBlendOperationAdvanced =
        renderer->getFeatures().supportsBlendOperationAdvanced.enabled;

    if (hasRasterizationOrderAttachmentAccess && !hasBlendOperationAdvanced)
    {
        // No need to specify a subpass dependency if VK_EXT_rasterization_order_attachment_access
        // is enabled, as that extension makes this subpass dependency implicit.
        return;
    }

    subpassDependencies->push_back({});
    VkSubpassDependency2 *dependency = &subpassDependencies->back();

    dependency->sType         = VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY_2;
    dependency->srcSubpass    = subpassIndex;
    dependency->dstSubpass    = subpassIndex;
    dependency->srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency->dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency->srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependency->dstAccessMask = 0;
    if (!hasRasterizationOrderAttachmentAccess)
    {
        dependency->dstStageMask |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        dependency->dstAccessMask |= VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
    }
    if (renderer->getFeatures().supportsBlendOperationAdvanced.enabled)
    {
        dependency->dstAccessMask |= VK_ACCESS_COLOR_ATTACHMENT_READ_NONCOHERENT_BIT_EXT;
    }
    dependency->dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
    if (desc.viewCount() > 0)
    {
        dependency->dependencyFlags |= VK_DEPENDENCY_VIEW_LOCAL_BIT;
    }
}

void InitializeMSRTSS(Context *context,
                      uint8_t renderToTextureSamples,
                      VkSubpassDescription2 *subpass,
                      VkSubpassDescriptionDepthStencilResolve *msrtssResolve,
                      VkMultisampledRenderToSingleSampledInfoEXT *msrtss,
                      VkMultisampledRenderToSingleSampledInfoGOOGLEX *msrtssGOOGLEX)
{
    RendererVk *renderer = context->getRenderer();

    ASSERT(renderer->getFeatures().supportsMultisampledRenderToSingleSampled.enabled ||
           renderer->getFeatures().supportsMultisampledRenderToSingleSampledGOOGLEX.enabled);

    *msrtssResolve                    = {};
    msrtssResolve->sType              = VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_DEPTH_STENCIL_RESOLVE;
    msrtssResolve->depthResolveMode   = VK_RESOLVE_MODE_SAMPLE_ZERO_BIT;
    msrtssResolve->stencilResolveMode = VK_RESOLVE_MODE_SAMPLE_ZERO_BIT;

    *msrtss       = {};
    msrtss->sType = VK_STRUCTURE_TYPE_MULTISAMPLED_RENDER_TO_SINGLE_SAMPLED_INFO_EXT;
    msrtss->pNext = &msrtssResolve;
    msrtss->multisampledRenderToSingleSampledEnable = true;
    msrtss->rasterizationSamples                    = gl_vk::GetSamples(renderToTextureSamples);

    *msrtssGOOGLEX       = {};
    msrtssGOOGLEX->sType = VK_STRUCTURE_TYPE_MULTISAMPLED_RENDER_TO_SINGLE_SAMPLED_INFO_GOOGLEX;
    msrtssGOOGLEX->multisampledRenderToSingleSampledEnable = true;
    msrtssGOOGLEX->rasterizationSamples                    = msrtss->rasterizationSamples;
    msrtssGOOGLEX->depthResolveMode                        = VK_RESOLVE_MODE_SAMPLE_ZERO_BIT;
    msrtssGOOGLEX->stencilResolveMode                      = VK_RESOLVE_MODE_SAMPLE_ZERO_BIT;

    if (renderer->getFeatures().supportsMultisampledRenderToSingleSampled.enabled)
    {
        AddToPNextChain(subpass, msrtss);
    }
    else
    {
        AddToPNextChain(subpass, msrtssGOOGLEX);
    }
}

void SetRenderPassViewMask(Context *context,
                           const uint32_t *viewMask,
                           VkRenderPassCreateInfo2 *createInfo,
                           SubpassVector<VkSubpassDescription2> *subpassDesc)
{
    for (VkSubpassDescription2 &subpass : *subpassDesc)
    {
        subpass.viewMask = *viewMask;
    }

    // For VR, the views are correlated, so this would be an optimization.  However, an
    // application can also use multiview for example to render to all 6 faces of a cubemap, in
    // which case the views are actually not so correlated.  In the absence of any hints from
    // the application, we have to decide on one or the other.  Since VR is more expensive, the
    // views are marked as correlated to optimize that use case.
    createInfo->correlatedViewMaskCount = 1;
    createInfo->pCorrelatedViewMasks    = viewMask;
}

void ToAttachmentDesciption1(const VkAttachmentDescription2 &desc,
                             VkAttachmentDescription *desc1Out)
{
    ASSERT(desc.pNext == nullptr);

    *desc1Out                = {};
    desc1Out->flags          = desc.flags;
    desc1Out->format         = desc.format;
    desc1Out->samples        = desc.samples;
    desc1Out->loadOp         = desc.loadOp;
    desc1Out->storeOp        = desc.storeOp;
    desc1Out->stencilLoadOp  = desc.stencilLoadOp;
    desc1Out->stencilStoreOp = desc.stencilStoreOp;
    desc1Out->initialLayout  = desc.initialLayout;
    desc1Out->finalLayout    = desc.finalLayout;
}

void ToAttachmentReference1(const VkAttachmentReference2 &ref, VkAttachmentReference *ref1Out)
{
    ASSERT(ref.pNext == nullptr);

    *ref1Out            = {};
    ref1Out->attachment = ref.attachment;
    ref1Out->layout     = ref.layout;
}

void ToSubpassDescription1(const VkSubpassDescription2 &desc,
                           const FramebufferAttachmentsVector<VkAttachmentReference> &inputRefs,
                           const gl::DrawBuffersVector<VkAttachmentReference> &colorRefs,
                           const gl::DrawBuffersVector<VkAttachmentReference> &resolveRefs,
                           const VkAttachmentReference &depthStencilRef,
                           VkSubpassDescription *desc1Out)
{
    ASSERT(desc.pNext == nullptr);

    *desc1Out                         = {};
    desc1Out->flags                   = desc.flags;
    desc1Out->pipelineBindPoint       = desc.pipelineBindPoint;
    desc1Out->inputAttachmentCount    = static_cast<uint32_t>(inputRefs.size());
    desc1Out->pInputAttachments       = !inputRefs.empty() ? inputRefs.data() : nullptr;
    desc1Out->colorAttachmentCount    = static_cast<uint32_t>(colorRefs.size());
    desc1Out->pColorAttachments       = !colorRefs.empty() ? colorRefs.data() : nullptr;
    desc1Out->pResolveAttachments     = !resolveRefs.empty() ? resolveRefs.data() : nullptr;
    desc1Out->pDepthStencilAttachment = desc.pDepthStencilAttachment ? &depthStencilRef : nullptr;
    desc1Out->preserveAttachmentCount = desc.preserveAttachmentCount;
    desc1Out->pPreserveAttachments    = desc.pPreserveAttachments;
}

void ToSubpassDependency1(const VkSubpassDependency2 &dep, VkSubpassDependency *dep1Out)
{
    ASSERT(dep.pNext == nullptr);

    *dep1Out                 = {};
    dep1Out->srcSubpass      = dep.srcSubpass;
    dep1Out->dstSubpass      = dep.dstSubpass;
    dep1Out->srcStageMask    = dep.srcStageMask;
    dep1Out->dstStageMask    = dep.dstStageMask;
    dep1Out->srcAccessMask   = dep.srcAccessMask;
    dep1Out->dstAccessMask   = dep.dstAccessMask;
    dep1Out->dependencyFlags = dep.dependencyFlags;
}

void ToRenderPassMultiviewCreateInfo(const VkRenderPassCreateInfo2 &createInfo,
                                     VkRenderPassCreateInfo *createInfo1,
                                     SubpassVector<uint32_t> *viewMasks,
                                     VkRenderPassMultiviewCreateInfo *multiviewInfo)
{
    ASSERT(createInfo.correlatedViewMaskCount == 1);
    const uint32_t viewMask = createInfo.pCorrelatedViewMasks[0];

    viewMasks->resize(createInfo.subpassCount, viewMask);

    multiviewInfo->sType                = VK_STRUCTURE_TYPE_RENDER_PASS_MULTIVIEW_CREATE_INFO;
    multiviewInfo->subpassCount         = createInfo.subpassCount;
    multiviewInfo->pViewMasks           = viewMasks->data();
    multiviewInfo->correlationMaskCount = createInfo.correlatedViewMaskCount;
    multiviewInfo->pCorrelationMasks    = createInfo.pCorrelatedViewMasks;

    AddToPNextChain(createInfo1, multiviewInfo);
}

angle::Result CreateRenderPass1(Context *context,
                                const VkRenderPassCreateInfo2 &createInfo,
                                uint8_t viewCount,
                                RenderPass *renderPass)
{
    // Convert the attachments to VkAttachmentDescription.
    FramebufferAttachmentArray<VkAttachmentDescription> attachmentDescs;
    for (uint32_t index = 0; index < createInfo.attachmentCount; ++index)
    {
        ToAttachmentDesciption1(createInfo.pAttachments[index], &attachmentDescs[index]);
    }

    // Convert subpass attachments to VkAttachmentReference and the subpass description to
    // VkSubpassDescription.
    SubpassVector<FramebufferAttachmentsVector<VkAttachmentReference>> subpassInputAttachmentRefs(
        createInfo.subpassCount);
    SubpassVector<gl::DrawBuffersVector<VkAttachmentReference>> subpassColorAttachmentRefs(
        createInfo.subpassCount);
    SubpassVector<gl::DrawBuffersVector<VkAttachmentReference>> subpassResolveAttachmentRefs(
        createInfo.subpassCount);
    SubpassVector<VkAttachmentReference> subpassDepthStencilAttachmentRefs(createInfo.subpassCount);
    SubpassVector<VkSubpassDescription> subpassDescriptions(createInfo.subpassCount);
    for (uint32_t subpass = 0; subpass < createInfo.subpassCount; ++subpass)
    {
        const VkSubpassDescription2 &desc = createInfo.pSubpasses[subpass];
        FramebufferAttachmentsVector<VkAttachmentReference> &inputRefs =
            subpassInputAttachmentRefs[subpass];
        gl::DrawBuffersVector<VkAttachmentReference> &colorRefs =
            subpassColorAttachmentRefs[subpass];
        gl::DrawBuffersVector<VkAttachmentReference> &resolveRefs =
            subpassResolveAttachmentRefs[subpass];
        VkAttachmentReference &depthStencilRef = subpassDepthStencilAttachmentRefs[subpass];

        inputRefs.resize(desc.inputAttachmentCount);
        colorRefs.resize(desc.colorAttachmentCount);

        for (uint32_t index = 0; index < desc.inputAttachmentCount; ++index)
        {
            ToAttachmentReference1(desc.pInputAttachments[index], &inputRefs[index]);
        }

        for (uint32_t index = 0; index < desc.colorAttachmentCount; ++index)
        {
            ToAttachmentReference1(desc.pColorAttachments[index], &colorRefs[index]);
        }
        if (desc.pResolveAttachments)
        {
            resolveRefs.resize(desc.colorAttachmentCount);
            for (uint32_t index = 0; index < desc.colorAttachmentCount; ++index)
            {
                ToAttachmentReference1(desc.pResolveAttachments[index], &resolveRefs[index]);
            }
        }
        if (desc.pDepthStencilAttachment)
        {
            ToAttachmentReference1(*desc.pDepthStencilAttachment, &depthStencilRef);
        }

        // Convert subpass itself.
        ToSubpassDescription1(desc, inputRefs, colorRefs, resolveRefs, depthStencilRef,
                              &subpassDescriptions[subpass]);
    }

    // Convert subpass dependencies to VkSubpassDependency.
    std::vector<VkSubpassDependency> subpassDependencies(createInfo.dependencyCount);
    for (uint32_t index = 0; index < createInfo.dependencyCount; ++index)
    {
        ToSubpassDependency1(createInfo.pDependencies[index], &subpassDependencies[index]);
    }

    // Convert CreateInfo itself
    VkRenderPassCreateInfo createInfo1 = {};
    createInfo1.sType                  = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    createInfo1.flags                  = createInfo.flags;
    createInfo1.attachmentCount        = createInfo.attachmentCount;
    createInfo1.pAttachments           = attachmentDescs.data();
    createInfo1.subpassCount           = createInfo.subpassCount;
    createInfo1.pSubpasses             = subpassDescriptions.data();
    createInfo1.dependencyCount        = static_cast<uint32_t>(subpassDependencies.size());
    createInfo1.pDependencies = !subpassDependencies.empty() ? subpassDependencies.data() : nullptr;

    SubpassVector<uint32_t> viewMasks;
    VkRenderPassMultiviewCreateInfo multiviewInfo = {};
    if (viewCount > 0)
    {
        ToRenderPassMultiviewCreateInfo(createInfo, &createInfo1, &viewMasks, &multiviewInfo);
    }

    // Initialize the render pass.
    ANGLE_VK_TRY(context, renderPass->init(context->getDevice(), createInfo1));

    return angle::Result::Continue;
}

void UpdateRenderPassColorPerfCounters(const VkRenderPassCreateInfo2 &createInfo,
                                       FramebufferAttachmentMask depthStencilAttachmentIndices,
                                       RenderPassPerfCounters *countersOut)
{
    for (uint32_t index = 0; index < createInfo.attachmentCount; index++)
    {
        if (depthStencilAttachmentIndices.test(index))
        {
            continue;
        }

        VkAttachmentLoadOp loadOp   = createInfo.pAttachments[index].loadOp;
        VkAttachmentStoreOp storeOp = createInfo.pAttachments[index].storeOp;
        countersOut->colorLoadOpClears += loadOp == VK_ATTACHMENT_LOAD_OP_CLEAR ? 1 : 0;
        countersOut->colorLoadOpLoads += loadOp == VK_ATTACHMENT_LOAD_OP_LOAD ? 1 : 0;
        countersOut->colorLoadOpNones += loadOp == VK_ATTACHMENT_LOAD_OP_NONE_EXT ? 1 : 0;
        countersOut->colorStoreOpStores += storeOp == VK_ATTACHMENT_STORE_OP_STORE ? 1 : 0;
        countersOut->colorStoreOpNones += storeOp == VK_ATTACHMENT_STORE_OP_NONE_EXT ? 1 : 0;
    }
}

void UpdateSubpassColorPerfCounters(const VkRenderPassCreateInfo2 &createInfo,
                                    const VkSubpassDescription2 &subpass,
                                    RenderPassPerfCounters *countersOut)
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

void UpdateRenderPassDepthStencilPerfCounters(const VkRenderPassCreateInfo2 &createInfo,
                                              size_t renderPassIndex,
                                              RenderPassPerfCounters *countersOut)
{
    ASSERT(renderPassIndex != VK_ATTACHMENT_UNUSED);

    // Depth/stencil ops counters.
    const VkAttachmentDescription2 &ds = createInfo.pAttachments[renderPassIndex];

    countersOut->depthLoadOpClears += ds.loadOp == VK_ATTACHMENT_LOAD_OP_CLEAR ? 1 : 0;
    countersOut->depthLoadOpLoads += ds.loadOp == VK_ATTACHMENT_LOAD_OP_LOAD ? 1 : 0;
    countersOut->depthLoadOpNones += ds.loadOp == VK_ATTACHMENT_LOAD_OP_NONE_EXT ? 1 : 0;
    countersOut->depthStoreOpStores += ds.storeOp == VK_ATTACHMENT_STORE_OP_STORE ? 1 : 0;
    countersOut->depthStoreOpNones += ds.storeOp == VK_ATTACHMENT_STORE_OP_NONE_EXT ? 1 : 0;

    countersOut->stencilLoadOpClears += ds.stencilLoadOp == VK_ATTACHMENT_LOAD_OP_CLEAR ? 1 : 0;
    countersOut->stencilLoadOpLoads += ds.stencilLoadOp == VK_ATTACHMENT_LOAD_OP_LOAD ? 1 : 0;
    countersOut->stencilLoadOpNones += ds.stencilLoadOp == VK_ATTACHMENT_LOAD_OP_NONE_EXT ? 1 : 0;
    countersOut->stencilStoreOpStores += ds.stencilStoreOp == VK_ATTACHMENT_STORE_OP_STORE ? 1 : 0;
    countersOut->stencilStoreOpNones +=
        ds.stencilStoreOp == VK_ATTACHMENT_STORE_OP_NONE_EXT ? 1 : 0;

    // Depth/stencil read-only mode.
    countersOut->readOnlyDepthStencil +=
        ds.finalLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL ? 1 : 0;
}

void UpdateRenderPassDepthStencilResolvePerfCounters(
    const VkRenderPassCreateInfo2 &createInfo,
    const VkSubpassDescriptionDepthStencilResolve &depthStencilResolve,
    RenderPassPerfCounters *countersOut)
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

    const VkAttachmentDescription2 &dsResolve = createInfo.pAttachments[resolveRenderPassIndex];

    // Resolve depth/stencil ops counters.
    countersOut->depthLoadOpClears += dsResolve.loadOp == VK_ATTACHMENT_LOAD_OP_CLEAR ? 1 : 0;
    countersOut->depthLoadOpLoads += dsResolve.loadOp == VK_ATTACHMENT_LOAD_OP_LOAD ? 1 : 0;
    countersOut->depthStoreOpStores +=
        dsResolve.storeOp == static_cast<uint16_t>(RenderPassStoreOp::Store) ? 1 : 0;

    countersOut->stencilLoadOpClears +=
        dsResolve.stencilLoadOp == VK_ATTACHMENT_LOAD_OP_CLEAR ? 1 : 0;
    countersOut->stencilLoadOpLoads +=
        dsResolve.stencilLoadOp == VK_ATTACHMENT_LOAD_OP_LOAD ? 1 : 0;
    countersOut->stencilStoreOpStores +=
        dsResolve.stencilStoreOp == static_cast<uint16_t>(RenderPassStoreOp::Store) ? 1 : 0;

    // Depth/stencil resolve counters.
    countersOut->depthAttachmentResolves +=
        depthStencilResolve.depthResolveMode != VK_RESOLVE_MODE_NONE ? 1 : 0;
    countersOut->stencilAttachmentResolves +=
        depthStencilResolve.stencilResolveMode != VK_RESOLVE_MODE_NONE ? 1 : 0;
}

void UpdateRenderPassPerfCounters(
    const RenderPassDesc &desc,
    const VkRenderPassCreateInfo2 &createInfo,
    const VkSubpassDescriptionDepthStencilResolve &depthStencilResolve,
    RenderPassPerfCounters *countersOut)
{
    // Accumulate depth/stencil attachment indices in all subpasses to avoid double-counting
    // counters.
    FramebufferAttachmentMask depthStencilAttachmentIndices;

    for (uint32_t subpassIndex = 0; subpassIndex < createInfo.subpassCount; ++subpassIndex)
    {
        const VkSubpassDescription2 &subpass = createInfo.pSubpasses[subpassIndex];

        // Color counters.
        // NOTE: For simplicity, this will accumulate counts for all subpasses in the renderpass.
        UpdateSubpassColorPerfCounters(createInfo, subpass, countersOut);

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

    UpdateRenderPassColorPerfCounters(createInfo, depthStencilAttachmentIndices, countersOut);

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
                                           RenderPassHelper *renderPassHelper)
{
    constexpr VkAttachmentReference2 kUnusedAttachment = {VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2,
                                                          nullptr, VK_ATTACHMENT_UNUSED,
                                                          VK_IMAGE_LAYOUT_UNDEFINED, 0};

    const bool needInputAttachments = desc.hasFramebufferFetch();
    const bool isRenderToTextureThroughExtension =
        desc.isRenderToTexture() &&
        (contextVk->getFeatures().supportsMultisampledRenderToSingleSampled.enabled ||
         contextVk->getFeatures().supportsMultisampledRenderToSingleSampledGOOGLEX.enabled);
    const bool isRenderToTextureThroughEmulation =
        desc.isRenderToTexture() && !isRenderToTextureThroughExtension;

    const uint8_t descSamples            = desc.samples();
    const uint8_t attachmentSamples      = isRenderToTextureThroughExtension ? 1 : descSamples;
    const uint8_t renderToTextureSamples = isRenderToTextureThroughExtension ? descSamples : 1;

    // Unpack the packed and split representation into the format required by Vulkan.
    gl::DrawBuffersVector<VkAttachmentReference2> colorAttachmentRefs;
    gl::DrawBuffersVector<VkAttachmentReference2> colorResolveAttachmentRefs;
    VkAttachmentReference2 depthStencilAttachmentRef        = kUnusedAttachment;
    VkAttachmentReference2 depthStencilResolveAttachmentRef = kUnusedAttachment;

    // The list of attachments includes all non-resolve and resolve attachments.
    FramebufferAttachmentArray<VkAttachmentDescription2> attachmentDescs;

    // Track invalidated attachments so their resolve attachments can be invalidated as well.
    // Resolve attachments can be removed in that case if the render pass has only one subpass
    // (which is the case if there are no unresolve attachments).
    gl::DrawBufferMask isColorInvalidated;
    bool isDepthInvalidated   = false;
    bool isStencilInvalidated = false;
    const bool hasUnresolveAttachments =
        desc.getColorUnresolveAttachmentMask().any() || desc.hasDepthStencilUnresolveAttachment();
    const bool canRemoveResolveAttachments =
        isRenderToTextureThroughEmulation && !hasUnresolveAttachments;

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

        angle::FormatID attachmentFormatID = desc[colorIndexGL];
        ASSERT(attachmentFormatID != angle::FormatID::NONE);

        VkAttachmentReference2 colorRef = {};
        colorRef.sType                  = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2;
        colorRef.attachment             = attachmentCount.get();
        colorRef.layout =
            needInputAttachments
                ? VK_IMAGE_LAYOUT_GENERAL
                : ConvertImageLayoutToVkImageLayout(
                      contextVk, static_cast<ImageLayout>(ops[attachmentCount].initialLayout));
        colorRef.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        colorAttachmentRefs.push_back(colorRef);

        UnpackAttachmentDesc(contextVk, &attachmentDescs[attachmentCount.get()], attachmentFormatID,
                             attachmentSamples, ops[attachmentCount]);

        // If this renderpass uses EXT_srgb_write_control, we need to override the format to its
        // linear counterpart. Formats that cannot be reinterpreted are exempt from this
        // requirement.
        angle::FormatID linearFormat = rx::ConvertToLinear(attachmentFormatID);
        if (linearFormat != angle::FormatID::NONE)
        {
            if (desc.getSRGBWriteControlMode() == gl::SrgbWriteControlMode::Linear)
            {
                attachmentFormatID = linearFormat;
            }
        }
        attachmentDescs[attachmentCount.get()].format = GetVkFormatFromFormatID(attachmentFormatID);
        ASSERT(attachmentDescs[attachmentCount.get()].format != VK_FORMAT_UNDEFINED);

        isColorInvalidated.set(colorIndexGL, ops[attachmentCount].isInvalidated);

        ++attachmentCount;
    }

    // Pack depth/stencil attachment, if any
    if (desc.hasDepthStencilAttachment())
    {
        uint32_t depthStencilIndexGL = static_cast<uint32_t>(desc.depthStencilAttachmentIndex());

        angle::FormatID attachmentFormatID = desc[depthStencilIndexGL];
        ASSERT(attachmentFormatID != angle::FormatID::NONE);

        depthStencilAttachmentRef.sType      = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2;
        depthStencilAttachmentRef.attachment = attachmentCount.get();
        depthStencilAttachmentRef.layout     = ConvertImageLayoutToVkImageLayout(
            contextVk, static_cast<ImageLayout>(ops[attachmentCount].initialLayout));
        depthStencilAttachmentRef.aspectMask =
            VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;

        UnpackAttachmentDesc(contextVk, &attachmentDescs[attachmentCount.get()], attachmentFormatID,
                             attachmentSamples, ops[attachmentCount]);

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

        angle::FormatID attachmentFormatID = desc[colorIndexGL];

        VkAttachmentReference2 colorRef = {};
        colorRef.sType                  = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2;
        colorRef.attachment             = attachmentCount.get();
        colorRef.layout                 = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        colorRef.aspectMask             = VK_IMAGE_ASPECT_COLOR_BIT;

        // If color attachment is invalidated, try to remove its resolve attachment altogether.
        if (canRemoveResolveAttachments && isColorInvalidated.test(colorIndexGL))
        {
            colorResolveAttachmentRefs.push_back(kUnusedAttachment);
        }
        else
        {
            colorResolveAttachmentRefs.push_back(colorRef);
        }

        // When multisampled-render-to-texture is used, invalidating an attachment invalidates both
        // the multisampled and the resolve attachments.  Otherwise, the resolve attachment is
        // independent of the multisampled attachment, and is never invalidated.
        UnpackColorResolveAttachmentDesc(
            &attachmentDescs[attachmentCount.get()], attachmentFormatID,
            desc.hasColorUnresolveAttachment(colorIndexGL),
            isColorInvalidated.test(colorIndexGL) && isRenderToTextureThroughEmulation);

        ++attachmentCount;
    }

    // Pack depth/stencil resolve attachment, if any
    if (desc.hasDepthStencilResolveAttachment())
    {
        ASSERT(desc.hasDepthStencilAttachment());

        uint32_t depthStencilIndexGL = static_cast<uint32_t>(desc.depthStencilAttachmentIndex());

        angle::FormatID attachmentFormatID = desc[depthStencilIndexGL];
        const angle::Format &angleFormat   = angle::Format::Get(attachmentFormatID);

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
            &attachmentDescs[attachmentCount.get()], attachmentFormatID,
            desc.hasDepthUnresolveAttachment(), desc.hasStencilUnresolveAttachment(),
            isDepthInvalidated, isStencilInvalidated);

        ++attachmentCount;
    }

    SubpassVector<VkSubpassDescription2> subpassDesc;

    // If any attachment needs to be unresolved, create an initial subpass for this purpose.  Note
    // that the following arrays are used in initializing a VkSubpassDescription2 in subpassDesc,
    // which is in turn used in VkRenderPassCreateInfo below.  That is why they are declared in the
    // same scope.
    gl::DrawBuffersVector<VkAttachmentReference2> unresolveColorAttachmentRefs;
    VkAttachmentReference2 unresolveDepthStencilAttachmentRef = kUnusedAttachment;
    FramebufferAttachmentsVector<VkAttachmentReference2> unresolveInputAttachmentRefs;
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
    VkSubpassDescription2 *applicationSubpass = &subpassDesc.back();

    applicationSubpass->sType             = VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_2;
    applicationSubpass->pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    applicationSubpass->inputAttachmentCount =
        needInputAttachments ? static_cast<uint32_t>(colorAttachmentRefs.size()) : 0;
    applicationSubpass->pInputAttachments =
        needInputAttachments ? colorAttachmentRefs.data() : nullptr;
    applicationSubpass->colorAttachmentCount = static_cast<uint32_t>(colorAttachmentRefs.size());
    applicationSubpass->pColorAttachments    = colorAttachmentRefs.data();
    applicationSubpass->pResolveAttachments  = attachmentCount.get() > nonResolveAttachmentCount
                                                   ? colorResolveAttachmentRefs.data()
                                                   : nullptr;
    applicationSubpass->pDepthStencilAttachment =
        (depthStencilAttachmentRef.attachment != VK_ATTACHMENT_UNUSED ? &depthStencilAttachmentRef
                                                                      : nullptr);

    // Specify rasterization order for color on the subpass when available and
    // there is framebuffer fetch.  This is required when the corresponding
    // flag is set on the pipeline.
    if (contextVk->getFeatures().supportsRasterizationOrderAttachmentAccess.enabled &&
        desc.hasFramebufferFetch())
    {
        for (VkSubpassDescription2 &subpass : subpassDesc)
        {
            subpass.flags |=
                VK_SUBPASS_DESCRIPTION_RASTERIZATION_ORDER_ATTACHMENT_COLOR_ACCESS_BIT_EXT;
        }
    }

    // If depth/stencil is to be resolved, add a VkSubpassDescriptionDepthStencilResolve to the
    // pNext chain of the subpass description.
    VkSubpassDescriptionDepthStencilResolve depthStencilResolve  = {};
    VkSubpassDescriptionDepthStencilResolve msrtssResolve        = {};
    VkMultisampledRenderToSingleSampledInfoEXT msrtss            = {};
    VkMultisampledRenderToSingleSampledInfoGOOGLEX msrtssGOOGLEX = {};
    if (desc.hasDepthStencilResolveAttachment())
    {
        ASSERT(!isRenderToTextureThroughExtension);

        depthStencilResolve.sType = VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_DEPTH_STENCIL_RESOLVE;
        depthStencilResolve.depthResolveMode   = VK_RESOLVE_MODE_SAMPLE_ZERO_BIT;
        depthStencilResolve.stencilResolveMode = VK_RESOLVE_MODE_SAMPLE_ZERO_BIT;

        // If depth/stencil attachment is invalidated, try to remove its resolve attachment
        // altogether.
        const bool removeDepthStencilResolve =
            canRemoveResolveAttachments && isDepthInvalidated && isStencilInvalidated;
        if (!removeDepthStencilResolve)
        {
            depthStencilResolve.pDepthStencilResolveAttachment = &depthStencilResolveAttachmentRef;
        }

        AddToPNextChain(&subpassDesc.back(), &depthStencilResolve);
    }
    else if (isRenderToTextureThroughExtension)
    {
        ASSERT(subpassDesc.size() == 1);
        InitializeMSRTSS(contextVk, renderToTextureSamples, &subpassDesc.back(), &msrtssResolve,
                         &msrtss, &msrtssGOOGLEX);
    }

    std::vector<VkSubpassDependency2> subpassDependencies;
    if (hasUnresolveAttachments)
    {
        InitializeUnresolveSubpassDependencies(
            subpassDesc, desc.getColorUnresolveAttachmentMask().any(),
            desc.hasDepthStencilUnresolveAttachment(), &subpassDependencies);
    }

    const uint32_t drawSubpassIndex = static_cast<uint32_t>(subpassDesc.size()) - 1;
    InitializeDefaultSubpassSelfDependencies(contextVk, desc, drawSubpassIndex,
                                             &subpassDependencies);

    VkRenderPassCreateInfo2 createInfo = {};
    createInfo.sType                   = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO_2;
    createInfo.attachmentCount         = attachmentCount.get();
    createInfo.pAttachments            = attachmentDescs.data();
    createInfo.subpassCount            = static_cast<uint32_t>(subpassDesc.size());
    createInfo.pSubpasses              = subpassDesc.data();

    if (!subpassDependencies.empty())
    {
        createInfo.dependencyCount = static_cast<uint32_t>(subpassDependencies.size());
        createInfo.pDependencies   = subpassDependencies.data();
    }

    const uint32_t viewMask = angle::BitMask<uint32_t>(desc.viewCount());
    if (desc.viewCount() > 0)
    {
        SetRenderPassViewMask(contextVk, &viewMask, &createInfo, &subpassDesc);
    }

    // If VK_KHR_create_renderpass2 is not supported, we must use core Vulkan 1.0.  This is
    // increasingly uncommon.  Note that extensions that require chaining information to subpasses
    // are automatically not used when this extension is not available.
    if (!contextVk->getFeatures().supportsRenderpass2.enabled)
    {
        ANGLE_TRY(CreateRenderPass1(contextVk, createInfo, desc.viewCount(),
                                    &renderPassHelper->getRenderPass()));
    }
    else
    {
        ANGLE_VK_TRY(contextVk,
                     renderPassHelper->getRenderPass().init2(contextVk->getDevice(), createInfo));
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
                                    RenderPassHelper *renderPassHelper,
                                    const RenderPass **renderPassOut)
{
    *renderPassOut = &renderPassHelper->getRenderPass();
    if (updatePerfCounters)
    {
        angle::VulkanPerfCounters &counters      = contextVk->getPerfCounters();
        const RenderPassPerfCounters &rpCounters = renderPassHelper->getPerfCounters();

        counters.colorLoadOpClears += rpCounters.colorLoadOpClears;
        counters.colorLoadOpLoads += rpCounters.colorLoadOpLoads;
        counters.colorLoadOpNones += rpCounters.colorLoadOpNones;
        counters.colorStoreOpStores += rpCounters.colorStoreOpStores;
        counters.colorStoreOpNones += rpCounters.colorStoreOpNones;
        counters.depthLoadOpClears += rpCounters.depthLoadOpClears;
        counters.depthLoadOpLoads += rpCounters.depthLoadOpLoads;
        counters.depthLoadOpNones += rpCounters.depthLoadOpNones;
        counters.depthStoreOpStores += rpCounters.depthStoreOpStores;
        counters.depthStoreOpNones += rpCounters.depthStoreOpNones;
        counters.stencilLoadOpClears += rpCounters.stencilLoadOpClears;
        counters.stencilLoadOpLoads += rpCounters.stencilLoadOpLoads;
        counters.stencilLoadOpNones += rpCounters.stencilLoadOpNones;
        counters.stencilStoreOpStores += rpCounters.stencilStoreOpStores;
        counters.stencilStoreOpNones += rpCounters.stencilStoreOpNones;
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
    const SpecializationConstants &specConsts,
    SpecializationConstantMap<VkSpecializationMapEntry> *specializationEntriesOut,
    VkSpecializationInfo *specializationInfoOut)
{
    // Collect specialization constants.
    for (const sh::vk::SpecializationConstantId id :
         angle::AllEnums<sh::vk::SpecializationConstantId>())
    {
        (*specializationEntriesOut)[id].constantID = static_cast<uint32_t>(id);
        switch (id)
        {
            case sh::vk::SpecializationConstantId::SurfaceRotation:
                (*specializationEntriesOut)[id].offset =
                    offsetof(SpecializationConstants, surfaceRotation);
                (*specializationEntriesOut)[id].size = sizeof(specConsts.surfaceRotation);
                break;
            case sh::vk::SpecializationConstantId::Dither:
                (*specializationEntriesOut)[id].offset =
                    offsetof(vk::SpecializationConstants, dither);
                (*specializationEntriesOut)[id].size = sizeof(specConsts.dither);
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

// Adjust border color value according to intended format.
gl::ColorGeneric AdjustBorderColor(const angle::ColorGeneric &borderColorGeneric,
                                   const angle::Format &format,
                                   bool stencilMode)
{
    gl::ColorGeneric adjustedBorderColor = borderColorGeneric;

    // Handle depth formats
    if (format.hasDepthOrStencilBits())
    {
        if (stencilMode)
        {
            // Stencil component
            adjustedBorderColor.colorUI.red = gl::clampForBitCount<unsigned int>(
                borderColorGeneric.colorUI.red, format.stencilBits);
        }
        else
        {
            // Depth component
            if (format.isUnorm())
            {
                adjustedBorderColor.colorF.red = gl::clamp01(borderColorGeneric.colorF.red);
            }
        }

        return adjustedBorderColor;
    }

    // Handle LUMA formats
    if (format.isLUMA() && format.alphaBits > 0)
    {
        if (format.luminanceBits > 0)
        {
            adjustedBorderColor.colorF.green = borderColorGeneric.colorF.alpha;
        }
        else
        {
            adjustedBorderColor.colorF.red = borderColorGeneric.colorF.alpha;
        }

        return adjustedBorderColor;
    }

    // Handle all other formats
    if (format.isSint())
    {
        adjustedBorderColor.colorI.red =
            gl::clampForBitCount<int>(borderColorGeneric.colorI.red, format.redBits);
        adjustedBorderColor.colorI.green =
            gl::clampForBitCount<int>(borderColorGeneric.colorI.green, format.greenBits);
        adjustedBorderColor.colorI.blue =
            gl::clampForBitCount<int>(borderColorGeneric.colorI.blue, format.blueBits);
        adjustedBorderColor.colorI.alpha =
            gl::clampForBitCount<int>(borderColorGeneric.colorI.alpha, format.alphaBits);
    }
    else if (format.isUint())
    {
        adjustedBorderColor.colorUI.red =
            gl::clampForBitCount<unsigned int>(borderColorGeneric.colorUI.red, format.redBits);
        adjustedBorderColor.colorUI.green =
            gl::clampForBitCount<unsigned int>(borderColorGeneric.colorUI.green, format.greenBits);
        adjustedBorderColor.colorUI.blue =
            gl::clampForBitCount<unsigned int>(borderColorGeneric.colorUI.blue, format.blueBits);
        adjustedBorderColor.colorUI.alpha =
            gl::clampForBitCount<unsigned int>(borderColorGeneric.colorUI.alpha, format.alphaBits);
    }
    else if (format.isSnorm())
    {
        // clamp between -1.0f and 1.0f
        adjustedBorderColor.colorF.red   = gl::clamp(borderColorGeneric.colorF.red, -1.0f, 1.0f);
        adjustedBorderColor.colorF.green = gl::clamp(borderColorGeneric.colorF.green, -1.0f, 1.0f);
        adjustedBorderColor.colorF.blue  = gl::clamp(borderColorGeneric.colorF.blue, -1.0f, 1.0f);
        adjustedBorderColor.colorF.alpha = gl::clamp(borderColorGeneric.colorF.alpha, -1.0f, 1.0f);
    }
    else if (format.isUnorm())
    {
        // clamp between 0.0f and 1.0f
        adjustedBorderColor.colorF.red   = gl::clamp01(borderColorGeneric.colorF.red);
        adjustedBorderColor.colorF.green = gl::clamp01(borderColorGeneric.colorF.green);
        adjustedBorderColor.colorF.blue  = gl::clamp01(borderColorGeneric.colorF.blue);
        adjustedBorderColor.colorF.alpha = gl::clamp01(borderColorGeneric.colorF.alpha);
    }

    return adjustedBorderColor;
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
// Uses the 'offsetof' macro to compute the offset 'Member' within the PipelineDesc.
// We can optimize the dirty bit setting by computing the shifted dirty bit at compile time instead
// of calling "set".
#define ANGLE_GET_TRANSITION_BIT(Member) \
    (offsetof(GraphicsPipelineDesc, Member) >> kTransitionByteShift)

// Indexed dirty bits cannot be entirely computed at compile time since the index is passed to
// the update function.
#define ANGLE_GET_INDEXED_TRANSITION_BIT(Member, Index, BitWidth) \
    (((BitWidth * Index) >> kTransitionBitShift) + ANGLE_GET_TRANSITION_BIT(Member))

constexpr angle::PackedEnumMap<gl::ComponentType, VkFormat> kMismatchedComponentTypeMap = {{
    {gl::ComponentType::Float, VK_FORMAT_R32G32B32A32_SFLOAT},
    {gl::ComponentType::Int, VK_FORMAT_R32G32B32A32_SINT},
    {gl::ComponentType::UnsignedInt, VK_FORMAT_R32G32B32A32_UINT},
}};

constexpr char kDescriptorTypeNameMap[][30] = {"sampler",
                                               "combined image sampler",
                                               "sampled image",
                                               "storage image",
                                               "uniform texel buffer",
                                               "storage texel buffer",
                                               "uniform buffer",
                                               "storage buffer",
                                               "uniform buffer dynamic",
                                               "storage buffer dynamic",
                                               "input attachment"};

// Helpers for creating a readable dump of the graphics pipeline graph.  Each program generates a
// group of nodes.  The group's description is the common state among all nodes.  Each node contains
// the diff with the shared state.  Arrows between nodes indicate the GraphicsPipelineTransitionBits
// that have caused the transition.  State that is 0 is not output for brevity.
enum class PipelineState
{
    VertexAttribFormat,
    VertexAttribDivisor             = VertexAttribFormat + gl::MAX_VERTEX_ATTRIBS,
    VertexAttribOffset              = VertexAttribDivisor + gl::MAX_VERTEX_ATTRIBS,
    VertexAttribStride              = VertexAttribOffset + gl::MAX_VERTEX_ATTRIBS,
    VertexAttribCompressed          = VertexAttribStride + gl::MAX_VERTEX_ATTRIBS,
    VertexAttribShaderComponentType = VertexAttribCompressed + gl::MAX_VERTEX_ATTRIBS,
    RenderPassSamples               = VertexAttribShaderComponentType + gl::MAX_VERTEX_ATTRIBS,
    RenderPassColorAttachmentRange,
    RenderPassViewCount,
    RenderPassSrgbWriteControl,
    RenderPassHasFramebufferFetch,
    RenderPassIsRenderToTexture,
    RenderPassResolveDepthStencil,
    RenderPassUnresolveDepth,
    RenderPassUnresolveStencil,
    RenderPassColorResolveMask,
    RenderPassColorUnresolveMask,
    RenderPassColorFormat,
    RenderPassDepthStencilFormat = RenderPassColorFormat + gl::IMPLEMENTATION_MAX_DRAW_BUFFERS,
    Subpass,
    Topology,
    PatchVertices,
    PrimitiveRestartEnable,
    CullMode,
    FrontFace,
    SurfaceRotation,
    ViewportNegativeOneToOne,
    SampleShadingEnable,
    RasterizationSamples,
    MinSampleShading,
    SampleMask,
    AlphaToCoverageEnable,
    AlphaToOneEnable,
    LogicOpEnable,
    LogicOp,
    RasterizerDiscardEnable,
    ColorWriteMask,
    BlendEnableMask = ColorWriteMask + gl::IMPLEMENTATION_MAX_DRAW_BUFFERS,
    MissingOutputsMask,
    SrcColorBlendFactor,
    DstColorBlendFactor   = SrcColorBlendFactor + gl::IMPLEMENTATION_MAX_DRAW_BUFFERS,
    ColorBlendOp          = DstColorBlendFactor + gl::IMPLEMENTATION_MAX_DRAW_BUFFERS,
    SrcAlphaBlendFactor   = ColorBlendOp + gl::IMPLEMENTATION_MAX_DRAW_BUFFERS,
    DstAlphaBlendFactor   = SrcAlphaBlendFactor + gl::IMPLEMENTATION_MAX_DRAW_BUFFERS,
    AlphaBlendOp          = DstAlphaBlendFactor + gl::IMPLEMENTATION_MAX_DRAW_BUFFERS,
    EmulatedDitherControl = AlphaBlendOp + gl::IMPLEMENTATION_MAX_DRAW_BUFFERS,
    DepthClampEnable,
    DepthBoundsTest,
    DepthCompareOp,
    DepthTest,
    DepthWrite,
    StencilTest,
    DepthBiasEnable,
    StencilOpFailFront,
    StencilOpPassFront,
    StencilOpDepthFailFront,
    StencilCompareFront,
    StencilOpFailBack,
    StencilOpPassBack,
    StencilOpDepthFailBack,
    StencilCompareBack,

    InvalidEnum,
    EnumCount = InvalidEnum,
};

using UnpackedPipelineState = angle::PackedEnumMap<PipelineState, uint32_t>;
using PipelineStateBitSet   = angle::BitSetArray<angle::EnumSize<PipelineState>()>;

[[maybe_unused]] void UnpackPipelineState(const GraphicsPipelineDesc &state,
                                          GraphicsPipelineSubset subset,
                                          UnpackedPipelineState *valuesOut)
{
    const bool hasVertexInput             = GraphicsPipelineHasVertexInput(subset);
    const bool hasShaders                 = GraphicsPipelineHasShaders(subset);
    const bool hasShadersOrFragmentOutput = GraphicsPipelineHasShadersOrFragmentOutput(subset);
    const bool hasFragmentOutput          = GraphicsPipelineHasFragmentOutput(subset);

    valuesOut->fill(0);

    if (hasVertexInput)
    {
        const PipelineVertexInputState &vertexInputState = state.getVertexInputStateForLog();

        const PackedVertexInputAttributes &vertex = vertexInputState.vertex;
        uint32_t *vaFormats    = &(*valuesOut)[PipelineState::VertexAttribFormat];
        uint32_t *vaDivisors   = &(*valuesOut)[PipelineState::VertexAttribDivisor];
        uint32_t *vaOffsets    = &(*valuesOut)[PipelineState::VertexAttribOffset];
        uint32_t *vaStrides    = &(*valuesOut)[PipelineState::VertexAttribStride];
        uint32_t *vaCompressed = &(*valuesOut)[PipelineState::VertexAttribCompressed];
        uint32_t *vaShaderComponentType =
            &(*valuesOut)[PipelineState::VertexAttribShaderComponentType];
        for (uint32_t attribIndex = 0; attribIndex < gl::MAX_VERTEX_ATTRIBS; ++attribIndex)
        {
            vaFormats[attribIndex]    = vertex.attribs[attribIndex].format;
            vaDivisors[attribIndex]   = vertex.attribs[attribIndex].divisor;
            vaOffsets[attribIndex]    = vertex.attribs[attribIndex].offset;
            vaStrides[attribIndex]    = vertex.strides[attribIndex];
            vaCompressed[attribIndex] = vertex.attribs[attribIndex].compressed;

            gl::ComponentType componentType = gl::GetComponentTypeMask(
                gl::ComponentTypeMask(vertex.shaderAttribComponentType), attribIndex);
            vaShaderComponentType[attribIndex] = componentType == gl::ComponentType::InvalidEnum
                                                     ? 0
                                                     : static_cast<uint32_t>(componentType);
        }

        const PackedInputAssemblyState &inputAssembly = vertexInputState.inputAssembly;
        (*valuesOut)[PipelineState::Topology]         = inputAssembly.bits.topology;
        (*valuesOut)[PipelineState::PrimitiveRestartEnable] =
            inputAssembly.bits.primitiveRestartEnable;
    }

    if (hasShaders)
    {
        const PipelineShadersState &shadersState = state.getShadersStateForLog();

        const PackedPreRasterizationAndFragmentStates &shaders = shadersState.shaders;
        (*valuesOut)[PipelineState::ViewportNegativeOneToOne] =
            shaders.bits.viewportNegativeOneToOne;
        (*valuesOut)[PipelineState::DepthClampEnable]        = shaders.bits.depthClampEnable;
        (*valuesOut)[PipelineState::CullMode]                = shaders.bits.cullMode;
        (*valuesOut)[PipelineState::FrontFace]               = shaders.bits.frontFace;
        (*valuesOut)[PipelineState::RasterizerDiscardEnable] = shaders.bits.rasterizerDiscardEnable;
        (*valuesOut)[PipelineState::DepthBiasEnable]         = shaders.bits.depthBiasEnable;
        (*valuesOut)[PipelineState::PatchVertices]           = shaders.bits.patchVertices;
        (*valuesOut)[PipelineState::DepthBoundsTest]         = shaders.bits.depthBoundsTest;
        (*valuesOut)[PipelineState::DepthTest]               = shaders.bits.depthTest;
        (*valuesOut)[PipelineState::DepthWrite]              = shaders.bits.depthWrite;
        (*valuesOut)[PipelineState::StencilTest]             = shaders.bits.stencilTest;
        (*valuesOut)[PipelineState::DepthCompareOp]          = shaders.bits.depthCompareOp;
        (*valuesOut)[PipelineState::SurfaceRotation]         = shaders.bits.surfaceRotation;
        (*valuesOut)[PipelineState::EmulatedDitherControl]   = shaders.emulatedDitherControl;
        (*valuesOut)[PipelineState::StencilOpFailFront]      = shaders.front.fail;
        (*valuesOut)[PipelineState::StencilOpPassFront]      = shaders.front.pass;
        (*valuesOut)[PipelineState::StencilOpDepthFailFront] = shaders.front.depthFail;
        (*valuesOut)[PipelineState::StencilCompareFront]     = shaders.front.compare;
        (*valuesOut)[PipelineState::StencilOpFailBack]       = shaders.back.fail;
        (*valuesOut)[PipelineState::StencilOpPassBack]       = shaders.back.pass;
        (*valuesOut)[PipelineState::StencilOpDepthFailBack]  = shaders.back.depthFail;
        (*valuesOut)[PipelineState::StencilCompareBack]      = shaders.back.compare;
    }

    if (hasShadersOrFragmentOutput)
    {
        const PipelineSharedNonVertexInputState &sharedNonVertexInputState =
            state.getSharedNonVertexInputStateForLog();

        const PackedMultisampleAndSubpassState &multisample = sharedNonVertexInputState.multisample;
        (*valuesOut)[PipelineState::SampleMask]             = multisample.bits.sampleMask;
        (*valuesOut)[PipelineState::RasterizationSamples] =
            multisample.bits.rasterizationSamplesMinusOne + 1;
        (*valuesOut)[PipelineState::SampleShadingEnable]   = multisample.bits.sampleShadingEnable;
        (*valuesOut)[PipelineState::AlphaToCoverageEnable] = multisample.bits.alphaToCoverageEnable;
        (*valuesOut)[PipelineState::AlphaToOneEnable]      = multisample.bits.alphaToOneEnable;
        (*valuesOut)[PipelineState::Subpass]               = multisample.bits.subpass;
        (*valuesOut)[PipelineState::MinSampleShading]      = multisample.bits.minSampleShading;

        const RenderPassDesc renderPass                = sharedNonVertexInputState.renderPass;
        (*valuesOut)[PipelineState::RenderPassSamples] = renderPass.samples();
        (*valuesOut)[PipelineState::RenderPassColorAttachmentRange] =
            static_cast<uint32_t>(renderPass.colorAttachmentRange());
        (*valuesOut)[PipelineState::RenderPassViewCount] = renderPass.viewCount();
        (*valuesOut)[PipelineState::RenderPassSrgbWriteControl] =
            static_cast<uint32_t>(renderPass.getSRGBWriteControlMode());
        (*valuesOut)[PipelineState::RenderPassHasFramebufferFetch] =
            renderPass.hasFramebufferFetch();
        (*valuesOut)[PipelineState::RenderPassIsRenderToTexture] = renderPass.isRenderToTexture();
        (*valuesOut)[PipelineState::RenderPassResolveDepthStencil] =
            renderPass.hasDepthStencilResolveAttachment();
        (*valuesOut)[PipelineState::RenderPassUnresolveDepth] =
            renderPass.hasDepthUnresolveAttachment();
        (*valuesOut)[PipelineState::RenderPassUnresolveStencil] =
            renderPass.hasStencilUnresolveAttachment();
        (*valuesOut)[PipelineState::RenderPassColorResolveMask] =
            renderPass.getColorResolveAttachmentMask().bits();
        (*valuesOut)[PipelineState::RenderPassColorUnresolveMask] =
            renderPass.getColorUnresolveAttachmentMask().bits();

        uint32_t *colorFormats = &(*valuesOut)[PipelineState::RenderPassColorFormat];
        for (uint32_t colorIndex = 0; colorIndex < renderPass.colorAttachmentRange(); ++colorIndex)
        {
            colorFormats[colorIndex] = static_cast<uint32_t>(renderPass[colorIndex]);
        }
        (*valuesOut)[PipelineState::RenderPassDepthStencilFormat] =
            static_cast<uint32_t>(renderPass[renderPass.depthStencilAttachmentIndex()]);
    }

    if (hasFragmentOutput)
    {
        const PipelineFragmentOutputState &fragmentOutputState =
            state.getFragmentOutputStateForLog();

        const PackedColorBlendState &blend = fragmentOutputState.blend;
        uint32_t *colorWriteMasks          = &(*valuesOut)[PipelineState::ColorWriteMask];
        uint32_t *srcColorBlendFactors     = &(*valuesOut)[PipelineState::SrcColorBlendFactor];
        uint32_t *dstColorBlendFactors     = &(*valuesOut)[PipelineState::DstColorBlendFactor];
        uint32_t *colorBlendOps            = &(*valuesOut)[PipelineState::ColorBlendOp];
        uint32_t *srcAlphaBlendFactors     = &(*valuesOut)[PipelineState::SrcAlphaBlendFactor];
        uint32_t *dstAlphaBlendFactors     = &(*valuesOut)[PipelineState::DstAlphaBlendFactor];
        uint32_t *alphaBlendOps            = &(*valuesOut)[PipelineState::AlphaBlendOp];
        for (uint32_t colorIndex = 0; colorIndex < gl::IMPLEMENTATION_MAX_DRAW_BUFFERS;
             ++colorIndex)
        {
            colorWriteMasks[colorIndex] =
                Int4Array_Get<VkColorComponentFlags>(blend.colorWriteMaskBits, colorIndex);

            srcColorBlendFactors[colorIndex] = blend.attachments[colorIndex].srcColorBlendFactor;
            dstColorBlendFactors[colorIndex] = blend.attachments[colorIndex].dstColorBlendFactor;
            colorBlendOps[colorIndex]        = blend.attachments[colorIndex].colorBlendOp;
            srcAlphaBlendFactors[colorIndex] = blend.attachments[colorIndex].srcAlphaBlendFactor;
            dstAlphaBlendFactors[colorIndex] = blend.attachments[colorIndex].dstAlphaBlendFactor;
            alphaBlendOps[colorIndex]        = blend.attachments[colorIndex].alphaBlendOp;
        }

        const PackedBlendMaskAndLogicOpState &blendMaskAndLogic =
            fragmentOutputState.blendMaskAndLogic;
        (*valuesOut)[PipelineState::BlendEnableMask]    = blendMaskAndLogic.bits.blendEnableMask;
        (*valuesOut)[PipelineState::LogicOpEnable]      = blendMaskAndLogic.bits.logicOpEnable;
        (*valuesOut)[PipelineState::LogicOp]            = blendMaskAndLogic.bits.logicOp;
        (*valuesOut)[PipelineState::MissingOutputsMask] = blendMaskAndLogic.bits.missingOutputsMask;
    }
}

[[maybe_unused]] PipelineStateBitSet GetCommonPipelineState(
    const std::vector<UnpackedPipelineState> &pipelines)
{
    PipelineStateBitSet commonState;
    commonState.set();

    ASSERT(!pipelines.empty());
    const UnpackedPipelineState &firstPipeline = pipelines[0];

    for (const UnpackedPipelineState &pipeline : pipelines)
    {
        for (size_t stateIndex = 0; stateIndex < firstPipeline.size(); ++stateIndex)
        {
            if (pipeline.data()[stateIndex] != firstPipeline.data()[stateIndex])
            {
                commonState.reset(stateIndex);
            }
        }
    }

    return commonState;
}

bool IsPipelineState(size_t stateIndex, PipelineState pipelineState, size_t range)
{
    size_t pipelineStateAsInt = static_cast<size_t>(pipelineState);

    return stateIndex >= pipelineStateAsInt && stateIndex < pipelineStateAsInt + range;
}

size_t GetPipelineStateSubIndex(size_t stateIndex, PipelineState pipelineState)
{
    return stateIndex - static_cast<size_t>(pipelineState);
}

PipelineState GetPipelineState(size_t stateIndex, bool *isRangedOut, size_t *subIndexOut)
{
    constexpr PipelineState kRangedStates[] = {
        PipelineState::VertexAttribFormat,     PipelineState::VertexAttribDivisor,
        PipelineState::VertexAttribOffset,     PipelineState::VertexAttribStride,
        PipelineState::VertexAttribCompressed, PipelineState::VertexAttribShaderComponentType,
        PipelineState::RenderPassColorFormat,  PipelineState::ColorWriteMask,
        PipelineState::SrcColorBlendFactor,    PipelineState::DstColorBlendFactor,
        PipelineState::ColorBlendOp,           PipelineState::SrcAlphaBlendFactor,
        PipelineState::DstAlphaBlendFactor,    PipelineState::AlphaBlendOp,
    };

    for (PipelineState ps : kRangedStates)
    {
        size_t range;
        switch (ps)
        {
            case PipelineState::VertexAttribFormat:
            case PipelineState::VertexAttribDivisor:
            case PipelineState::VertexAttribOffset:
            case PipelineState::VertexAttribStride:
            case PipelineState::VertexAttribCompressed:
            case PipelineState::VertexAttribShaderComponentType:
                range = gl::MAX_VERTEX_ATTRIBS;
                break;
            default:
                range = gl::IMPLEMENTATION_MAX_DRAW_BUFFERS;
                break;
        }

        if (IsPipelineState(stateIndex, ps, range))
        {
            *subIndexOut = GetPipelineStateSubIndex(stateIndex, ps);
            *isRangedOut = true;
            return ps;
        }
    }

    *isRangedOut = false;
    return static_cast<PipelineState>(stateIndex);
}

[[maybe_unused]] void OutputPipelineState(std::ostream &out, size_t stateIndex, uint32_t state)
{
    size_t subIndex             = 0;
    bool isRanged               = false;
    PipelineState pipelineState = GetPipelineState(stateIndex, &isRanged, &subIndex);

    constexpr angle::PackedEnumMap<PipelineState, const char *> kStateNames = {{
        {PipelineState::VertexAttribFormat, "va_format"},
        {PipelineState::VertexAttribDivisor, "va_divisor"},
        {PipelineState::VertexAttribOffset, "va_offset"},
        {PipelineState::VertexAttribStride, "va_stride"},
        {PipelineState::VertexAttribCompressed, "va_compressed"},
        {PipelineState::VertexAttribShaderComponentType, "va_shader_component_type"},
        {PipelineState::RenderPassSamples, "rp_samples"},
        {PipelineState::RenderPassColorAttachmentRange, "rp_color_range"},
        {PipelineState::RenderPassViewCount, "rp_views"},
        {PipelineState::RenderPassSrgbWriteControl, "rp_srgb"},
        {PipelineState::RenderPassHasFramebufferFetch, "rp_has_framebuffer_fetch"},
        {PipelineState::RenderPassIsRenderToTexture, "rp_is_msrtt"},
        {PipelineState::RenderPassResolveDepthStencil, "rp_resolve_depth_stencil"},
        {PipelineState::RenderPassUnresolveDepth, "rp_unresolve_depth"},
        {PipelineState::RenderPassUnresolveStencil, "rp_unresolve_stencil"},
        {PipelineState::RenderPassColorResolveMask, "rp_resolve_color"},
        {PipelineState::RenderPassColorUnresolveMask, "rp_unresolve_color"},
        {PipelineState::RenderPassColorFormat, "rp_color"},
        {PipelineState::RenderPassDepthStencilFormat, "rp_depth_stencil"},
        {PipelineState::Subpass, "subpass"},
        {PipelineState::Topology, "topology"},
        {PipelineState::PatchVertices, "patch_vertices"},
        {PipelineState::PrimitiveRestartEnable, "primitive_restart"},
        {PipelineState::CullMode, "cull_mode"},
        {PipelineState::FrontFace, "front_face"},
        {PipelineState::SurfaceRotation, "rotated_surface"},
        {PipelineState::ViewportNegativeOneToOne, "viewport_depth_[-1,1]"},
        {PipelineState::SampleShadingEnable, "sample_shading"},
        {PipelineState::RasterizationSamples, "rasterization_samples"},
        {PipelineState::MinSampleShading, "min_sample_shading"},
        {PipelineState::SampleMask, "sample_mask"},
        {PipelineState::AlphaToCoverageEnable, "alpha_to_coverage"},
        {PipelineState::AlphaToOneEnable, "alpha_to_one"},
        {PipelineState::LogicOpEnable, "logic_op_enable"},
        {PipelineState::LogicOp, "logic_op"},
        {PipelineState::RasterizerDiscardEnable, "rasterization_discard"},
        {PipelineState::ColorWriteMask, "color_write"},
        {PipelineState::BlendEnableMask, "blend_mask"},
        {PipelineState::MissingOutputsMask, "missing_outputs_mask"},
        {PipelineState::SrcColorBlendFactor, "src_color_blend"},
        {PipelineState::DstColorBlendFactor, "dst_color_blend"},
        {PipelineState::ColorBlendOp, "color_blend"},
        {PipelineState::SrcAlphaBlendFactor, "src_alpha_blend"},
        {PipelineState::DstAlphaBlendFactor, "dst_alpha_blend"},
        {PipelineState::AlphaBlendOp, "alpha_blend"},
        {PipelineState::EmulatedDitherControl, "dither"},
        {PipelineState::DepthClampEnable, "depth_clamp"},
        {PipelineState::DepthBoundsTest, "depth_bounds_test"},
        {PipelineState::DepthCompareOp, "depth_compare"},
        {PipelineState::DepthTest, "depth_test"},
        {PipelineState::DepthWrite, "depth_write"},
        {PipelineState::StencilTest, "stencil_test"},
        {PipelineState::DepthBiasEnable, "depth_bias"},
        {PipelineState::StencilOpFailFront, "stencil_front_fail"},
        {PipelineState::StencilOpPassFront, "stencil_front_pass"},
        {PipelineState::StencilOpDepthFailFront, "stencil_front_depth_fail"},
        {PipelineState::StencilCompareFront, "stencil_front_compare"},
        {PipelineState::StencilOpFailBack, "stencil_back_fail"},
        {PipelineState::StencilOpPassBack, "stencil_back_pass"},
        {PipelineState::StencilOpDepthFailBack, "stencil_back_depth_fail"},
        {PipelineState::StencilCompareBack, "stencil_back_compare"},
    }};

    out << kStateNames[pipelineState];
    if (isRanged)
    {
        out << "_" << subIndex;
    }

    switch (pipelineState)
    {
        // Given that state == 0 produces no output, binary state doesn't require anything but
        // its name specified, as it being enabled would be implied.
        case PipelineState::VertexAttribCompressed:
        case PipelineState::RenderPassSrgbWriteControl:
        case PipelineState::RenderPassHasFramebufferFetch:
        case PipelineState::RenderPassIsRenderToTexture:
        case PipelineState::RenderPassResolveDepthStencil:
        case PipelineState::RenderPassUnresolveDepth:
        case PipelineState::RenderPassUnresolveStencil:
        case PipelineState::PrimitiveRestartEnable:
        case PipelineState::SurfaceRotation:
        case PipelineState::ViewportNegativeOneToOne:
        case PipelineState::SampleShadingEnable:
        case PipelineState::AlphaToCoverageEnable:
        case PipelineState::AlphaToOneEnable:
        case PipelineState::LogicOpEnable:
        case PipelineState::RasterizerDiscardEnable:
        case PipelineState::DepthClampEnable:
        case PipelineState::DepthBoundsTest:
        case PipelineState::DepthTest:
        case PipelineState::DepthWrite:
        case PipelineState::StencilTest:
        case PipelineState::DepthBiasEnable:
            break;

        // Special formatting for some state
        case PipelineState::VertexAttribShaderComponentType:
            out << "=";
            switch (state)
            {
                case 0:
                    static_assert(static_cast<uint32_t>(gl::ComponentType::Float) == 0);
                    out << "float";
                    break;
                case 1:
                    static_assert(static_cast<uint32_t>(gl::ComponentType::Int) == 1);
                    out << "int";
                    break;
                case 2:
                    static_assert(static_cast<uint32_t>(gl::ComponentType::UnsignedInt) == 2);
                    out << "uint";
                    break;
                case 3:
                    static_assert(static_cast<uint32_t>(gl::ComponentType::NoType) == 3);
                    out << "none";
                    break;
                default:
                    UNREACHABLE();
            }
            break;
        case PipelineState::Topology:
            out << "=";
            switch (state)
            {
                case VK_PRIMITIVE_TOPOLOGY_POINT_LIST:
                    out << "points";
                    break;
                case VK_PRIMITIVE_TOPOLOGY_LINE_LIST:
                    out << "lines";
                    break;
                case VK_PRIMITIVE_TOPOLOGY_LINE_STRIP:
                    out << "line_strip";
                    break;
                case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST:
                    out << "tris";
                    break;
                case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP:
                    out << "tri_strip";
                    break;
                case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN:
                    out << "tri_fan";
                    break;
                case VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY:
                    out << "lines_with_adj";
                    break;
                case VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY:
                    out << "line_strip_with_adj";
                    break;
                case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY:
                    out << "tris_with_adj";
                    break;
                case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY:
                    out << "tri_strip_with_adj";
                    break;
                case VK_PRIMITIVE_TOPOLOGY_PATCH_LIST:
                    out << "patches";
                    break;
                default:
                    UNREACHABLE();
            }
            break;
        case PipelineState::CullMode:
            out << "=";
            if ((state & VK_CULL_MODE_FRONT_BIT) != 0)
            {
                out << "front";
            }
            if (state == VK_CULL_MODE_FRONT_AND_BACK)
            {
                out << "+";
            }
            if ((state & VK_CULL_MODE_BACK_BIT) != 0)
            {
                out << "back";
            }
            break;
        case PipelineState::FrontFace:
            out << "=" << (state == VK_FRONT_FACE_COUNTER_CLOCKWISE ? "ccw" : "cw");
            break;
        case PipelineState::MinSampleShading:
            out << "=" << (static_cast<float>(state) / kMinSampleShadingScale);
            break;
        case PipelineState::SrcColorBlendFactor:
        case PipelineState::DstColorBlendFactor:
        case PipelineState::SrcAlphaBlendFactor:
        case PipelineState::DstAlphaBlendFactor:
            out << "=";
            switch (state)
            {
                case VK_BLEND_FACTOR_ZERO:
                    out << "0";
                    break;
                case VK_BLEND_FACTOR_ONE:
                    out << "1";
                    break;
                case VK_BLEND_FACTOR_SRC_COLOR:
                    out << "sc";
                    break;
                case VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR:
                    out << "1-sc";
                    break;
                case VK_BLEND_FACTOR_DST_COLOR:
                    out << "dc";
                    break;
                case VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR:
                    out << "1-dc";
                    break;
                case VK_BLEND_FACTOR_SRC_ALPHA:
                    out << "sa";
                    break;
                case VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA:
                    out << "1-sa";
                    break;
                case VK_BLEND_FACTOR_DST_ALPHA:
                    out << "da";
                    break;
                case VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA:
                    out << "1-da";
                    break;
                case VK_BLEND_FACTOR_CONSTANT_COLOR:
                    out << "const_color";
                    break;
                case VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR:
                    out << "1-const_color";
                    break;
                case VK_BLEND_FACTOR_CONSTANT_ALPHA:
                    out << "const_alpha";
                    break;
                case VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA:
                    out << "1-const_alpha";
                    break;
                case VK_BLEND_FACTOR_SRC_ALPHA_SATURATE:
                    out << "sat(sa)";
                    break;
                case VK_BLEND_FACTOR_SRC1_COLOR:
                    out << "sc1";
                    break;
                case VK_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR:
                    out << "1-sc1";
                    break;
                case VK_BLEND_FACTOR_SRC1_ALPHA:
                    out << "sa1";
                    break;
                case VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA:
                    out << "1-sa1";
                    break;
                default:
                    UNREACHABLE();
            }
            break;
        case PipelineState::ColorBlendOp:
        case PipelineState::AlphaBlendOp:
            out << "=";
            switch (UnpackBlendOp(static_cast<uint8_t>(state)))
            {
                case VK_BLEND_OP_ADD:
                    out << "add";
                    break;
                case VK_BLEND_OP_SUBTRACT:
                    out << "sub";
                    break;
                case VK_BLEND_OP_REVERSE_SUBTRACT:
                    out << "reverse_sub";
                    break;
                case VK_BLEND_OP_MIN:
                    out << "min";
                    break;
                case VK_BLEND_OP_MAX:
                    out << "max";
                    break;
                case VK_BLEND_OP_MULTIPLY_EXT:
                    out << "multiply";
                    break;
                case VK_BLEND_OP_SCREEN_EXT:
                    out << "screen";
                    break;
                case VK_BLEND_OP_OVERLAY_EXT:
                    out << "overlay";
                    break;
                case VK_BLEND_OP_DARKEN_EXT:
                    out << "darken";
                    break;
                case VK_BLEND_OP_LIGHTEN_EXT:
                    out << "lighten";
                    break;
                case VK_BLEND_OP_COLORDODGE_EXT:
                    out << "dodge";
                    break;
                case VK_BLEND_OP_COLORBURN_EXT:
                    out << "burn";
                    break;
                case VK_BLEND_OP_HARDLIGHT_EXT:
                    out << "hardlight";
                    break;
                case VK_BLEND_OP_SOFTLIGHT_EXT:
                    out << "softlight";
                    break;
                case VK_BLEND_OP_DIFFERENCE_EXT:
                    out << "difference";
                    break;
                case VK_BLEND_OP_EXCLUSION_EXT:
                    out << "exclusion";
                    break;
                case VK_BLEND_OP_HSL_HUE_EXT:
                    out << "hsl_hue";
                    break;
                case VK_BLEND_OP_HSL_SATURATION_EXT:
                    out << "hsl_sat";
                    break;
                case VK_BLEND_OP_HSL_COLOR_EXT:
                    out << "hsl_color";
                    break;
                case VK_BLEND_OP_HSL_LUMINOSITY_EXT:
                    out << "hsl_lum";
                    break;
                default:
                    UNREACHABLE();
            }
            break;
        case PipelineState::DepthCompareOp:
        case PipelineState::StencilCompareFront:
        case PipelineState::StencilCompareBack:
            out << "=";
            switch (state)
            {
                case VK_COMPARE_OP_NEVER:
                    out << "never";
                    break;
                case VK_COMPARE_OP_LESS:
                    out << "'<'";
                    break;
                case VK_COMPARE_OP_EQUAL:
                    out << "'='";
                    break;
                case VK_COMPARE_OP_LESS_OR_EQUAL:
                    out << "'<='";
                    break;
                case VK_COMPARE_OP_GREATER:
                    out << "'>'";
                    break;
                case VK_COMPARE_OP_NOT_EQUAL:
                    out << "'!='";
                    break;
                case VK_COMPARE_OP_GREATER_OR_EQUAL:
                    out << "'>='";
                    break;
                case VK_COMPARE_OP_ALWAYS:
                    out << "always";
                    break;
                default:
                    UNREACHABLE();
            }
            break;
        case PipelineState::StencilOpFailFront:
        case PipelineState::StencilOpPassFront:
        case PipelineState::StencilOpDepthFailFront:
        case PipelineState::StencilOpFailBack:
        case PipelineState::StencilOpPassBack:
        case PipelineState::StencilOpDepthFailBack:
            out << "=";
            switch (state)
            {
                case VK_STENCIL_OP_KEEP:
                    out << "keep";
                    break;
                case VK_STENCIL_OP_ZERO:
                    out << "0";
                    break;
                case VK_STENCIL_OP_REPLACE:
                    out << "replace";
                    break;
                case VK_STENCIL_OP_INCREMENT_AND_CLAMP:
                    out << "clamp++";
                    break;
                case VK_STENCIL_OP_DECREMENT_AND_CLAMP:
                    out << "clamp--";
                    break;
                case VK_STENCIL_OP_INVERT:
                    out << "'~'";
                    break;
                case VK_STENCIL_OP_INCREMENT_AND_WRAP:
                    out << "wrap++";
                    break;
                case VK_STENCIL_OP_DECREMENT_AND_WRAP:
                    out << "wrap--";
                    break;
                default:
                    UNREACHABLE();
            }
            break;

        // Some state output the value as hex because they are bitmasks
        case PipelineState::RenderPassColorResolveMask:
        case PipelineState::RenderPassColorUnresolveMask:
        case PipelineState::SampleMask:
        case PipelineState::ColorWriteMask:
        case PipelineState::BlendEnableMask:
        case PipelineState::MissingOutputsMask:
        case PipelineState::EmulatedDitherControl:
            out << "=0x" << std::hex << state << std::dec;
            break;

        // The rest will simply output the state value
        default:
            out << "=" << state;
            break;
    }

    out << "\\n";
}

[[maybe_unused]] void OutputAllPipelineState(ContextVk *contextVk,
                                             std::ostream &out,
                                             const UnpackedPipelineState &pipeline,
                                             GraphicsPipelineSubset subset,
                                             const PipelineStateBitSet &include,
                                             bool isCommonState)
{
    // Default non-existing state to 0, so they are automatically not output as
    // UnpackedPipelineState also sets them to 0.
    const bool hasVertexInput             = GraphicsPipelineHasVertexInput(subset);
    const bool hasShaders                 = GraphicsPipelineHasShaders(subset);
    const bool hasShadersOrFragmentOutput = GraphicsPipelineHasShadersOrFragmentOutput(subset);
    const bool hasFragmentOutput          = GraphicsPipelineHasFragmentOutput(subset);

    const angle::PackedEnumMap<PipelineState, uint32_t> kDefaultState = {{
        // Vertex input state
        {PipelineState::VertexAttribFormat,
         hasVertexInput
             ? static_cast<uint32_t>(GetCurrentValueFormatID(gl::VertexAttribType::Float))
             : 0},
        {PipelineState::VertexAttribDivisor, 0},
        {PipelineState::VertexAttribOffset, 0},
        {PipelineState::VertexAttribStride, 0},
        {PipelineState::VertexAttribCompressed, 0},
        {PipelineState::VertexAttribShaderComponentType, 0},
        {PipelineState::Topology, hasVertexInput ? VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST : 0},
        {PipelineState::PrimitiveRestartEnable, 0},

        // Shaders state
        {PipelineState::ViewportNegativeOneToOne,
         hasShaders && contextVk->getFeatures().supportsDepthClipControl.enabled},
        {PipelineState::DepthClampEnable,
         hasShaders && contextVk->getFeatures().depthClamping.enabled},
        {PipelineState::CullMode, hasShaders ? VK_CULL_MODE_NONE : 0},
        {PipelineState::FrontFace, hasShaders ? VK_FRONT_FACE_COUNTER_CLOCKWISE : 0},
        {PipelineState::RasterizerDiscardEnable, 0},
        {PipelineState::DepthBiasEnable, 0},
        {PipelineState::PatchVertices, hasShaders ? 3 : 0},
        {PipelineState::DepthBoundsTest, 0},
        {PipelineState::DepthTest, 0},
        {PipelineState::DepthWrite, 0},
        {PipelineState::StencilTest, 0},
        {PipelineState::DepthCompareOp, hasShaders ? VK_COMPARE_OP_LESS : 0},
        {PipelineState::SurfaceRotation, 0},
        {PipelineState::EmulatedDitherControl, 0},
        {PipelineState::StencilOpFailFront, hasShaders ? VK_STENCIL_OP_KEEP : 0},
        {PipelineState::StencilOpPassFront, hasShaders ? VK_STENCIL_OP_KEEP : 0},
        {PipelineState::StencilOpDepthFailFront, hasShaders ? VK_STENCIL_OP_KEEP : 0},
        {PipelineState::StencilCompareFront, hasShaders ? VK_COMPARE_OP_ALWAYS : 0},
        {PipelineState::StencilOpFailBack, hasShaders ? VK_STENCIL_OP_KEEP : 0},
        {PipelineState::StencilOpPassBack, hasShaders ? VK_STENCIL_OP_KEEP : 0},
        {PipelineState::StencilOpDepthFailBack, hasShaders ? VK_STENCIL_OP_KEEP : 0},
        {PipelineState::StencilCompareBack, hasShaders ? VK_COMPARE_OP_ALWAYS : 0},

        // Shared shaders and fragment output state
        {PipelineState::SampleMask,
         hasShadersOrFragmentOutput ? std::numeric_limits<uint16_t>::max() : 0},
        {PipelineState::RasterizationSamples, hasShadersOrFragmentOutput ? 1 : 0},
        {PipelineState::SampleShadingEnable, 0},
        {PipelineState::MinSampleShading, hasShadersOrFragmentOutput ? kMinSampleShadingScale : 0},
        {PipelineState::AlphaToCoverageEnable, 0},
        {PipelineState::AlphaToOneEnable, 0},
        {PipelineState::RenderPassSamples, hasShadersOrFragmentOutput ? 1 : 0},
        {PipelineState::RenderPassColorAttachmentRange, 0},
        {PipelineState::RenderPassViewCount, 0},
        {PipelineState::RenderPassSrgbWriteControl, 0},
        {PipelineState::RenderPassHasFramebufferFetch, 0},
        {PipelineState::RenderPassIsRenderToTexture, 0},
        {PipelineState::RenderPassResolveDepthStencil, 0},
        {PipelineState::RenderPassUnresolveDepth, 0},
        {PipelineState::RenderPassUnresolveStencil, 0},
        {PipelineState::RenderPassColorResolveMask, 0},
        {PipelineState::RenderPassColorUnresolveMask, 0},
        {PipelineState::RenderPassColorFormat, 0},
        {PipelineState::RenderPassDepthStencilFormat, 0},
        {PipelineState::Subpass, 0},

        // Fragment output state
        {PipelineState::ColorWriteMask, 0},
        {PipelineState::SrcColorBlendFactor, hasFragmentOutput ? VK_BLEND_FACTOR_ONE : 0},
        {PipelineState::DstColorBlendFactor, hasFragmentOutput ? VK_BLEND_FACTOR_ZERO : 0},
        {PipelineState::ColorBlendOp, hasFragmentOutput ? VK_BLEND_OP_ADD : 0},
        {PipelineState::SrcAlphaBlendFactor, hasFragmentOutput ? VK_BLEND_FACTOR_ONE : 0},
        {PipelineState::DstAlphaBlendFactor, hasFragmentOutput ? VK_BLEND_FACTOR_ZERO : 0},
        {PipelineState::AlphaBlendOp, hasFragmentOutput ? VK_BLEND_OP_ADD : 0},
        {PipelineState::BlendEnableMask, 0},
        {PipelineState::LogicOpEnable, 0},
        {PipelineState::LogicOp, hasFragmentOutput ? VK_LOGIC_OP_COPY : 0},
        {PipelineState::MissingOutputsMask, 0},
    }};

    bool anyStateOutput = false;
    for (size_t stateIndex : include)
    {
        size_t subIndex             = 0;
        bool isRanged               = false;
        PipelineState pipelineState = GetPipelineState(stateIndex, &isRanged, &subIndex);

        const uint32_t state = pipeline.data()[stateIndex];
        if (state != kDefaultState[pipelineState])
        {
            OutputPipelineState(out, stateIndex, state);
            anyStateOutput = true;
        }
    }

    if (!isCommonState)
    {
        out << "(" << (anyStateOutput ? "+" : "") << "common state)\\n";
    }
}

template <typename Hash>
void DumpPipelineCacheGraph(
    ContextVk *contextVk,
    const std::unordered_map<GraphicsPipelineDesc,
                             PipelineHelper,
                             Hash,
                             typename GraphicsPipelineCacheTypeHelper<Hash>::KeyEqual> &cache)
{
    constexpr GraphicsPipelineSubset kSubset = GraphicsPipelineCacheTypeHelper<Hash>::kSubset;

    std::ostream &out = contextVk->getPipelineCacheGraphStream();

    static std::atomic<uint32_t> sCacheSerial(0);
    angle::HashMap<GraphicsPipelineDesc, uint32_t, Hash,
                   typename GraphicsPipelineCacheTypeHelper<Hash>::KeyEqual>
        descToId;

    uint32_t cacheSerial = sCacheSerial.fetch_add(1);
    uint32_t descId      = 0;

    // Unpack pipeline states
    std::vector<UnpackedPipelineState> pipelines(cache.size());
    for (const auto &descAndPipeline : cache)
    {
        UnpackPipelineState(descAndPipeline.first, kSubset, &pipelines[descId++]);
    }

    // Extract common state between all pipelines.
    PipelineStateBitSet commonState = GetCommonPipelineState(pipelines);
    PipelineStateBitSet nodeState   = ~commonState;

    const char *subsetDescription = "";
    const char *subsetTag         = "";
    switch (kSubset)
    {
        case GraphicsPipelineSubset::VertexInput:
            subsetDescription = "(vertex input)\\n";
            subsetTag         = "VI_";
            break;
        case GraphicsPipelineSubset::Shaders:
            subsetDescription = "(shaders)\\n";
            subsetTag         = "S_";
            break;
        case GraphicsPipelineSubset::FragmentOutput:
            subsetDescription = "(fragment output)\\n";
            subsetTag         = "FO_";
            break;
        default:
            break;
    }

    out << " subgraph cluster_" << subsetTag << cacheSerial << "{\n";
    out << "  label=\"Program " << cacheSerial << "\\n"
        << subsetDescription << "\\nCommon state:\\n";
    OutputAllPipelineState(contextVk, out, pipelines[0], kSubset, commonState, true);
    out << "\";\n";

    descId = 0;
    for (const auto &descAndPipeline : cache)
    {
        const GraphicsPipelineDesc &desc = descAndPipeline.first;

        const char *style        = "";
        const char *feedbackDesc = "";
        switch (descAndPipeline.second.getCacheLookUpFeedback())
        {
            case CacheLookUpFeedback::Hit:
                // Default is green already
                break;
            case CacheLookUpFeedback::Miss:
                style = "[color=red]";
                break;
            case CacheLookUpFeedback::LinkedDrawHit:
                // Default is green already
                style        = "[style=dotted]";
                feedbackDesc = "(linked)\\n";
                break;
            case CacheLookUpFeedback::LinkedDrawMiss:
                style        = "[style=dotted,color=red]";
                feedbackDesc = "(linked)\\n";
                break;
            case CacheLookUpFeedback::WarmUpHit:
                // Default is green already
                style        = "[style=dashed]";
                feedbackDesc = "(warm up)\\n";
                break;
            case CacheLookUpFeedback::WarmUpMiss:
                style        = "[style=dashed,color=red]";
                feedbackDesc = "(warm up)\\n";
                break;
            case CacheLookUpFeedback::UtilsHit:
                style        = "[color=yellow]";
                feedbackDesc = "(utils)\\n";
                break;
            case CacheLookUpFeedback::UtilsMiss:
                style        = "[color=purple]";
                feedbackDesc = "(utils)\\n";
                break;
            default:
                // No feedback available
                break;
        }

        out << "  p" << subsetTag << cacheSerial << "_" << descId << "[label=\"Pipeline " << descId
            << "\\n"
            << feedbackDesc << "\\n";
        OutputAllPipelineState(contextVk, out, pipelines[descId], kSubset, nodeState, false);
        out << "\"]" << style << ";\n";

        descToId[desc] = descId++;
    }
    for (const auto &descAndPipeline : cache)
    {
        const GraphicsPipelineDesc &desc     = descAndPipeline.first;
        const PipelineHelper &pipelineHelper = descAndPipeline.second;
        const std::vector<GraphicsPipelineTransition> &transitions =
            pipelineHelper.getTransitions();

        for (const GraphicsPipelineTransition &transition : transitions)
        {
#if defined(ANGLE_IS_64_BIT_CPU)
            const uint64_t transitionBits = transition.bits.bits();
#else
            const uint64_t transitionBits =
                static_cast<uint64_t>(transition.bits.bits(1)) << 32 | transition.bits.bits(0);
#endif
            out << "  p" << subsetTag << cacheSerial << "_" << descToId[desc] << " -> p"
                << subsetTag << cacheSerial << "_" << descToId[*transition.desc] << " [label=\"'0x"
                << std::hex << transitionBits << std::dec << "'\"];\n";
        }
    }
    out << " }\n";
}

// Used by SharedCacheKeyManager
void ReleaseCachedObject(ContextVk *contextVk, const FramebufferDesc &desc)
{
    contextVk->getShareGroup()->getFramebufferCache().erase(contextVk, desc);
}

void ReleaseCachedObject(ContextVk *contextVk, const DescriptorSetDescAndPool &descAndPool)
{
    ASSERT(descAndPool.mPool != nullptr);
    descAndPool.mPool->releaseCachedDescriptorSet(contextVk, descAndPool.mDesc);
}

void DestroyCachedObject(RendererVk *renderer, const FramebufferDesc &desc)
{
    // Framebuffer cache are implemented in a way that each cache entry tracks GPU progress and we
    // always guarantee cache entries are released before calling destroy.
}

void DestroyCachedObject(RendererVk *renderer, const DescriptorSetDescAndPool &descAndPool)
{
    ASSERT(descAndPool.mPool != nullptr);
    descAndPool.mPool->destroyCachedDescriptorSet(renderer, descAndPool.mDesc);
}

angle::Result InitializePipelineFromLibraries(Context *context,
                                              PipelineCacheAccess *pipelineCache,
                                              const vk::PipelineLayout &pipelineLayout,
                                              const vk::PipelineHelper &vertexInputPipeline,
                                              const vk::PipelineHelper &shadersPipeline,
                                              const vk::PipelineHelper &fragmentOutputPipeline,
                                              Pipeline *pipelineOut,
                                              CacheLookUpFeedback *feedbackOut)
{
    // Nothing in the create info, everything comes from the libraries.
    VkGraphicsPipelineCreateInfo createInfo = {};
    createInfo.sType                        = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;

    // Note: the pipeline layout is not necessary when linking libraries as ANGLE does not use
    // VK_PIPELINE_LAYOUT_CREATE_INDEPENDENT_SETS_BIT_EXT.  However, some drivers have come to
    // expect a non-null value for the layout, so the layout is provided here as a workaround.
    createInfo.layout = pipelineLayout.getHandle();

    const std::array<VkPipeline, 3> pipelines = {
        vertexInputPipeline.getPipeline().getHandle(),
        shadersPipeline.getPipeline().getHandle(),
        fragmentOutputPipeline.getPipeline().getHandle(),
    };

    // Specify the three subsets as input libraries.
    VkPipelineLibraryCreateInfoKHR libraryInfo = {};
    libraryInfo.sType                          = VK_STRUCTURE_TYPE_PIPELINE_LIBRARY_CREATE_INFO_KHR;
    libraryInfo.libraryCount                   = 3;
    libraryInfo.pLibraries                     = pipelines.data();

    AddToPNextChain(&createInfo, &libraryInfo);

    // If supported, get feedback.
    VkPipelineCreationFeedback feedback               = {};
    VkPipelineCreationFeedbackCreateInfo feedbackInfo = {};
    feedbackInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CREATION_FEEDBACK_CREATE_INFO;

    const bool supportsFeedback = context->getFeatures().supportsPipelineCreationFeedback.enabled;
    if (supportsFeedback)
    {
        feedbackInfo.pPipelineCreationFeedback = &feedback;
        AddToPNextChain(&createInfo, &feedbackInfo);
    }

    // Create the pipeline
    ANGLE_VK_TRY(context, pipelineCache->createGraphicsPipeline(context, createInfo, pipelineOut));

    if (supportsFeedback)
    {
        const bool cacheHit =
            (feedback.flags & VK_PIPELINE_CREATION_FEEDBACK_APPLICATION_PIPELINE_CACHE_HIT_BIT) !=
            0;

        *feedbackOut = cacheHit ? CacheLookUpFeedback::Hit : CacheLookUpFeedback::Miss;
        ApplyPipelineCreationFeedback(context, feedback);
    }

    return angle::Result::Continue;
}
}  // anonymous namespace

GraphicsPipelineTransitionBits GetGraphicsPipelineTransitionBitsMask(GraphicsPipelineSubset subset)
{
    switch (subset)
    {
        case GraphicsPipelineSubset::VertexInput:
            return kPipelineVertexInputTransitionBitsMask;
        case GraphicsPipelineSubset::Shaders:
            return kPipelineShadersTransitionBitsMask;
        case GraphicsPipelineSubset::FragmentOutput:
            return kPipelineFragmentOutputTransitionBitsMask;
        default:
            break;
    }

    ASSERT(subset == GraphicsPipelineSubset::Complete);

    GraphicsPipelineTransitionBits allBits;
    allBits.set();

    return allBits;
}

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

void RenderPassDesc::packColorAttachment(size_t colorIndexGL, angle::FormatID formatID)
{
    ASSERT(colorIndexGL < mAttachmentFormats.size());
    static_assert(angle::kNumANGLEFormats < std::numeric_limits<uint8_t>::max(),
                  "Too many ANGLE formats to fit in uint8_t");
    // Force the user to pack the depth/stencil attachment last.
    ASSERT(!hasDepthStencilAttachment());
    // This function should only be called for enabled GL color attachments.
    ASSERT(formatID != angle::FormatID::NONE);

    uint8_t &packedFormat = mAttachmentFormats[colorIndexGL];
    SetBitField(packedFormat, formatID);

    // Set color attachment range such that it covers the range from index 0 through last active
    // index.  This is the reasons why we need depth/stencil to be packed last.
    SetBitField(mColorAttachmentRange, std::max<size_t>(mColorAttachmentRange, colorIndexGL + 1));
}

void RenderPassDesc::packColorAttachmentGap(size_t colorIndexGL)
{
    ASSERT(colorIndexGL < mAttachmentFormats.size());
    static_assert(angle::kNumANGLEFormats < std::numeric_limits<uint8_t>::max(),
                  "Too many ANGLE formats to fit in uint8_t");
    // Force the user to pack the depth/stencil attachment last.
    ASSERT(!hasDepthStencilAttachment());

    // Use NONE as a flag for gaps in GL color attachments.
    uint8_t &packedFormat = mAttachmentFormats[colorIndexGL];
    SetBitField(packedFormat, angle::FormatID::NONE);
}

void RenderPassDesc::packDepthStencilAttachment(angle::FormatID formatID)
{
    ASSERT(!hasDepthStencilAttachment());

    size_t index = depthStencilAttachmentIndex();
    ASSERT(index < mAttachmentFormats.size());

    uint8_t &packedFormat = mAttachmentFormats[index];
    SetBitField(packedFormat, formatID);
}

void RenderPassDesc::packColorResolveAttachment(size_t colorIndexGL)
{
    ASSERT(isColorAttachmentEnabled(colorIndexGL));
    ASSERT(!mColorResolveAttachmentMask.test(colorIndexGL));
    ASSERT(mSamples > 1);
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

void RenderPassDesc::packDepthStencilResolveAttachment()
{
    ASSERT(hasDepthStencilAttachment());
    ASSERT(!hasDepthStencilResolveAttachment());

    mResolveDepthStencil = true;
}

void RenderPassDesc::packDepthStencilUnresolveAttachment(bool unresolveDepth, bool unresolveStencil)
{
    ASSERT(hasDepthStencilAttachment());

    mUnresolveDepth   = unresolveDepth;
    mUnresolveStencil = unresolveStencil;
}

void RenderPassDesc::removeDepthStencilUnresolveAttachment()
{
    mUnresolveDepth   = false;
    mUnresolveStencil = false;
}

RenderPassDesc &RenderPassDesc::operator=(const RenderPassDesc &other)
{
    memcpy(this, &other, sizeof(RenderPassDesc));
    return *this;
}

void RenderPassDesc::setWriteControlMode(gl::SrgbWriteControlMode mode)
{
    SetBitField(mSrgbWriteControl, mode);
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

bool RenderPassDesc::hasDepthStencilAttachment() const
{
    angle::FormatID formatID = operator[](depthStencilAttachmentIndex());
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
    return memcmp(&lhs, &rhs, sizeof(RenderPassDesc)) == 0;
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
    *this = other;
}

GraphicsPipelineDesc &GraphicsPipelineDesc::operator=(const GraphicsPipelineDesc &other)
{
    memcpy(this, &other, sizeof(*this));
    return *this;
}

const void *GraphicsPipelineDesc::getPipelineSubsetMemory(GraphicsPipelineSubset subset,
                                                          size_t *sizeOut) const
{
    // GraphicsPipelineDesc must be laid out such that the three subsets are contiguous.  The layout
    // is:
    //
    //     Shaders State                 \
    //                                    )--> Pre-rasterization + fragment subset
    //     Shared Non-Vertex-Input State /  \
    //                                       )--> fragment output subset
    //     Fragment Output State            /
    //
    //     Vertex Input State            ----> Vertex input subset
    static_assert(offsetof(GraphicsPipelineDesc, mShaders) == kPipelineShadersDescOffset);
    static_assert(offsetof(GraphicsPipelineDesc, mSharedNonVertexInput) ==
                  kPipelineShadersDescOffset + kGraphicsPipelineShadersStateSize);
    static_assert(offsetof(GraphicsPipelineDesc, mSharedNonVertexInput) ==
                  kPipelineFragmentOutputDescOffset);
    static_assert(offsetof(GraphicsPipelineDesc, mFragmentOutput) ==
                  kPipelineFragmentOutputDescOffset +
                      kGraphicsPipelineSharedNonVertexInputStateSize);
    static_assert(offsetof(GraphicsPipelineDesc, mVertexInput) == kPipelineVertexInputDescOffset);

    // Exclude vertex strides from the hash. It's conveniently placed last, so it would be easy to
    // exclude it from hash.
    static_assert(offsetof(GraphicsPipelineDesc, mVertexInput.vertex.strides) +
                      sizeof(PackedVertexInputAttributes::strides) ==
                  sizeof(GraphicsPipelineDesc));

    size_t vertexInputReduceSize = 0;
    if (mVertexInput.inputAssembly.bits.supportsDynamicState1 &&
        !mVertexInput.inputAssembly.bits.forceStaticVertexStrideState)
    {
        vertexInputReduceSize = sizeof(PackedVertexInputAttributes::strides);
    }

    switch (subset)
    {
        case GraphicsPipelineSubset::VertexInput:
            *sizeOut = kPipelineVertexInputDescSize - vertexInputReduceSize;
            return &mVertexInput;

        case GraphicsPipelineSubset::Shaders:
            *sizeOut = kPipelineShadersDescSize;
            return &mShaders;

        case GraphicsPipelineSubset::FragmentOutput:
            *sizeOut = kPipelineFragmentOutputDescSize;
            return &mSharedNonVertexInput;

        case GraphicsPipelineSubset::Complete:
        default:
            *sizeOut = sizeof(*this) - vertexInputReduceSize;
            return this;
    }
}

size_t GraphicsPipelineDesc::hash(GraphicsPipelineSubset subset) const
{
    size_t keySize  = 0;
    const void *key = getPipelineSubsetMemory(subset, &keySize);

    return angle::ComputeGenericHash(key, keySize);
}

bool GraphicsPipelineDesc::keyEqual(const GraphicsPipelineDesc &other,
                                    GraphicsPipelineSubset subset) const
{
    size_t keySize  = 0;
    const void *key = getPipelineSubsetMemory(subset, &keySize);

    size_t otherKeySize  = 0;
    const void *otherKey = other.getPipelineSubsetMemory(subset, &otherKeySize);

    // Compare the relevant part of the desc memory.  Note that due to workarounds (e.g.
    // forceStaticVertexStrideState), |this| or |other| may produce different key sizes.  In that
    // case, comparing the minimum of the two is sufficient; if the workarounds are different, the
    // comparison would fail anyway.
    return memcmp(key, otherKey, std::min(keySize, otherKeySize)) == 0;
}

// Initialize PSO states, it is consistent with initial value of gl::State.
//
// Some states affect the pipeline, but they are not derived from the GL state, but rather the
// properties of the Vulkan device or the context itself; such as whether a workaround is in
// effect, or the context is robust.  For VK_EXT_graphics_pipeline_library, such state that affects
// multiple subsets of the pipeline is duplicated in each subset (for example, there are two
// copies of isRobustContext, one for vertex input and one for shader stages).
void GraphicsPipelineDesc::initDefaults(const ContextVk *contextVk, GraphicsPipelineSubset subset)
{
    if (GraphicsPipelineHasVertexInput(subset))
    {
        // Set all vertex input attributes to default, the default format is Float
        angle::FormatID defaultFormat = GetCurrentValueFormatID(gl::VertexAttribType::Float);
        for (PackedAttribDesc &packedAttrib : mVertexInput.vertex.attribs)
        {
            SetBitField(packedAttrib.divisor, 0);
            SetBitField(packedAttrib.format, defaultFormat);
            SetBitField(packedAttrib.compressed, 0);
            SetBitField(packedAttrib.offset, 0);
        }
        mVertexInput.vertex.shaderAttribComponentType = 0;
        memset(mVertexInput.vertex.strides, 0, sizeof(mVertexInput.vertex.strides));

        SetBitField(mVertexInput.inputAssembly.bits.topology, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
        mVertexInput.inputAssembly.bits.primitiveRestartEnable = 0;
        mVertexInput.inputAssembly.bits.supportsDynamicState1 =
            contextVk->getFeatures().supportsExtendedDynamicState.enabled;
        mVertexInput.inputAssembly.bits.forceStaticVertexStrideState =
            contextVk->getFeatures().forceStaticVertexStrideState.enabled;
        mVertexInput.inputAssembly.bits.padding = 0;
    }

    if (GraphicsPipelineHasShaders(subset))
    {
        mShaders.shaders.bits.viewportNegativeOneToOne =
            contextVk->getFeatures().supportsDepthClipControl.enabled;
        mShaders.shaders.bits.depthClampEnable =
            contextVk->getFeatures().depthClamping.enabled ? VK_TRUE : VK_FALSE;
        SetBitField(mShaders.shaders.bits.cullMode, VK_CULL_MODE_NONE);
        SetBitField(mShaders.shaders.bits.frontFace, VK_FRONT_FACE_COUNTER_CLOCKWISE);
        mShaders.shaders.bits.rasterizerDiscardEnable = 0;
        mShaders.shaders.bits.depthBiasEnable         = 0;
        SetBitField(mShaders.shaders.bits.patchVertices, 3);
        mShaders.shaders.bits.depthBoundsTest                   = 0;
        mShaders.shaders.bits.depthTest                         = 0;
        mShaders.shaders.bits.depthWrite                        = 0;
        mShaders.shaders.bits.stencilTest                       = 0;
        mShaders.shaders.bits.nonZeroStencilWriteMaskWorkaround = 0;
        SetBitField(mShaders.shaders.bits.depthCompareOp, VK_COMPARE_OP_LESS);
        mShaders.shaders.bits.surfaceRotation  = 0;
        mShaders.shaders.bits.padding          = 0;
        mShaders.shaders.emulatedDitherControl = 0;
        mShaders.shaders.padding               = 0;
        SetBitField(mShaders.shaders.front.fail, VK_STENCIL_OP_KEEP);
        SetBitField(mShaders.shaders.front.pass, VK_STENCIL_OP_KEEP);
        SetBitField(mShaders.shaders.front.depthFail, VK_STENCIL_OP_KEEP);
        SetBitField(mShaders.shaders.front.compare, VK_COMPARE_OP_ALWAYS);
        SetBitField(mShaders.shaders.back.fail, VK_STENCIL_OP_KEEP);
        SetBitField(mShaders.shaders.back.pass, VK_STENCIL_OP_KEEP);
        SetBitField(mShaders.shaders.back.depthFail, VK_STENCIL_OP_KEEP);
        SetBitField(mShaders.shaders.back.compare, VK_COMPARE_OP_ALWAYS);
    }

    if (GraphicsPipelineHasShadersOrFragmentOutput(subset))
    {
        mSharedNonVertexInput.multisample.bits.sampleMask = std::numeric_limits<uint16_t>::max();
        mSharedNonVertexInput.multisample.bits.rasterizationSamplesMinusOne = 0;
        mSharedNonVertexInput.multisample.bits.sampleShadingEnable          = 0;
        mSharedNonVertexInput.multisample.bits.alphaToCoverageEnable        = 0;
        mSharedNonVertexInput.multisample.bits.alphaToOneEnable             = 0;
        mSharedNonVertexInput.multisample.bits.subpass                      = 0;
        mSharedNonVertexInput.multisample.bits.minSampleShading = kMinSampleShadingScale;
    }

    if (GraphicsPipelineHasFragmentOutput(subset))
    {
        constexpr VkFlags kAllColorBits = (VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                           VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT);

        for (uint32_t colorIndexGL = 0; colorIndexGL < gl::IMPLEMENTATION_MAX_DRAW_BUFFERS;
             ++colorIndexGL)
        {
            Int4Array_Set(mFragmentOutput.blend.colorWriteMaskBits, colorIndexGL, kAllColorBits);
        }

        PackedColorBlendAttachmentState blendAttachmentState;
        SetBitField(blendAttachmentState.srcColorBlendFactor, VK_BLEND_FACTOR_ONE);
        SetBitField(blendAttachmentState.dstColorBlendFactor, VK_BLEND_FACTOR_ZERO);
        SetBitField(blendAttachmentState.colorBlendOp, VK_BLEND_OP_ADD);
        SetBitField(blendAttachmentState.srcAlphaBlendFactor, VK_BLEND_FACTOR_ONE);
        SetBitField(blendAttachmentState.dstAlphaBlendFactor, VK_BLEND_FACTOR_ZERO);
        SetBitField(blendAttachmentState.alphaBlendOp, VK_BLEND_OP_ADD);

        std::fill(&mFragmentOutput.blend.attachments[0],
                  &mFragmentOutput.blend.attachments[gl::IMPLEMENTATION_MAX_DRAW_BUFFERS],
                  blendAttachmentState);

        mFragmentOutput.blendMaskAndLogic.bits.blendEnableMask = 0;
        mFragmentOutput.blendMaskAndLogic.bits.logicOpEnable   = 0;
        SetBitField(mFragmentOutput.blendMaskAndLogic.bits.logicOp, VK_LOGIC_OP_COPY);
        mFragmentOutput.blendMaskAndLogic.bits.padding = 0;
    }

    // Context robustness affects vertex input and shader stages.
    mVertexInput.inputAssembly.bits.isRobustContext = mShaders.shaders.bits.isRobustContext =
        contextVk->shouldUsePipelineRobustness();

    // Context protected-ness affects all subsets.
    mVertexInput.inputAssembly.bits.isProtectedContext = mShaders.shaders.bits.isProtectedContext =
        mFragmentOutput.blendMaskAndLogic.bits.isProtectedContext =
            contextVk->shouldRestrictPipelineToProtectedAccess();
}

VkResult GraphicsPipelineDesc::initializePipeline(Context *context,
                                                  PipelineCacheAccess *pipelineCache,
                                                  GraphicsPipelineSubset subset,
                                                  const RenderPass &compatibleRenderPass,
                                                  const PipelineLayout &pipelineLayout,
                                                  const ShaderModuleMap &shaders,
                                                  const SpecializationConstants &specConsts,
                                                  Pipeline *pipelineOut,
                                                  CacheLookUpFeedback *feedbackOut) const
{
    GraphicsPipelineVertexInputVulkanStructs vertexInputState;
    GraphicsPipelineShadersVulkanStructs shadersState;
    GraphicsPipelineSharedNonVertexInputVulkanStructs sharedNonVertexInputState;
    GraphicsPipelineFragmentOutputVulkanStructs fragmentOutputState;
    GraphicsPipelineDynamicStateList dynamicStateList;

    VkGraphicsPipelineCreateInfo createInfo = {};
    createInfo.sType                        = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    createInfo.flags                        = 0;
    createInfo.renderPass                   = compatibleRenderPass.getHandle();
    createInfo.subpass                      = mSharedNonVertexInput.multisample.bits.subpass;

    const bool hasVertexInput             = GraphicsPipelineHasVertexInput(subset);
    const bool hasShaders                 = GraphicsPipelineHasShaders(subset);
    const bool hasShadersOrFragmentOutput = GraphicsPipelineHasShadersOrFragmentOutput(subset);
    const bool hasFragmentOutput          = GraphicsPipelineHasFragmentOutput(subset);

    if (hasVertexInput)
    {
        initializePipelineVertexInputState(context, &vertexInputState, &dynamicStateList);

        createInfo.pVertexInputState   = &vertexInputState.vertexInputState;
        createInfo.pInputAssemblyState = &vertexInputState.inputAssemblyState;
    }

    if (hasShaders)
    {
        initializePipelineShadersState(context, shaders, specConsts, &shadersState,
                                       &dynamicStateList);

        createInfo.stageCount          = static_cast<uint32_t>(shadersState.shaderStages.size());
        createInfo.pStages             = shadersState.shaderStages.data();
        createInfo.pTessellationState  = &shadersState.tessellationState;
        createInfo.pViewportState      = &shadersState.viewportState;
        createInfo.pRasterizationState = &shadersState.rasterState;
        createInfo.pDepthStencilState  = &shadersState.depthStencilState;
        createInfo.layout              = pipelineLayout.getHandle();
    }

    if (hasShadersOrFragmentOutput)
    {
        initializePipelineSharedNonVertexInputState(context, &sharedNonVertexInputState,
                                                    &dynamicStateList);

        createInfo.pMultisampleState = &sharedNonVertexInputState.multisampleState;
    }

    if (hasFragmentOutput)
    {
        initializePipelineFragmentOutputState(context, &fragmentOutputState, &dynamicStateList);

        createInfo.pColorBlendState = &fragmentOutputState.blendState;
    }

    VkPipelineDynamicStateCreateInfo dynamicState = {};
    dynamicState.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStateList.size());
    dynamicState.pDynamicStates    = dynamicStateList.data();
    createInfo.pDynamicState       = dynamicStateList.empty() ? nullptr : &dynamicState;

    // If not a complete pipeline, specify which subset is being created
    VkGraphicsPipelineLibraryCreateInfoEXT libraryInfo = {};
    libraryInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_LIBRARY_CREATE_INFO_EXT;

    if (subset != GraphicsPipelineSubset::Complete)
    {
        switch (subset)
        {
            case GraphicsPipelineSubset::VertexInput:
                libraryInfo.flags = VK_GRAPHICS_PIPELINE_LIBRARY_VERTEX_INPUT_INTERFACE_BIT_EXT;
                break;
            case GraphicsPipelineSubset::Shaders:
                libraryInfo.flags = VK_GRAPHICS_PIPELINE_LIBRARY_PRE_RASTERIZATION_SHADERS_BIT_EXT |
                                    VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_SHADER_BIT_EXT;
                break;
            case GraphicsPipelineSubset::FragmentOutput:
                libraryInfo.flags = VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_OUTPUT_INTERFACE_BIT_EXT;
                break;
            default:
                UNREACHABLE();
                break;
        }

        createInfo.flags |= VK_PIPELINE_CREATE_LIBRARY_BIT_KHR;

        AddToPNextChain(&createInfo, &libraryInfo);
    }

    VkPipelineRobustnessCreateInfoEXT robustness = {};
    robustness.sType = VK_STRUCTURE_TYPE_PIPELINE_ROBUSTNESS_CREATE_INFO_EXT;

    // Enable robustness on the pipeline if needed.  Note that the global robustBufferAccess feature
    // must be disabled by default.
    if ((hasVertexInput && mVertexInput.inputAssembly.bits.isRobustContext) ||
        (hasShaders && mShaders.shaders.bits.isRobustContext))
    {
        ASSERT(context->getFeatures().supportsPipelineRobustness.enabled);

        robustness.storageBuffers = VK_PIPELINE_ROBUSTNESS_BUFFER_BEHAVIOR_ROBUST_BUFFER_ACCESS_EXT;
        robustness.uniformBuffers = VK_PIPELINE_ROBUSTNESS_BUFFER_BEHAVIOR_ROBUST_BUFFER_ACCESS_EXT;
        robustness.vertexInputs   = VK_PIPELINE_ROBUSTNESS_BUFFER_BEHAVIOR_ROBUST_BUFFER_ACCESS_EXT;
        robustness.images         = VK_PIPELINE_ROBUSTNESS_IMAGE_BEHAVIOR_DEVICE_DEFAULT_EXT;

        AddToPNextChain(&createInfo, &robustness);
    }

    if ((hasVertexInput && mVertexInput.inputAssembly.bits.isProtectedContext) ||
        (hasShaders && mShaders.shaders.bits.isProtectedContext) ||
        (hasFragmentOutput && mFragmentOutput.blendMaskAndLogic.bits.isProtectedContext))
    {
        ASSERT(context->getFeatures().supportsPipelineProtectedAccess.enabled);
        createInfo.flags |= VK_PIPELINE_CREATE_PROTECTED_ACCESS_ONLY_BIT_EXT;
    }
    else if (context->getFeatures().supportsPipelineProtectedAccess.enabled)
    {
        createInfo.flags |= VK_PIPELINE_CREATE_NO_PROTECTED_ACCESS_BIT_EXT;
    }

    VkPipelineCreationFeedback feedback = {};
    gl::ShaderMap<VkPipelineCreationFeedback> perStageFeedback;

    VkPipelineCreationFeedbackCreateInfo feedbackInfo = {};
    feedbackInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CREATION_FEEDBACK_CREATE_INFO;

    const bool supportsFeedback = context->getFeatures().supportsPipelineCreationFeedback.enabled;
    if (supportsFeedback)
    {
        feedbackInfo.pPipelineCreationFeedback = &feedback;
        // Provide some storage for per-stage data, even though it's not used.  This first works
        // around a VVL bug that doesn't allow `pipelineStageCreationFeedbackCount=0` despite the
        // spec (See https://github.com/KhronosGroup/Vulkan-ValidationLayers/issues/4161).  Even
        // with fixed VVL, several drivers crash when this storage is missing too.
        feedbackInfo.pipelineStageCreationFeedbackCount = createInfo.stageCount;
        feedbackInfo.pPipelineStageCreationFeedbacks    = perStageFeedback.data();

        AddToPNextChain(&createInfo, &feedbackInfo);
    }

    VkResult result = pipelineCache->createGraphicsPipeline(context, createInfo, pipelineOut);

    if (supportsFeedback)
    {
        const bool cacheHit =
            (feedback.flags & VK_PIPELINE_CREATION_FEEDBACK_APPLICATION_PIPELINE_CACHE_HIT_BIT) !=
            0;

        *feedbackOut = cacheHit ? CacheLookUpFeedback::Hit : CacheLookUpFeedback::Miss;
        ApplyPipelineCreationFeedback(context, feedback);
    }

    return result;
}

void GraphicsPipelineDesc::initializePipelineVertexInputState(
    Context *context,
    GraphicsPipelineVertexInputVulkanStructs *stateOut,
    GraphicsPipelineDynamicStateList *dynamicStateListOut) const
{
    // TODO(jmadill): Possibly use different path for ES 3.1 split bindings/attribs.
    uint32_t vertexAttribCount = 0;

    stateOut->divisorState.sType =
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_DIVISOR_STATE_CREATE_INFO_EXT;
    stateOut->divisorState.pVertexBindingDivisors = stateOut->divisorDesc.data();
    for (size_t attribIndexSizeT :
         gl::AttributesMask(mVertexInput.inputAssembly.bits.programActiveAttributeLocations))
    {
        const uint32_t attribIndex = static_cast<uint32_t>(attribIndexSizeT);

        VkVertexInputBindingDescription &bindingDesc  = stateOut->bindingDescs[vertexAttribCount];
        VkVertexInputAttributeDescription &attribDesc = stateOut->attributeDescs[vertexAttribCount];
        const PackedAttribDesc &packedAttrib          = mVertexInput.vertex.attribs[attribIndex];

        bindingDesc.binding = attribIndex;
        bindingDesc.stride  = static_cast<uint32_t>(mVertexInput.vertex.strides[attribIndex]);
        if (packedAttrib.divisor != 0)
        {
            bindingDesc.inputRate = static_cast<VkVertexInputRate>(VK_VERTEX_INPUT_RATE_INSTANCE);
            stateOut->divisorDesc[stateOut->divisorState.vertexBindingDivisorCount].binding =
                bindingDesc.binding;
            stateOut->divisorDesc[stateOut->divisorState.vertexBindingDivisorCount].divisor =
                packedAttrib.divisor;
            ++stateOut->divisorState.vertexBindingDivisorCount;
        }
        else
        {
            bindingDesc.inputRate = static_cast<VkVertexInputRate>(VK_VERTEX_INPUT_RATE_VERTEX);
        }

        // Get the corresponding VkFormat for the attrib's format.
        angle::FormatID formatID            = static_cast<angle::FormatID>(packedAttrib.format);
        const Format &format                = context->getRenderer()->getFormat(formatID);
        const angle::Format &intendedFormat = format.getIntendedFormat();
        VkFormat vkFormat = format.getActualBufferVkFormat(packedAttrib.compressed);

        const gl::ComponentType attribType = GetVertexAttributeComponentType(
            intendedFormat.isPureInt(), intendedFormat.vertexAttribType);
        const gl::ComponentType programAttribType = gl::GetComponentTypeMask(
            gl::ComponentTypeMask(mVertexInput.vertex.shaderAttribComponentType), attribIndex);

        // If using dynamic state for stride, the value for stride is unconditionally 0 here.
        // |ContextVk::handleDirtyGraphicsVertexBuffers| implements the same fix when setting stride
        // dynamically.
        ASSERT(!context->getFeatures().supportsExtendedDynamicState.enabled ||
               context->getFeatures().forceStaticVertexStrideState.enabled ||
               bindingDesc.stride == 0);

        // This forces stride to 0 when glVertexAttribPointer specifies a different type from the
        // program's attribute type except when the type mismatch is a mismatched integer sign.
        if (bindingDesc.stride > 0 && attribType != programAttribType)
        {
            if (attribType == gl::ComponentType::Float ||
                programAttribType == gl::ComponentType::Float)
            {
                // When dealing with float to int or unsigned int or vice versa, just override the
                // format with a compatible one.
                vkFormat = kMismatchedComponentTypeMap[programAttribType];
            }
            else
            {
                // When converting from an unsigned to a signed format or vice versa, attempt to
                // match the bit width.
                angle::FormatID convertedFormatID = gl::ConvertFormatSignedness(intendedFormat);
                const Format &convertedFormat =
                    context->getRenderer()->getFormat(convertedFormatID);
                ASSERT(intendedFormat.channelCount ==
                       convertedFormat.getIntendedFormat().channelCount);
                ASSERT(intendedFormat.redBits == convertedFormat.getIntendedFormat().redBits);
                ASSERT(intendedFormat.greenBits == convertedFormat.getIntendedFormat().greenBits);
                ASSERT(intendedFormat.blueBits == convertedFormat.getIntendedFormat().blueBits);
                ASSERT(intendedFormat.alphaBits == convertedFormat.getIntendedFormat().alphaBits);

                vkFormat = convertedFormat.getActualBufferVkFormat(packedAttrib.compressed);
            }

            ASSERT(context->getRenderer()->getNativeExtensions().relaxedVertexAttributeTypeANGLE);

            if (programAttribType == gl::ComponentType::Float ||
                attribType == gl::ComponentType::Float)
            {
                bindingDesc.stride = 0;  // Prevent out-of-bounds accesses.
            }
        }

        attribDesc.binding  = attribIndex;
        attribDesc.format   = vkFormat;
        attribDesc.location = static_cast<uint32_t>(attribIndex);
        attribDesc.offset   = packedAttrib.offset;

        vertexAttribCount++;
    }

    // The binding descriptions are filled in at draw time.
    stateOut->vertexInputState.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    stateOut->vertexInputState.flags = 0;
    stateOut->vertexInputState.vertexBindingDescriptionCount   = vertexAttribCount;
    stateOut->vertexInputState.pVertexBindingDescriptions      = stateOut->bindingDescs.data();
    stateOut->vertexInputState.vertexAttributeDescriptionCount = vertexAttribCount;
    stateOut->vertexInputState.pVertexAttributeDescriptions    = stateOut->attributeDescs.data();
    if (stateOut->divisorState.vertexBindingDivisorCount)
    {
        stateOut->vertexInputState.pNext = &stateOut->divisorState;
    }

    const PackedInputAssemblyState &inputAssembly = mVertexInput.inputAssembly;

    // Primitive topology is filled in at draw time.
    stateOut->inputAssemblyState.sType =
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    stateOut->inputAssemblyState.flags = 0;
    stateOut->inputAssemblyState.topology =
        static_cast<VkPrimitiveTopology>(inputAssembly.bits.topology);
    stateOut->inputAssemblyState.primitiveRestartEnable =
        static_cast<VkBool32>(inputAssembly.bits.primitiveRestartEnable);

    // Dynamic state
    if (context->getFeatures().supportsExtendedDynamicState.enabled)
    {
        if (vertexAttribCount > 0 && !context->getFeatures().forceStaticVertexStrideState.enabled)
        {
            dynamicStateListOut->push_back(VK_DYNAMIC_STATE_VERTEX_INPUT_BINDING_STRIDE);
        }
    }
    if (context->getFeatures().supportsExtendedDynamicState2.enabled)
    {
        dynamicStateListOut->push_back(VK_DYNAMIC_STATE_PRIMITIVE_RESTART_ENABLE);
    }
}

void GraphicsPipelineDesc::initializePipelineShadersState(
    Context *context,
    const ShaderModuleMap &shaders,
    const SpecializationConstants &specConsts,
    GraphicsPipelineShadersVulkanStructs *stateOut,
    GraphicsPipelineDynamicStateList *dynamicStateListOut) const
{
    InitializeSpecializationInfo(specConsts, &stateOut->specializationEntries,
                                 &stateOut->specializationInfo);

    // Vertex shader is always expected to be present.
    const ShaderModule &vertexModule = shaders[gl::ShaderType::Vertex].get();
    ASSERT(vertexModule.valid());
    VkPipelineShaderStageCreateInfo vertexStage = {};
    SetPipelineShaderStageInfo(VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                               VK_SHADER_STAGE_VERTEX_BIT, vertexModule.getHandle(),
                               stateOut->specializationInfo, &vertexStage);
    stateOut->shaderStages.push_back(vertexStage);

    const ShaderModulePointer &tessControlPointer = shaders[gl::ShaderType::TessControl];
    if (tessControlPointer.valid())
    {
        const ShaderModule &tessControlModule            = tessControlPointer.get();
        VkPipelineShaderStageCreateInfo tessControlStage = {};
        SetPipelineShaderStageInfo(VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                                   VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
                                   tessControlModule.getHandle(), stateOut->specializationInfo,
                                   &tessControlStage);
        stateOut->shaderStages.push_back(tessControlStage);
    }

    const ShaderModulePointer &tessEvaluationPointer = shaders[gl::ShaderType::TessEvaluation];
    if (tessEvaluationPointer.valid())
    {
        const ShaderModule &tessEvaluationModule            = tessEvaluationPointer.get();
        VkPipelineShaderStageCreateInfo tessEvaluationStage = {};
        SetPipelineShaderStageInfo(VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                                   VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
                                   tessEvaluationModule.getHandle(), stateOut->specializationInfo,
                                   &tessEvaluationStage);
        stateOut->shaderStages.push_back(tessEvaluationStage);
    }

    const ShaderModulePointer &geometryPointer = shaders[gl::ShaderType::Geometry];
    if (geometryPointer.valid())
    {
        const ShaderModule &geometryModule            = geometryPointer.get();
        VkPipelineShaderStageCreateInfo geometryStage = {};
        SetPipelineShaderStageInfo(VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                                   VK_SHADER_STAGE_GEOMETRY_BIT, geometryModule.getHandle(),
                                   stateOut->specializationInfo, &geometryStage);
        stateOut->shaderStages.push_back(geometryStage);
    }

    // Fragment shader is optional.
    const ShaderModulePointer &fragmentPointer = shaders[gl::ShaderType::Fragment];
    if (fragmentPointer.valid() && !mShaders.shaders.bits.rasterizerDiscardEnable)
    {
        const ShaderModule &fragmentModule            = fragmentPointer.get();
        VkPipelineShaderStageCreateInfo fragmentStage = {};
        SetPipelineShaderStageInfo(VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                                   VK_SHADER_STAGE_FRAGMENT_BIT, fragmentModule.getHandle(),
                                   stateOut->specializationInfo, &fragmentStage);
        stateOut->shaderStages.push_back(fragmentStage);
    }

    // Set initial viewport and scissor state.
    stateOut->viewportState.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    stateOut->viewportState.flags         = 0;
    stateOut->viewportState.viewportCount = 1;
    stateOut->viewportState.pViewports    = nullptr;
    stateOut->viewportState.scissorCount  = 1;
    stateOut->viewportState.pScissors     = nullptr;

    if (context->getFeatures().supportsDepthClipControl.enabled)
    {
        stateOut->depthClipControl.sType =
            VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_DEPTH_CLIP_CONTROL_CREATE_INFO_EXT;
        stateOut->depthClipControl.negativeOneToOne =
            static_cast<VkBool32>(mShaders.shaders.bits.viewportNegativeOneToOne);

        stateOut->viewportState.pNext = &stateOut->depthClipControl;
    }

    // Rasterizer state.
    stateOut->rasterState.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    stateOut->rasterState.flags = 0;
    stateOut->rasterState.depthClampEnable =
        static_cast<VkBool32>(mShaders.shaders.bits.depthClampEnable);
    stateOut->rasterState.rasterizerDiscardEnable =
        static_cast<VkBool32>(mShaders.shaders.bits.rasterizerDiscardEnable);
    stateOut->rasterState.polygonMode = VK_POLYGON_MODE_FILL;
    stateOut->rasterState.cullMode  = static_cast<VkCullModeFlags>(mShaders.shaders.bits.cullMode);
    stateOut->rasterState.frontFace = static_cast<VkFrontFace>(mShaders.shaders.bits.frontFace);
    stateOut->rasterState.depthBiasEnable =
        static_cast<VkBool32>(mShaders.shaders.bits.depthBiasEnable);
    stateOut->rasterState.lineWidth = 0;
    const void **pNextPtr           = &stateOut->rasterState.pNext;

    const PackedMultisampleAndSubpassState &multisample = mSharedNonVertexInput.multisample;

    stateOut->rasterLineState.sType =
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_LINE_STATE_CREATE_INFO_EXT;
    // Enable Bresenham line rasterization if available and the following conditions are met:
    // 1.) not multisampling
    // 2.) VUID-VkGraphicsPipelineCreateInfo-lineRasterizationMode-02766:
    // The Vulkan spec states: If the lineRasterizationMode member of a
    // VkPipelineRasterizationLineStateCreateInfoEXT structure included in the pNext chain of
    // pRasterizationState is VK_LINE_RASTERIZATION_MODE_BRESENHAM_EXT or
    // VK_LINE_RASTERIZATION_MODE_RECTANGULAR_SMOOTH_EXT and if rasterization is enabled, then the
    // alphaToCoverageEnable, alphaToOneEnable, and sampleShadingEnable members of pMultisampleState
    // must all be VK_FALSE.
    if (multisample.bits.rasterizationSamplesMinusOne == 0 &&
        !mShaders.shaders.bits.rasterizerDiscardEnable && !multisample.bits.alphaToCoverageEnable &&
        !multisample.bits.alphaToOneEnable && !multisample.bits.sampleShadingEnable &&
        context->getFeatures().bresenhamLineRasterization.enabled)
    {
        stateOut->rasterLineState.lineRasterizationMode = VK_LINE_RASTERIZATION_MODE_BRESENHAM_EXT;
        *pNextPtr                                       = &stateOut->rasterLineState;
        pNextPtr                                        = &stateOut->rasterLineState.pNext;
    }

    // Always set provoking vertex mode to last if available.
    if (context->getFeatures().provokingVertex.enabled)
    {
        stateOut->provokingVertexState.sType =
            VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_PROVOKING_VERTEX_STATE_CREATE_INFO_EXT;
        stateOut->provokingVertexState.provokingVertexMode =
            VK_PROVOKING_VERTEX_MODE_LAST_VERTEX_EXT;
        *pNextPtr = &stateOut->provokingVertexState;
        pNextPtr  = &stateOut->provokingVertexState.pNext;
    }

    // When depth clamping is used, depth clipping is automatically disabled.
    // When the 'depthClamping' feature is enabled, we'll be using depth clamping
    // to work around a driver issue, not as an alternative to depth clipping. Therefore we need to
    // explicitly re-enable depth clipping.
    if (context->getFeatures().depthClamping.enabled)
    {
        stateOut->depthClipState.sType =
            VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_DEPTH_CLIP_STATE_CREATE_INFO_EXT;
        stateOut->depthClipState.depthClipEnable = VK_TRUE;
        *pNextPtr                                = &stateOut->depthClipState;
        pNextPtr                                 = &stateOut->depthClipState.pNext;
    }

    if (context->getFeatures().supportsGeometryStreamsCapability.enabled)
    {
        stateOut->rasterStreamState.sType =
            VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_STREAM_CREATE_INFO_EXT;
        stateOut->rasterStreamState.rasterizationStream = 0;
        *pNextPtr                                       = &stateOut->rasterStreamState;
        pNextPtr                                        = &stateOut->rasterStreamState.pNext;
    }

    // Depth/stencil state.
    stateOut->depthStencilState.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    stateOut->depthStencilState.flags = 0;
    stateOut->depthStencilState.depthTestEnable =
        static_cast<VkBool32>(mShaders.shaders.bits.depthTest);
    stateOut->depthStencilState.depthWriteEnable =
        static_cast<VkBool32>(mShaders.shaders.bits.depthWrite);
    stateOut->depthStencilState.depthCompareOp =
        static_cast<VkCompareOp>(mShaders.shaders.bits.depthCompareOp);
    stateOut->depthStencilState.depthBoundsTestEnable =
        static_cast<VkBool32>(mShaders.shaders.bits.depthBoundsTest);
    stateOut->depthStencilState.stencilTestEnable =
        static_cast<VkBool32>(mShaders.shaders.bits.stencilTest);
    UnpackStencilState(mShaders.shaders.front, &stateOut->depthStencilState.front,
                       mShaders.shaders.bits.nonZeroStencilWriteMaskWorkaround);
    UnpackStencilState(mShaders.shaders.back, &stateOut->depthStencilState.back,
                       mShaders.shaders.bits.nonZeroStencilWriteMaskWorkaround);
    stateOut->depthStencilState.minDepthBounds = 0;
    stateOut->depthStencilState.maxDepthBounds = 0;

    // tessellation State
    if (tessControlPointer.valid() && tessEvaluationPointer.valid())
    {
        stateOut->domainOriginState.sType =
            VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_DOMAIN_ORIGIN_STATE_CREATE_INFO;
        stateOut->domainOriginState.pNext        = NULL;
        stateOut->domainOriginState.domainOrigin = VK_TESSELLATION_DOMAIN_ORIGIN_LOWER_LEFT;

        stateOut->tessellationState.sType =
            VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO;
        stateOut->tessellationState.flags = 0;
        stateOut->tessellationState.pNext = &stateOut->domainOriginState;
        stateOut->tessellationState.patchControlPoints =
            static_cast<uint32_t>(mShaders.shaders.bits.patchVertices);
    }

    // Dynamic state
    dynamicStateListOut->push_back(VK_DYNAMIC_STATE_VIEWPORT);
    dynamicStateListOut->push_back(VK_DYNAMIC_STATE_SCISSOR);
    dynamicStateListOut->push_back(VK_DYNAMIC_STATE_LINE_WIDTH);
    dynamicStateListOut->push_back(VK_DYNAMIC_STATE_DEPTH_BIAS);
    dynamicStateListOut->push_back(VK_DYNAMIC_STATE_DEPTH_BOUNDS);
    dynamicStateListOut->push_back(VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK);
    dynamicStateListOut->push_back(VK_DYNAMIC_STATE_STENCIL_WRITE_MASK);
    dynamicStateListOut->push_back(VK_DYNAMIC_STATE_STENCIL_REFERENCE);
    if (context->getFeatures().supportsExtendedDynamicState.enabled)
    {
        dynamicStateListOut->push_back(VK_DYNAMIC_STATE_CULL_MODE_EXT);
        dynamicStateListOut->push_back(VK_DYNAMIC_STATE_FRONT_FACE_EXT);
        dynamicStateListOut->push_back(VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE);
        dynamicStateListOut->push_back(VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE);
        dynamicStateListOut->push_back(VK_DYNAMIC_STATE_DEPTH_COMPARE_OP);
        dynamicStateListOut->push_back(VK_DYNAMIC_STATE_STENCIL_TEST_ENABLE);
        dynamicStateListOut->push_back(VK_DYNAMIC_STATE_STENCIL_OP);
    }
    if (context->getFeatures().supportsExtendedDynamicState2.enabled)
    {
        dynamicStateListOut->push_back(VK_DYNAMIC_STATE_RASTERIZER_DISCARD_ENABLE);
        dynamicStateListOut->push_back(VK_DYNAMIC_STATE_DEPTH_BIAS_ENABLE);
    }
    if (context->getFeatures().supportsFragmentShadingRate.enabled)
    {
        dynamicStateListOut->push_back(VK_DYNAMIC_STATE_FRAGMENT_SHADING_RATE_KHR);
    }
}

void GraphicsPipelineDesc::initializePipelineSharedNonVertexInputState(
    Context *context,
    GraphicsPipelineSharedNonVertexInputVulkanStructs *stateOut,
    GraphicsPipelineDynamicStateList *dynamicStateListOut) const
{
    const PackedMultisampleAndSubpassState &multisample = mSharedNonVertexInput.multisample;

    stateOut->sampleMask = multisample.bits.sampleMask;

    // Multisample state.
    stateOut->multisampleState.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    stateOut->multisampleState.flags = 0;
    stateOut->multisampleState.rasterizationSamples =
        gl_vk::GetSamples(multisample.bits.rasterizationSamplesMinusOne + 1);
    stateOut->multisampleState.sampleShadingEnable =
        static_cast<VkBool32>(multisample.bits.sampleShadingEnable);
    stateOut->multisampleState.minSampleShading =
        static_cast<float>(multisample.bits.minSampleShading) / kMinSampleShadingScale;
    stateOut->multisampleState.pSampleMask = &stateOut->sampleMask;
    stateOut->multisampleState.alphaToCoverageEnable =
        static_cast<VkBool32>(multisample.bits.alphaToCoverageEnable);
    stateOut->multisampleState.alphaToOneEnable =
        static_cast<VkBool32>(multisample.bits.alphaToOneEnable);
}

void GraphicsPipelineDesc::initializePipelineFragmentOutputState(
    Context *context,
    GraphicsPipelineFragmentOutputVulkanStructs *stateOut,
    GraphicsPipelineDynamicStateList *dynamicStateListOut) const
{
    stateOut->blendState.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    stateOut->blendState.flags = 0;
    stateOut->blendState.logicOpEnable =
        static_cast<VkBool32>(mFragmentOutput.blendMaskAndLogic.bits.logicOpEnable);
    stateOut->blendState.logicOp =
        static_cast<VkLogicOp>(mFragmentOutput.blendMaskAndLogic.bits.logicOp);
    stateOut->blendState.attachmentCount =
        static_cast<uint32_t>(mSharedNonVertexInput.renderPass.colorAttachmentRange());
    stateOut->blendState.pAttachments = stateOut->blendAttachmentState.data();

    // If this graphics pipeline is for the unresolve operation, correct the color attachment count
    // for that subpass.
    if ((mSharedNonVertexInput.renderPass.getColorUnresolveAttachmentMask().any() ||
         mSharedNonVertexInput.renderPass.hasDepthStencilUnresolveAttachment()) &&
        mSharedNonVertexInput.multisample.bits.subpass == 0)
    {
        stateOut->blendState.attachmentCount = static_cast<uint32_t>(
            mSharedNonVertexInput.renderPass.getColorUnresolveAttachmentMask().count());
    }

    // Specify rasterization order for color when available and there is
    // framebuffer fetch.  This allows implementation of coherent framebuffer
    // fetch / advanced blend.
    //
    // We can do better by setting the bit only when there is coherent
    // framebuffer fetch, but getRenderPassFramebufferFetchMode does not
    // distinguish coherent / non-coherent yet.  Also, once an app uses
    // framebufer fetch, we treat all render passes as if they use framebuffer
    // fetch.  This check is not very effective.
    if (context->getFeatures().supportsRasterizationOrderAttachmentAccess.enabled &&
        getRenderPassFramebufferFetchMode())
    {
        stateOut->blendState.flags |=
            VK_PIPELINE_COLOR_BLEND_STATE_CREATE_RASTERIZATION_ORDER_ATTACHMENT_ACCESS_BIT_EXT;
    }

    const gl::DrawBufferMask blendEnableMask(
        mFragmentOutput.blendMaskAndLogic.bits.blendEnableMask);

    // Zero-init all states.
    stateOut->blendAttachmentState = {};

    const PackedColorBlendState &colorBlend = mFragmentOutput.blend;

    for (uint32_t colorIndexGL = 0; colorIndexGL < stateOut->blendState.attachmentCount;
         ++colorIndexGL)
    {
        VkPipelineColorBlendAttachmentState &state = stateOut->blendAttachmentState[colorIndexGL];

        if (blendEnableMask[colorIndexGL])
        {
            // To avoid triggering valid usage error, blending must be disabled for formats that do
            // not have VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT feature bit set.
            // From OpenGL ES clients, this means disabling blending for integer formats.
            if (!angle::Format::Get(mSharedNonVertexInput.renderPass[colorIndexGL]).isInt())
            {
                ASSERT(!context->getRenderer()
                            ->getFormat(mSharedNonVertexInput.renderPass[colorIndexGL])
                            .getActualRenderableImageFormat()
                            .isInt());

                // The blend fixed-function is enabled with normal blend as well as advanced blend
                // when the Vulkan extension is present.  When emulating advanced blend in the
                // shader, the blend fixed-function must be disabled.
                const PackedColorBlendAttachmentState &packedBlendState =
                    colorBlend.attachments[colorIndexGL];
                if (packedBlendState.colorBlendOp <= static_cast<uint8_t>(VK_BLEND_OP_MAX) ||
                    context->getFeatures().supportsBlendOperationAdvanced.enabled)
                {
                    state.blendEnable = VK_TRUE;
                    UnpackBlendAttachmentState(packedBlendState, &state);
                }
            }
        }

        ASSERT(context->getRenderer()->getNativeExtensions().robustFragmentShaderOutputANGLE);
        if ((mFragmentOutput.blendMaskAndLogic.bits.missingOutputsMask >> colorIndexGL & 1) != 0)
        {
            state.colorWriteMask = 0;
        }
        else
        {
            state.colorWriteMask =
                Int4Array_Get<VkColorComponentFlags>(colorBlend.colorWriteMaskBits, colorIndexGL);
        }
    }

    // Dynamic state
    dynamicStateListOut->push_back(VK_DYNAMIC_STATE_BLEND_CONSTANTS);
    if (context->getFeatures().supportsLogicOpDynamicState.enabled)
    {
        dynamicStateListOut->push_back(VK_DYNAMIC_STATE_LOGIC_OP_EXT);
    }
}

void GraphicsPipelineDesc::updateVertexInput(ContextVk *contextVk,
                                             GraphicsPipelineTransitionBits *transition,
                                             uint32_t attribIndex,
                                             GLuint stride,
                                             GLuint divisor,
                                             angle::FormatID format,
                                             bool compressed,
                                             GLuint relativeOffset)
{
    PackedAttribDesc &packedAttrib = mVertexInput.vertex.attribs[attribIndex];

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
        ANGLE_GET_INDEXED_TRANSITION_BIT(mVertexInput.vertex.attribs, attribIndex, kAttribBits);

    // Each attribute is 4 bytes, so only one transition bit needs to be set.
    static_assert(kPackedAttribDescSize == kGraphicsPipelineDirtyBitBytes,
                  "Adjust transition bits");
    transition->set(kBit);

    if (!contextVk->getFeatures().supportsExtendedDynamicState.enabled ||
        contextVk->getFeatures().forceStaticVertexStrideState.enabled)
    {
        SetBitField(mVertexInput.vertex.strides[attribIndex], stride);
        transition->set(ANGLE_GET_INDEXED_TRANSITION_BIT(
            mVertexInput.vertex.strides, attribIndex,
            sizeof(mVertexInput.vertex.strides[0]) * kBitsPerByte));
    }
}

void GraphicsPipelineDesc::setVertexShaderComponentTypes(gl::AttributesMask activeAttribLocations,
                                                         gl::ComponentTypeMask componentTypeMask)
{
    SetBitField(mVertexInput.inputAssembly.bits.programActiveAttributeLocations,
                activeAttribLocations.bits());

    const gl::ComponentTypeMask activeComponentTypeMask =
        componentTypeMask & gl::GetActiveComponentTypeMask(activeAttribLocations);

    SetBitField(mVertexInput.vertex.shaderAttribComponentType, activeComponentTypeMask.bits());
}

void GraphicsPipelineDesc::updateVertexShaderComponentTypes(
    GraphicsPipelineTransitionBits *transition,
    gl::AttributesMask activeAttribLocations,
    gl::ComponentTypeMask componentTypeMask)
{
    if (mVertexInput.inputAssembly.bits.programActiveAttributeLocations !=
        activeAttribLocations.bits())
    {
        SetBitField(mVertexInput.inputAssembly.bits.programActiveAttributeLocations,
                    activeAttribLocations.bits());
        transition->set(ANGLE_GET_TRANSITION_BIT(mVertexInput.inputAssembly.bits));
    }

    const gl::ComponentTypeMask activeComponentTypeMask =
        componentTypeMask & gl::GetActiveComponentTypeMask(activeAttribLocations);

    if (mVertexInput.vertex.shaderAttribComponentType != activeComponentTypeMask.bits())
    {
        SetBitField(mVertexInput.vertex.shaderAttribComponentType, activeComponentTypeMask.bits());
        transition->set(ANGLE_GET_TRANSITION_BIT(mVertexInput.vertex.shaderAttribComponentType));
    }
}

void GraphicsPipelineDesc::setTopology(gl::PrimitiveMode drawMode)
{
    VkPrimitiveTopology vkTopology = gl_vk::GetPrimitiveTopology(drawMode);
    SetBitField(mVertexInput.inputAssembly.bits.topology, vkTopology);
}

void GraphicsPipelineDesc::updateTopology(GraphicsPipelineTransitionBits *transition,
                                          gl::PrimitiveMode drawMode)
{
    setTopology(drawMode);
    transition->set(ANGLE_GET_TRANSITION_BIT(mVertexInput.inputAssembly.bits));
}

void GraphicsPipelineDesc::updateDepthClipControl(GraphicsPipelineTransitionBits *transition,
                                                  bool negativeOneToOne)
{
    SetBitField(mShaders.shaders.bits.viewportNegativeOneToOne, negativeOneToOne);
    transition->set(ANGLE_GET_TRANSITION_BIT(mShaders.shaders.bits));
}

void GraphicsPipelineDesc::updatePrimitiveRestartEnabled(GraphicsPipelineTransitionBits *transition,
                                                         bool primitiveRestartEnabled)
{
    mVertexInput.inputAssembly.bits.primitiveRestartEnable =
        static_cast<uint16_t>(primitiveRestartEnabled);
    transition->set(ANGLE_GET_TRANSITION_BIT(mVertexInput.inputAssembly.bits));
}

void GraphicsPipelineDesc::updateCullMode(GraphicsPipelineTransitionBits *transition,
                                          const gl::RasterizerState &rasterState)
{
    SetBitField(mShaders.shaders.bits.cullMode, gl_vk::GetCullMode(rasterState));
    transition->set(ANGLE_GET_TRANSITION_BIT(mShaders.shaders.bits));
}

void GraphicsPipelineDesc::updateFrontFace(GraphicsPipelineTransitionBits *transition,
                                           const gl::RasterizerState &rasterState,
                                           bool invertFrontFace)
{
    SetBitField(mShaders.shaders.bits.frontFace,
                gl_vk::GetFrontFace(rasterState.frontFace, invertFrontFace));
    transition->set(ANGLE_GET_TRANSITION_BIT(mShaders.shaders.bits));
}

void GraphicsPipelineDesc::updateRasterizerDiscardEnabled(
    GraphicsPipelineTransitionBits *transition,
    bool rasterizerDiscardEnabled)
{
    mShaders.shaders.bits.rasterizerDiscardEnable = static_cast<uint32_t>(rasterizerDiscardEnabled);
    transition->set(ANGLE_GET_TRANSITION_BIT(mShaders.shaders.bits));
}

uint32_t GraphicsPipelineDesc::getRasterizationSamples() const
{
    return mSharedNonVertexInput.multisample.bits.rasterizationSamplesMinusOne + 1;
}

void GraphicsPipelineDesc::setRasterizationSamples(uint32_t rasterizationSamples)
{
    ASSERT(rasterizationSamples > 0);
    mSharedNonVertexInput.multisample.bits.rasterizationSamplesMinusOne = rasterizationSamples - 1;
}

void GraphicsPipelineDesc::updateRasterizationSamples(GraphicsPipelineTransitionBits *transition,
                                                      uint32_t rasterizationSamples)
{
    setRasterizationSamples(rasterizationSamples);
    transition->set(ANGLE_GET_TRANSITION_BIT(mSharedNonVertexInput.multisample.bits));
}

void GraphicsPipelineDesc::updateAlphaToCoverageEnable(GraphicsPipelineTransitionBits *transition,
                                                       bool enable)
{
    mSharedNonVertexInput.multisample.bits.alphaToCoverageEnable = enable;
    transition->set(ANGLE_GET_TRANSITION_BIT(mSharedNonVertexInput.multisample.bits));
}

void GraphicsPipelineDesc::updateAlphaToOneEnable(GraphicsPipelineTransitionBits *transition,
                                                  bool enable)
{
    mSharedNonVertexInput.multisample.bits.alphaToOneEnable = enable;
    transition->set(ANGLE_GET_TRANSITION_BIT(mSharedNonVertexInput.multisample.bits));
}

void GraphicsPipelineDesc::updateSampleMask(GraphicsPipelineTransitionBits *transition,
                                            uint32_t maskNumber,
                                            uint32_t mask)
{
    ASSERT(maskNumber == 0);
    SetBitField(mSharedNonVertexInput.multisample.bits.sampleMask, mask);

    transition->set(ANGLE_GET_TRANSITION_BIT(mSharedNonVertexInput.multisample.bits));
}

void GraphicsPipelineDesc::updateSampleShading(GraphicsPipelineTransitionBits *transition,
                                               bool enable,
                                               float value)
{
    mSharedNonVertexInput.multisample.bits.sampleShadingEnable = enable;
    if (enable)
    {
        SetBitField(mSharedNonVertexInput.multisample.bits.minSampleShading,
                    static_cast<uint16_t>(value * kMinSampleShadingScale));
    }
    else
    {
        mSharedNonVertexInput.multisample.bits.minSampleShading = kMinSampleShadingScale;
    }

    transition->set(ANGLE_GET_TRANSITION_BIT(mSharedNonVertexInput.multisample.bits));
}

void GraphicsPipelineDesc::setSingleBlend(uint32_t colorIndexGL,
                                          bool enabled,
                                          VkBlendOp op,
                                          VkBlendFactor srcFactor,
                                          VkBlendFactor dstFactor)
{
    mFragmentOutput.blendMaskAndLogic.bits.blendEnableMask |=
        static_cast<uint8_t>(1 << colorIndexGL);

    PackedColorBlendAttachmentState &blendAttachmentState =
        mFragmentOutput.blend.attachments[colorIndexGL];

    SetBitField(blendAttachmentState.colorBlendOp, op);
    SetBitField(blendAttachmentState.alphaBlendOp, op);

    SetBitField(blendAttachmentState.srcColorBlendFactor, srcFactor);
    SetBitField(blendAttachmentState.dstColorBlendFactor, dstFactor);
    SetBitField(blendAttachmentState.srcAlphaBlendFactor, VK_BLEND_FACTOR_ZERO);
    SetBitField(blendAttachmentState.dstAlphaBlendFactor, VK_BLEND_FACTOR_ONE);
}

void GraphicsPipelineDesc::updateBlendEnabled(GraphicsPipelineTransitionBits *transition,
                                              gl::DrawBufferMask blendEnabledMask)
{
    SetBitField(mFragmentOutput.blendMaskAndLogic.bits.blendEnableMask, blendEnabledMask.bits());
    transition->set(ANGLE_GET_TRANSITION_BIT(mFragmentOutput.blendMaskAndLogic.bits));
}

void GraphicsPipelineDesc::updateBlendEquations(GraphicsPipelineTransitionBits *transition,
                                                const gl::BlendStateExt &blendStateExt,
                                                gl::DrawBufferMask attachmentMask)
{
    constexpr size_t kSizeBits = sizeof(PackedColorBlendAttachmentState) * 8;

    for (size_t attachmentIndex : attachmentMask)
    {
        PackedColorBlendAttachmentState &blendAttachmentState =
            mFragmentOutput.blend.attachments[attachmentIndex];
        blendAttachmentState.colorBlendOp =
            PackGLBlendOp(blendStateExt.getEquationColorIndexed(attachmentIndex));
        blendAttachmentState.alphaBlendOp =
            PackGLBlendOp(blendStateExt.getEquationAlphaIndexed(attachmentIndex));
        transition->set(ANGLE_GET_INDEXED_TRANSITION_BIT(mFragmentOutput.blend.attachments,
                                                         attachmentIndex, kSizeBits));
    }
}

void GraphicsPipelineDesc::updateBlendFuncs(GraphicsPipelineTransitionBits *transition,
                                            const gl::BlendStateExt &blendStateExt,
                                            gl::DrawBufferMask attachmentMask)
{
    constexpr size_t kSizeBits = sizeof(PackedColorBlendAttachmentState) * 8;
    for (size_t attachmentIndex : attachmentMask)
    {
        PackedColorBlendAttachmentState &blendAttachmentState =
            mFragmentOutput.blend.attachments[attachmentIndex];
        blendAttachmentState.srcColorBlendFactor =
            PackGLBlendFactor(blendStateExt.getSrcColorIndexed(attachmentIndex));
        blendAttachmentState.dstColorBlendFactor =
            PackGLBlendFactor(blendStateExt.getDstColorIndexed(attachmentIndex));
        blendAttachmentState.srcAlphaBlendFactor =
            PackGLBlendFactor(blendStateExt.getSrcAlphaIndexed(attachmentIndex));
        blendAttachmentState.dstAlphaBlendFactor =
            PackGLBlendFactor(blendStateExt.getDstAlphaIndexed(attachmentIndex));
        transition->set(ANGLE_GET_INDEXED_TRANSITION_BIT(mFragmentOutput.blend.attachments,
                                                         attachmentIndex, kSizeBits));
    }
}

void GraphicsPipelineDesc::resetBlendFuncsAndEquations(GraphicsPipelineTransitionBits *transition,
                                                       const gl::BlendStateExt &blendStateExt,
                                                       gl::DrawBufferMask previousAttachmentsMask,
                                                       gl::DrawBufferMask newAttachmentsMask)
{
    // A framebuffer with attachments in P was bound, and now one with attachments in N is bound.
    // We need to clear blend funcs and equations for attachments in P that are not in N.  That is
    // attachments in P&~N.
    const gl::DrawBufferMask attachmentsToClear = previousAttachmentsMask & ~newAttachmentsMask;
    // We also need to restore blend funcs and equations for attachments in N that are not in P.
    const gl::DrawBufferMask attachmentsToAdd = newAttachmentsMask & ~previousAttachmentsMask;
    constexpr size_t kSizeBits                = sizeof(PackedColorBlendAttachmentState) * 8;

    for (size_t attachmentIndex : attachmentsToClear)
    {
        PackedColorBlendAttachmentState &blendAttachmentState =
            mFragmentOutput.blend.attachments[attachmentIndex];

        blendAttachmentState.colorBlendOp        = VK_BLEND_OP_ADD;
        blendAttachmentState.alphaBlendOp        = VK_BLEND_OP_ADD;
        blendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
        blendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
        blendAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        blendAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;

        transition->set(ANGLE_GET_INDEXED_TRANSITION_BIT(mFragmentOutput.blend.attachments,
                                                         attachmentIndex, kSizeBits));
    }

    if (attachmentsToAdd.any())
    {
        updateBlendFuncs(transition, blendStateExt, attachmentsToAdd);
        updateBlendEquations(transition, blendStateExt, attachmentsToAdd);
    }
}

void GraphicsPipelineDesc::setColorWriteMasks(gl::BlendStateExt::ColorMaskStorage::Type colorMasks,
                                              const gl::DrawBufferMask &alphaMask,
                                              const gl::DrawBufferMask &enabledDrawBuffers)
{
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
        Int4Array_Set(mFragmentOutput.blend.colorWriteMaskBits, colorIndexGL, mask);
    }
}

void GraphicsPipelineDesc::setSingleColorWriteMask(uint32_t colorIndexGL,
                                                   VkColorComponentFlags colorComponentFlags)
{
    uint8_t colorMask = static_cast<uint8_t>(colorComponentFlags);
    Int4Array_Set(mFragmentOutput.blend.colorWriteMaskBits, colorIndexGL, colorMask);
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
        transition->set(ANGLE_GET_INDEXED_TRANSITION_BIT(mFragmentOutput.blend.colorWriteMaskBits,
                                                         colorIndexGL, 4));
    }
}

void GraphicsPipelineDesc::updateMissingOutputsMask(GraphicsPipelineTransitionBits *transition,
                                                    gl::DrawBufferMask missingOutputsMask)
{
    if (mFragmentOutput.blendMaskAndLogic.bits.missingOutputsMask != missingOutputsMask.bits())
    {
        SetBitField(mFragmentOutput.blendMaskAndLogic.bits.missingOutputsMask,
                    missingOutputsMask.bits());
        transition->set(ANGLE_GET_TRANSITION_BIT(mFragmentOutput.blendMaskAndLogic.bits));
    }
}

void GraphicsPipelineDesc::updateLogicOpEnabled(GraphicsPipelineTransitionBits *transition,
                                                bool enable)
{
    mFragmentOutput.blendMaskAndLogic.bits.logicOpEnable = enable;
    transition->set(ANGLE_GET_TRANSITION_BIT(mFragmentOutput.blendMaskAndLogic.bits));
}

void GraphicsPipelineDesc::updateLogicOp(GraphicsPipelineTransitionBits *transition,
                                         VkLogicOp logicOp)
{
    SetBitField(mFragmentOutput.blendMaskAndLogic.bits.logicOp, logicOp);
    transition->set(ANGLE_GET_TRANSITION_BIT(mFragmentOutput.blendMaskAndLogic.bits));
}

void GraphicsPipelineDesc::setDepthTestEnabled(bool enabled)
{
    mShaders.shaders.bits.depthTest = enabled;
}

void GraphicsPipelineDesc::setDepthWriteEnabled(bool enabled)
{
    mShaders.shaders.bits.depthWrite = enabled;
}

void GraphicsPipelineDesc::setDepthFunc(VkCompareOp op)
{
    SetBitField(mShaders.shaders.bits.depthCompareOp, op);
}

void GraphicsPipelineDesc::setDepthClampEnabled(bool enabled)
{
    mShaders.shaders.bits.depthClampEnable = enabled;
}

void GraphicsPipelineDesc::setStencilTestEnabled(bool enabled)
{
    mShaders.shaders.bits.stencilTest = enabled;
}

void GraphicsPipelineDesc::setStencilFrontFuncs(VkCompareOp compareOp)
{
    SetBitField(mShaders.shaders.front.compare, compareOp);
}

void GraphicsPipelineDesc::setStencilBackFuncs(VkCompareOp compareOp)
{
    SetBitField(mShaders.shaders.back.compare, compareOp);
}

void GraphicsPipelineDesc::setStencilFrontOps(VkStencilOp failOp,
                                              VkStencilOp passOp,
                                              VkStencilOp depthFailOp)
{
    SetBitField(mShaders.shaders.front.fail, failOp);
    SetBitField(mShaders.shaders.front.pass, passOp);
    SetBitField(mShaders.shaders.front.depthFail, depthFailOp);
}

void GraphicsPipelineDesc::setStencilBackOps(VkStencilOp failOp,
                                             VkStencilOp passOp,
                                             VkStencilOp depthFailOp)
{
    SetBitField(mShaders.shaders.back.fail, failOp);
    SetBitField(mShaders.shaders.back.pass, passOp);
    SetBitField(mShaders.shaders.back.depthFail, depthFailOp);
}

void GraphicsPipelineDesc::updateDepthTestEnabled(GraphicsPipelineTransitionBits *transition,
                                                  const gl::DepthStencilState &depthStencilState,
                                                  const gl::Framebuffer *drawFramebuffer)
{
    // Only enable the depth test if the draw framebuffer has a depth buffer.  It's possible that
    // we're emulating a stencil-only buffer with a depth-stencil buffer
    setDepthTestEnabled(depthStencilState.depthTest && drawFramebuffer->hasDepth());
    transition->set(ANGLE_GET_TRANSITION_BIT(mShaders.shaders.bits));
}

void GraphicsPipelineDesc::updateDepthFunc(GraphicsPipelineTransitionBits *transition,
                                           const gl::DepthStencilState &depthStencilState)
{
    setDepthFunc(gl_vk::GetCompareOp(depthStencilState.depthFunc));
    transition->set(ANGLE_GET_TRANSITION_BIT(mShaders.shaders.bits));
}

void GraphicsPipelineDesc::updateSurfaceRotation(GraphicsPipelineTransitionBits *transition,
                                                 bool isRotatedAspectRatio)
{
    SetBitField(mShaders.shaders.bits.surfaceRotation, isRotatedAspectRatio);
    transition->set(ANGLE_GET_TRANSITION_BIT(mShaders.shaders.bits));
}

void GraphicsPipelineDesc::updateDepthWriteEnabled(GraphicsPipelineTransitionBits *transition,
                                                   const gl::DepthStencilState &depthStencilState,
                                                   const gl::Framebuffer *drawFramebuffer)
{
    // Don't write to depth buffers that should not exist
    const bool depthWriteEnabled =
        drawFramebuffer->hasDepth() && depthStencilState.depthTest && depthStencilState.depthMask;
    if (static_cast<bool>(mShaders.shaders.bits.depthWrite) != depthWriteEnabled)
    {
        setDepthWriteEnabled(depthWriteEnabled);
        transition->set(ANGLE_GET_TRANSITION_BIT(mShaders.shaders.bits));
    }
}

void GraphicsPipelineDesc::updateStencilTestEnabled(GraphicsPipelineTransitionBits *transition,
                                                    const gl::DepthStencilState &depthStencilState,
                                                    const gl::Framebuffer *drawFramebuffer)
{
    // Only enable the stencil test if the draw framebuffer has a stencil buffer.  It's possible
    // that we're emulating a depth-only buffer with a depth-stencil buffer
    setStencilTestEnabled(depthStencilState.stencilTest && drawFramebuffer->hasStencil());
    transition->set(ANGLE_GET_TRANSITION_BIT(mShaders.shaders.bits));
}

void GraphicsPipelineDesc::updateStencilFrontFuncs(GraphicsPipelineTransitionBits *transition,
                                                   const gl::DepthStencilState &depthStencilState)
{
    setStencilFrontFuncs(gl_vk::GetCompareOp(depthStencilState.stencilFunc));
    transition->set(ANGLE_GET_TRANSITION_BIT(mShaders.shaders.front));
}

void GraphicsPipelineDesc::updateStencilBackFuncs(GraphicsPipelineTransitionBits *transition,
                                                  const gl::DepthStencilState &depthStencilState)
{
    setStencilBackFuncs(gl_vk::GetCompareOp(depthStencilState.stencilBackFunc));
    transition->set(ANGLE_GET_TRANSITION_BIT(mShaders.shaders.back));
}

void GraphicsPipelineDesc::updateStencilFrontOps(GraphicsPipelineTransitionBits *transition,
                                                 const gl::DepthStencilState &depthStencilState)
{
    setStencilFrontOps(gl_vk::GetStencilOp(depthStencilState.stencilFail),
                       gl_vk::GetStencilOp(depthStencilState.stencilPassDepthPass),
                       gl_vk::GetStencilOp(depthStencilState.stencilPassDepthFail));
    transition->set(ANGLE_GET_TRANSITION_BIT(mShaders.shaders.front));
}

void GraphicsPipelineDesc::updateStencilBackOps(GraphicsPipelineTransitionBits *transition,
                                                const gl::DepthStencilState &depthStencilState)
{
    setStencilBackOps(gl_vk::GetStencilOp(depthStencilState.stencilBackFail),
                      gl_vk::GetStencilOp(depthStencilState.stencilBackPassDepthPass),
                      gl_vk::GetStencilOp(depthStencilState.stencilBackPassDepthFail));
    transition->set(ANGLE_GET_TRANSITION_BIT(mShaders.shaders.back));
}

void GraphicsPipelineDesc::updatePolygonOffsetFillEnabled(
    GraphicsPipelineTransitionBits *transition,
    bool enabled)
{
    mShaders.shaders.bits.depthBiasEnable = enabled;
    transition->set(ANGLE_GET_TRANSITION_BIT(mShaders.shaders.bits));
}

void GraphicsPipelineDesc::setRenderPassDesc(const RenderPassDesc &renderPassDesc)
{
    mSharedNonVertexInput.renderPass = renderPassDesc;
}

void GraphicsPipelineDesc::updateSubpass(GraphicsPipelineTransitionBits *transition,
                                         uint32_t subpass)
{
    if (mSharedNonVertexInput.multisample.bits.subpass != subpass)
    {
        SetBitField(mSharedNonVertexInput.multisample.bits.subpass, subpass);
        transition->set(ANGLE_GET_TRANSITION_BIT(mSharedNonVertexInput.multisample.bits));
    }
}

void GraphicsPipelineDesc::updatePatchVertices(GraphicsPipelineTransitionBits *transition,
                                               GLuint value)
{
    SetBitField(mShaders.shaders.bits.patchVertices, value);
    transition->set(ANGLE_GET_TRANSITION_BIT(mShaders.shaders.bits));
}

void GraphicsPipelineDesc::resetSubpass(GraphicsPipelineTransitionBits *transition)
{
    updateSubpass(transition, 0);
}

void GraphicsPipelineDesc::nextSubpass(GraphicsPipelineTransitionBits *transition)
{
    updateSubpass(transition, mSharedNonVertexInput.multisample.bits.subpass + 1);
}

void GraphicsPipelineDesc::setSubpass(uint32_t subpass)
{
    SetBitField(mSharedNonVertexInput.multisample.bits.subpass, subpass);
}

uint32_t GraphicsPipelineDesc::getSubpass() const
{
    return mSharedNonVertexInput.multisample.bits.subpass;
}

void GraphicsPipelineDesc::updateEmulatedDitherControl(GraphicsPipelineTransitionBits *transition,
                                                       uint16_t value)
{
    // Make sure we don't waste time resetting this to zero in the common no-dither case.
    ASSERT(value != 0 || mShaders.shaders.emulatedDitherControl != 0);

    mShaders.shaders.emulatedDitherControl = value;
    transition->set(ANGLE_GET_TRANSITION_BIT(mShaders.shaders.emulatedDitherControl));
}

void GraphicsPipelineDesc::updateNonZeroStencilWriteMaskWorkaround(
    GraphicsPipelineTransitionBits *transition,
    bool enabled)
{
    mShaders.shaders.bits.nonZeroStencilWriteMaskWorkaround = enabled;
    transition->set(ANGLE_GET_TRANSITION_BIT(mShaders.shaders.bits));
}

void GraphicsPipelineDesc::updateRenderPassDesc(GraphicsPipelineTransitionBits *transition,
                                                const RenderPassDesc &renderPassDesc)
{
    setRenderPassDesc(renderPassDesc);

    // The RenderPass is a special case where it spans multiple bits but has no member.
    constexpr size_t kFirstBit =
        offsetof(GraphicsPipelineDesc, mSharedNonVertexInput.renderPass) >> kTransitionByteShift;
    constexpr size_t kBitCount = kRenderPassDescSize >> kTransitionByteShift;
    for (size_t bit = 0; bit < kBitCount; ++bit)
    {
        transition->set(kFirstBit + bit);
    }
}

void GraphicsPipelineDesc::setRenderPassSampleCount(GLint samples)
{
    mSharedNonVertexInput.renderPass.setSamples(samples);
}

void GraphicsPipelineDesc::setRenderPassFramebufferFetchMode(bool hasFramebufferFetch)
{
    mSharedNonVertexInput.renderPass.setFramebufferFetchMode(hasFramebufferFetch);
}

void GraphicsPipelineDesc::setRenderPassColorAttachmentFormat(size_t colorIndexGL,
                                                              angle::FormatID formatID)
{
    mSharedNonVertexInput.renderPass.packColorAttachment(colorIndexGL, formatID);
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
    setOps(index, RenderPassLoadOp::Load, RenderPassStoreOp::Store);
    setStencilOps(index, RenderPassLoadOp::Load, RenderPassStoreOp::Store);
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
                                RenderPassLoadOp loadOp,
                                RenderPassStoreOp storeOp)
{
    PackedAttachmentOpsDesc &ops = mOps[index.get()];
    SetBitField(ops.loadOp, loadOp);
    SetBitField(ops.storeOp, storeOp);
    ops.isInvalidated = false;
}

void AttachmentOpsArray::setStencilOps(PackedAttachmentIndex index,
                                       RenderPassLoadOp loadOp,
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
    SetBitField(ops.loadOp, RenderPassLoadOp::Clear);
}

void AttachmentOpsArray::setClearStencilOp(PackedAttachmentIndex index)
{
    PackedAttachmentOpsDesc &ops = mOps[index.get()];
    SetBitField(ops.stencilLoadOp, RenderPassLoadOp::Clear);
}

size_t AttachmentOpsArray::hash() const
{
    return angle::ComputeGenericHash(mOps);
}

bool operator==(const AttachmentOpsArray &lhs, const AttachmentOpsArray &rhs)
{
    return memcmp(&lhs, &rhs, sizeof(AttachmentOpsArray)) == 0;
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
    return memcmp(&mPackedDescriptorSetLayout, &other.mPackedDescriptorSetLayout,
                  sizeof(mPackedDescriptorSetLayout)) == 0;
}

void DescriptorSetLayoutDesc::update(uint32_t bindingIndex,
                                     VkDescriptorType descriptorType,
                                     uint32_t count,
                                     VkShaderStageFlags stages,
                                     const Sampler *immutableSampler)
{
    ASSERT(static_cast<size_t>(descriptorType) < std::numeric_limits<uint16_t>::max());
    ASSERT(count < std::numeric_limits<uint16_t>::max());

    PackedDescriptorSetBinding &packedBinding = mPackedDescriptorSetLayout[bindingIndex];

    SetBitField(packedBinding.type, descriptorType);
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
PipelineLayoutDesc::PipelineLayoutDesc()
    : mDescriptorSetLayouts{}, mPushConstantRange{}, mPadding(0)
{}

PipelineLayoutDesc::~PipelineLayoutDesc() = default;

PipelineLayoutDesc::PipelineLayoutDesc(const PipelineLayoutDesc &other) = default;

PipelineLayoutDesc &PipelineLayoutDesc::operator=(const PipelineLayoutDesc &rhs)
{
    mDescriptorSetLayouts = rhs.mDescriptorSetLayouts;
    mPushConstantRange    = rhs.mPushConstantRange;
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
    mDescriptorSetLayouts[setIndex] = desc;
}

void PipelineLayoutDesc::updatePushConstantRange(VkShaderStageFlags stageMask,
                                                 uint32_t offset,
                                                 uint32_t size)
{
    SetBitField(mPushConstantRange.offset, offset);
    SetBitField(mPushConstantRange.size, size);
    SetBitField(mPushConstantRange.stageMask, stageMask);
}

// CreateMonolithicPipelineTask implementation.
CreateMonolithicPipelineTask::CreateMonolithicPipelineTask(
    RendererVk *renderer,
    const PipelineCacheAccess &pipelineCache,
    const PipelineLayout &pipelineLayout,
    const ShaderModuleMap &shaders,
    const SpecializationConstants &specConsts,
    const GraphicsPipelineDesc &desc)
    : Context(renderer),
      mPipelineCache(pipelineCache),
      mCompatibleRenderPass(nullptr),
      mPipelineLayout(pipelineLayout),
      mShaders(shaders),
      mSpecConsts(specConsts),
      mDesc(desc),
      mResult(VK_NOT_READY),
      mFeedback(CacheLookUpFeedback::None)
{
    // Make sure the given pipeline cache has an associated mutex as this task will be running
    // asynchronously.
    ASSERT(pipelineCache.isThreadSafe());
}

void CreateMonolithicPipelineTask::setCompatibleRenderPass(const RenderPass *compatibleRenderPass)
{
    mCompatibleRenderPass = compatibleRenderPass;
}

void CreateMonolithicPipelineTask::operator()()
{
    ANGLE_TRACE_EVENT0("gpu.angle", "CreateMonolithicPipelineTask");
    mResult = mDesc.initializePipeline(this, &mPipelineCache, vk::GraphicsPipelineSubset::Complete,
                                       *mCompatibleRenderPass, mPipelineLayout, mShaders,
                                       mSpecConsts, &mPipeline, &mFeedback);

    if (mRenderer->getFeatures().slowDownMonolithicPipelineCreationForTesting.enabled)
    {
        constexpr double kSlowdownTime = 0.05;

        double startTime = angle::GetCurrentSystemTime();
        while (angle::GetCurrentSystemTime() - startTime < kSlowdownTime)
        {
            // Busy waiting
        }
    }
}

void CreateMonolithicPipelineTask::handleError(VkResult result,
                                               const char *file,
                                               const char *function,
                                               unsigned int line)
{
    UNREACHABLE();
}

// WaitableMonolithicPipelineCreationTask implementation
WaitableMonolithicPipelineCreationTask::~WaitableMonolithicPipelineCreationTask()
{
    ASSERT(!mWaitableEvent);
    ASSERT(!mTask);
}

// PipelineHelper implementation.
PipelineHelper::PipelineHelper() = default;

PipelineHelper::~PipelineHelper() = default;

void PipelineHelper::destroy(VkDevice device)
{
    mPipeline.destroy(device);
    mLinkedPipelineToRelease.destroy(device);

    // If there is a pending task, wait for it before destruction.
    if (mMonolithicPipelineCreationTask.isValid())
    {
        if (mMonolithicPipelineCreationTask.isPosted())
        {
            mMonolithicPipelineCreationTask.wait();
            mMonolithicPipelineCreationTask.getTask()->getPipeline().destroy(device);
        }
        mMonolithicPipelineCreationTask.reset();
    }

    reset();
}

void PipelineHelper::release(ContextVk *contextVk)
{
    contextVk->getRenderer()->collectGarbage(mUse, &mPipeline);
    contextVk->getRenderer()->collectGarbage(mUse, &mLinkedPipelineToRelease);

    // If there is a pending task, wait for it before release.
    if (mMonolithicPipelineCreationTask.isValid())
    {
        if (mMonolithicPipelineCreationTask.isPosted())
        {
            mMonolithicPipelineCreationTask.wait();
            contextVk->getRenderer()->collectGarbage(
                mUse, &mMonolithicPipelineCreationTask.getTask()->getPipeline());
        }
        mMonolithicPipelineCreationTask.reset();
    }

    reset();
}

void PipelineHelper::reset()
{
    mCacheLookUpFeedback           = CacheLookUpFeedback::None;
    mMonolithicCacheLookUpFeedback = CacheLookUpFeedback::None;

    mLinkedShaders = nullptr;
}

angle::Result PipelineHelper::getPreferredPipeline(ContextVk *contextVk,
                                                   const Pipeline **pipelineOut)
{
    if (mMonolithicPipelineCreationTask.isValid())
    {
        // If there is a monolithic task pending, attempt to post it if not already.  Once the task
        // is done, retrieve the results and replace the pipeline.
        if (!mMonolithicPipelineCreationTask.isPosted())
        {
            ANGLE_TRY(contextVk->getShareGroup()->scheduleMonolithicPipelineCreationTask(
                contextVk, &mMonolithicPipelineCreationTask));
        }
        else if (mMonolithicPipelineCreationTask.isReady())
        {
            CreateMonolithicPipelineTask *task = &*mMonolithicPipelineCreationTask.getTask();
            ANGLE_VK_TRY(contextVk, task->getResult());

            mMonolithicCacheLookUpFeedback = task->getFeedback();

            // The pipeline will not be used anymore.  Every context that has used this pipeline has
            // already updated the serial.
            mLinkedPipelineToRelease = std::move(mPipeline);

            // Replace it with the monolithic one.
            mPipeline = std::move(task->getPipeline());

            mLinkedShaders = nullptr;

            mMonolithicPipelineCreationTask.reset();

            ++contextVk->getPerfCounters().monolithicPipelineCreation;
        }
    }

    *pipelineOut = &mPipeline;

    return angle::Result::Continue;
}

void PipelineHelper::addTransition(GraphicsPipelineTransitionBits bits,
                                   const GraphicsPipelineDesc *desc,
                                   PipelineHelper *pipeline)
{
    mTransitions.emplace_back(bits, desc, pipeline);
}

void PipelineHelper::setLinkedLibraryReferences(vk::PipelineHelper *shadersPipeline)
{
    mLinkedShaders = shadersPipeline;
}

void PipelineHelper::retainInRenderPass(RenderPassCommandBufferHelper *renderPassCommands)
{
    renderPassCommands->retainResource(this);

    // Keep references to the linked libraries alive.  Note that currently only need to do this for
    // the shaders library, as the vertex and fragment libraries live in the context until
    // destruction.
    if (mLinkedShaders != nullptr)
    {
        renderPassCommands->retainResource(mLinkedShaders);
    }
}

// FramebufferHelper implementation.
FramebufferHelper::FramebufferHelper() = default;

FramebufferHelper::~FramebufferHelper() = default;

FramebufferHelper::FramebufferHelper(FramebufferHelper &&other) : Resource(std::move(other))
{
    mFramebuffer = std::move(other.mFramebuffer);
}

FramebufferHelper &FramebufferHelper::operator=(FramebufferHelper &&other)
{
    std::swap(mUse, other.mUse);
    std::swap(mFramebuffer, other.mFramebuffer);
    return *this;
}

angle::Result FramebufferHelper::init(ContextVk *contextVk,
                                      const VkFramebufferCreateInfo &createInfo)
{
    ANGLE_VK_TRY(contextVk, mFramebuffer.init(contextVk->getDevice(), createInfo));
    return angle::Result::Continue;
}

void FramebufferHelper::destroy(RendererVk *rendererVk)
{
    mFramebuffer.destroy(rendererVk->getDevice());
}

void FramebufferHelper::release(ContextVk *contextVk)
{
    contextVk->addGarbage(&mFramebuffer);
}

// DescriptorSetDesc implementation.
size_t DescriptorSetDesc::hash() const
{
    if (mDescriptorInfos.empty())
    {
        return 0;
    }

    return angle::ComputeGenericHash(mDescriptorInfos.data(),
                                     sizeof(mDescriptorInfos[0]) * mDescriptorInfos.size());
}

// FramebufferDesc implementation.

FramebufferDesc::FramebufferDesc()
{
    reset();
}

FramebufferDesc::~FramebufferDesc()                                       = default;
FramebufferDesc::FramebufferDesc(const FramebufferDesc &other)            = default;
FramebufferDesc &FramebufferDesc::operator=(const FramebufferDesc &other) = default;

void FramebufferDesc::update(uint32_t index, ImageOrBufferViewSubresourceSerial serial)
{
    static_assert(kMaxFramebufferAttachments + 1 < std::numeric_limits<uint8_t>::max(),
                  "mMaxIndex size is too small");
    ASSERT(index < kMaxFramebufferAttachments);
    mSerials[index] = serial;
    if (serial.viewSerial.valid())
    {
        SetBitField(mMaxIndex, std::max(mMaxIndex, static_cast<uint16_t>(index + 1)));
    }
}

void FramebufferDesc::updateColor(uint32_t index, ImageOrBufferViewSubresourceSerial serial)
{
    update(kFramebufferDescColorIndexOffset + index, serial);
}

void FramebufferDesc::updateColorResolve(uint32_t index, ImageOrBufferViewSubresourceSerial serial)
{
    update(kFramebufferDescColorResolveIndexOffset + index, serial);
}

void FramebufferDesc::updateUnresolveMask(FramebufferNonResolveAttachmentMask unresolveMask)
{
    SetBitField(mUnresolveAttachmentMask, unresolveMask.bits());
}

void FramebufferDesc::updateDepthStencil(ImageOrBufferViewSubresourceSerial serial)
{
    update(kFramebufferDescDepthStencilIndex, serial);
}

void FramebufferDesc::updateDepthStencilResolve(ImageOrBufferViewSubresourceSerial serial)
{
    update(kFramebufferDescDepthStencilResolveIndexOffset, serial);
}

size_t FramebufferDesc::hash() const
{
    return angle::ComputeGenericHash(&mSerials, sizeof(mSerials[0]) * mMaxIndex) ^
           mHasFramebufferFetch << 26 ^ mIsRenderToTexture << 25 ^ mLayerCount << 16 ^
           mUnresolveAttachmentMask;
}

void FramebufferDesc::reset()
{
    mMaxIndex                = 0;
    mHasFramebufferFetch     = false;
    mLayerCount              = 0;
    mSrgbWriteControlMode    = 0;
    mUnresolveAttachmentMask = 0;
    mIsRenderToTexture       = 0;
    memset(&mSerials, 0, sizeof(mSerials));
}

bool FramebufferDesc::operator==(const FramebufferDesc &other) const
{
    if (mMaxIndex != other.mMaxIndex || mLayerCount != other.mLayerCount ||
        mUnresolveAttachmentMask != other.mUnresolveAttachmentMask ||
        mHasFramebufferFetch != other.mHasFramebufferFetch ||
        mSrgbWriteControlMode != other.mSrgbWriteControlMode ||
        mIsRenderToTexture != other.mIsRenderToTexture)
    {
        return false;
    }

    size_t validRegionSize = sizeof(mSerials[0]) * mMaxIndex;
    return memcmp(&mSerials, &other.mSerials, validRegionSize) == 0;
}

uint32_t FramebufferDesc::attachmentCount() const
{
    uint32_t count = 0;
    for (const ImageOrBufferViewSubresourceSerial &serial : mSerials)
    {
        if (serial.viewSerial.valid())
        {
            count++;
        }
    }
    return count;
}

FramebufferNonResolveAttachmentMask FramebufferDesc::getUnresolveAttachmentMask() const
{
    return FramebufferNonResolveAttachmentMask(mUnresolveAttachmentMask);
}

void FramebufferDesc::updateLayerCount(uint32_t layerCount)
{
    SetBitField(mLayerCount, layerCount);
}

void FramebufferDesc::setFramebufferFetchMode(bool hasFramebufferFetch)
{
    SetBitField(mHasFramebufferFetch, hasFramebufferFetch);
}

void FramebufferDesc::updateRenderToTexture(bool isRenderToTexture)
{
    SetBitField(mIsRenderToTexture, isRenderToTexture);
}

// YcbcrConversionDesc implementation
YcbcrConversionDesc::YcbcrConversionDesc()
{
    reset();
}

YcbcrConversionDesc::~YcbcrConversionDesc() = default;

YcbcrConversionDesc::YcbcrConversionDesc(const YcbcrConversionDesc &other) = default;

YcbcrConversionDesc &YcbcrConversionDesc::operator=(const YcbcrConversionDesc &rhs) = default;

size_t YcbcrConversionDesc::hash() const
{
    return angle::ComputeGenericHash(*this);
}

bool YcbcrConversionDesc::operator==(const YcbcrConversionDesc &other) const
{
    return memcmp(this, &other, sizeof(YcbcrConversionDesc)) == 0;
}

void YcbcrConversionDesc::reset()
{
    mExternalOrVkFormat = 0;
    mIsExternalFormat   = 0;
    mConversionModel    = 0;
    mColorRange         = 0;
    mXChromaOffset      = 0;
    mYChromaOffset      = 0;
    mChromaFilter       = 0;
    mRSwizzle           = 0;
    mGSwizzle           = 0;
    mBSwizzle           = 0;
    mASwizzle           = 0;
    mPadding            = 0;
    mReserved           = 0;
}

void YcbcrConversionDesc::update(RendererVk *rendererVk,
                                 uint64_t externalFormat,
                                 VkSamplerYcbcrModelConversion conversionModel,
                                 VkSamplerYcbcrRange colorRange,
                                 VkChromaLocation xChromaOffset,
                                 VkChromaLocation yChromaOffset,
                                 VkFilter chromaFilter,
                                 VkComponentMapping components,
                                 angle::FormatID intendedFormatID)
{
    const vk::Format &vkFormat = rendererVk->getFormat(intendedFormatID);
    ASSERT(externalFormat != 0 || vkFormat.getIntendedFormat().isYUV);

    SetBitField(mIsExternalFormat, (externalFormat) ? 1 : 0);
    mExternalOrVkFormat = (externalFormat)
                              ? externalFormat
                              : vkFormat.getActualImageVkFormat(vk::ImageAccess::SampleOnly);

    updateChromaFilter(rendererVk, chromaFilter);

    SetBitField(mConversionModel, conversionModel);
    SetBitField(mColorRange, colorRange);
    SetBitField(mXChromaOffset, xChromaOffset);
    SetBitField(mYChromaOffset, yChromaOffset);
    SetBitField(mRSwizzle, components.r);
    SetBitField(mGSwizzle, components.g);
    SetBitField(mBSwizzle, components.b);
    SetBitField(mASwizzle, components.a);
}

bool YcbcrConversionDesc::updateChromaFilter(RendererVk *rendererVk, VkFilter filter)
{
    // The app has requested a specific min/mag filter, reconcile that with the filter
    // requested by preferLinearFilterForYUV feature.
    //
    // preferLinearFilterForYUV enforces linear filter while forceNearestFiltering and
    // forceNearestMipFiltering enforces nearest filter, enabling one precludes the other.
    ASSERT(!rendererVk->getFeatures().preferLinearFilterForYUV.enabled ||
           (!rendererVk->getFeatures().forceNearestFiltering.enabled &&
            !rendererVk->getFeatures().forceNearestMipFiltering.enabled));

    VkFilter preferredChromaFilter = rendererVk->getPreferredFilterForYUV(filter);
    ASSERT(preferredChromaFilter == VK_FILTER_LINEAR || preferredChromaFilter == VK_FILTER_NEAREST);

    if (preferredChromaFilter == VK_FILTER_LINEAR && mIsExternalFormat == 0)
    {
        // Vulkan ICDs are allowed to not support LINEAR filter.
        angle::FormatID formatId =
            vk::GetFormatIDFromVkFormat(static_cast<VkFormat>(mExternalOrVkFormat));
        if (!rendererVk->hasImageFormatFeatureBits(
                formatId, VK_FORMAT_FEATURE_SAMPLED_IMAGE_YCBCR_CONVERSION_LINEAR_FILTER_BIT))
        {
            preferredChromaFilter = VK_FILTER_NEAREST;
        }
    }

    if (getChromaFilter() != preferredChromaFilter)
    {
        SetBitField(mChromaFilter, preferredChromaFilter);
        return true;
    }
    return false;
}

void YcbcrConversionDesc::updateConversionModel(VkSamplerYcbcrModelConversion conversionModel)
{
    SetBitField(mConversionModel, conversionModel);
}

angle::Result YcbcrConversionDesc::init(Context *context,
                                        SamplerYcbcrConversion *conversionOut) const
{
    // Create the VkSamplerYcbcrConversion
    VkSamplerYcbcrConversionCreateInfo samplerYcbcrConversionInfo = {};
    samplerYcbcrConversionInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_CREATE_INFO;
    samplerYcbcrConversionInfo.format =
        getExternalFormat() == 0 ? static_cast<VkFormat>(mExternalOrVkFormat) : VK_FORMAT_UNDEFINED;
    samplerYcbcrConversionInfo.xChromaOffset = static_cast<VkChromaLocation>(mXChromaOffset);
    samplerYcbcrConversionInfo.yChromaOffset = static_cast<VkChromaLocation>(mYChromaOffset);
    samplerYcbcrConversionInfo.ycbcrModel =
        static_cast<VkSamplerYcbcrModelConversion>(mConversionModel);
    samplerYcbcrConversionInfo.ycbcrRange   = static_cast<VkSamplerYcbcrRange>(mColorRange);
    samplerYcbcrConversionInfo.chromaFilter = static_cast<VkFilter>(mChromaFilter);
    samplerYcbcrConversionInfo.components   = {
        static_cast<VkComponentSwizzle>(mRSwizzle), static_cast<VkComponentSwizzle>(mGSwizzle),
        static_cast<VkComponentSwizzle>(mBSwizzle), static_cast<VkComponentSwizzle>(mASwizzle)};

#ifdef VK_USE_PLATFORM_ANDROID_KHR
    VkExternalFormatANDROID externalFormat = {};
    if (getExternalFormat() != 0)
    {
        externalFormat.sType             = VK_STRUCTURE_TYPE_EXTERNAL_FORMAT_ANDROID;
        externalFormat.externalFormat    = mExternalOrVkFormat;
        samplerYcbcrConversionInfo.pNext = &externalFormat;
    }
#else
    // We do not support external format for any platform other than Android.
    ASSERT(mIsExternalFormat == 0);
#endif  // VK_USE_PLATFORM_ANDROID_KHR

    ANGLE_VK_TRY(context, conversionOut->init(context->getDevice(), samplerYcbcrConversionInfo));
    return angle::Result::Continue;
}

// SamplerDesc implementation.
SamplerDesc::SamplerDesc()
{
    reset();
}

SamplerDesc::~SamplerDesc() = default;

SamplerDesc::SamplerDesc(const SamplerDesc &other) = default;

SamplerDesc &SamplerDesc::operator=(const SamplerDesc &rhs) = default;

SamplerDesc::SamplerDesc(ContextVk *contextVk,
                         const gl::SamplerState &samplerState,
                         bool stencilMode,
                         const YcbcrConversionDesc *ycbcrConversionDesc,
                         angle::FormatID intendedFormatID)
{
    update(contextVk, samplerState, stencilMode, ycbcrConversionDesc, intendedFormatID);
}

void SamplerDesc::reset()
{
    mMipLodBias    = 0.0f;
    mMaxAnisotropy = 0.0f;
    mMinLod        = 0.0f;
    mMaxLod        = 0.0f;
    mYcbcrConversionDesc.reset();
    mMagFilter         = 0;
    mMinFilter         = 0;
    mMipmapMode        = 0;
    mAddressModeU      = 0;
    mAddressModeV      = 0;
    mAddressModeW      = 0;
    mCompareEnabled    = 0;
    mCompareOp         = 0;
    mPadding           = 0;
    mBorderColorType   = 0;
    mBorderColor.red   = 0.0f;
    mBorderColor.green = 0.0f;
    mBorderColor.blue  = 0.0f;
    mBorderColor.alpha = 0.0f;
    mReserved          = 0;
}

void SamplerDesc::update(ContextVk *contextVk,
                         const gl::SamplerState &samplerState,
                         bool stencilMode,
                         const YcbcrConversionDesc *ycbcrConversionDesc,
                         angle::FormatID intendedFormatID)
{
    const angle::FeaturesVk &featuresVk = contextVk->getFeatures();
    mMipLodBias                         = 0.0f;
    if (featuresVk.forceTextureLodOffset1.enabled)
    {
        mMipLodBias = 1.0f;
    }
    else if (featuresVk.forceTextureLodOffset2.enabled)
    {
        mMipLodBias = 2.0f;
    }
    else if (featuresVk.forceTextureLodOffset3.enabled)
    {
        mMipLodBias = 3.0f;
    }
    else if (featuresVk.forceTextureLodOffset4.enabled)
    {
        mMipLodBias = 4.0f;
    }

    mMaxAnisotropy = samplerState.getMaxAnisotropy();
    mMinLod        = samplerState.getMinLod();
    mMaxLod        = samplerState.getMaxLod();

    GLenum minFilter = samplerState.getMinFilter();
    GLenum magFilter = samplerState.getMagFilter();
    if (ycbcrConversionDesc && ycbcrConversionDesc->valid())
    {
        // Update the SamplerYcbcrConversionCache key
        mYcbcrConversionDesc = *ycbcrConversionDesc;

        // Reconcile chroma filter and min/mag filters.
        //
        // VUID-VkSamplerCreateInfo-minFilter-01645
        // If sampler YCBCR conversion is enabled and the potential format features of the
        // sampler YCBCR conversion do not support
        // VK_FORMAT_FEATURE_SAMPLED_IMAGE_YCBCR_CONVERSION_SEPARATE_RECONSTRUCTION_FILTER_BIT,
        // minFilter and magFilter must be equal to the sampler YCBCR conversions chromaFilter.
        //
        // For simplicity assume external formats do not support that feature bit.
        ASSERT((mYcbcrConversionDesc.getExternalFormat() != 0) ||
               (angle::Format::Get(intendedFormatID).isYUV));
        const bool filtersMustMatch =
            (mYcbcrConversionDesc.getExternalFormat() != 0) ||
            !contextVk->getRenderer()->hasImageFormatFeatureBits(
                intendedFormatID,
                VK_FORMAT_FEATURE_SAMPLED_IMAGE_YCBCR_CONVERSION_SEPARATE_RECONSTRUCTION_FILTER_BIT);
        if (filtersMustMatch)
        {
            GLenum glFilter = (mYcbcrConversionDesc.getChromaFilter() == VK_FILTER_LINEAR)
                                  ? GL_LINEAR
                                  : GL_NEAREST;
            minFilter       = glFilter;
            magFilter       = glFilter;
        }
    }

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

    if (featuresVk.forceNearestFiltering.enabled)
    {
        magFilter = gl::ConvertToNearestFilterMode(magFilter);
        minFilter = gl::ConvertToNearestFilterMode(minFilter);
    }
    if (featuresVk.forceNearestMipFiltering.enabled)
    {
        minFilter = gl::ConvertToNearestMipFilterMode(minFilter);
    }

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

    mPadding = 0;

    mBorderColorType =
        (samplerState.getBorderColor().type == angle::ColorGeneric::Type::Float) ? 0 : 1;

    // Adjust border color according to intended format
    const vk::Format &vkFormat = contextVk->getRenderer()->getFormat(intendedFormatID);
    gl::ColorGeneric adjustedBorderColor =
        AdjustBorderColor(samplerState.getBorderColor(), vkFormat.getIntendedFormat(), stencilMode);
    mBorderColor = adjustedBorderColor.colorF;

    mReserved = 0;
}

angle::Result SamplerDesc::init(ContextVk *contextVk, Sampler *sampler) const
{
    const gl::Extensions &extensions = contextVk->getExtensions();

    bool anisotropyEnable = extensions.textureFilterAnisotropicEXT && mMaxAnisotropy > 1.0f;

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
        ASSERT(extensions.textureFilteringHintCHROMIUM);
        filteringInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_FILTERING_PRECISION_GOOGLE;
        filteringInfo.samplerFilteringPrecisionMode =
            VK_SAMPLER_FILTERING_PRECISION_MODE_HIGH_GOOGLE;
        AddToPNextChain(&createInfo, &filteringInfo);
    }

    VkSamplerYcbcrConversionInfo samplerYcbcrConversionInfo = {};
    if (mYcbcrConversionDesc.valid())
    {
        ASSERT((contextVk->getRenderer()->getFeatures().supportsYUVSamplerConversion.enabled));
        samplerYcbcrConversionInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_INFO;
        samplerYcbcrConversionInfo.pNext = nullptr;
        ANGLE_TRY(contextVk->getRenderer()->getYuvConversionCache().getSamplerYcbcrConversion(
            contextVk, mYcbcrConversionDesc, &samplerYcbcrConversionInfo.conversion));
        AddToPNextChain(&createInfo, &samplerYcbcrConversionInfo);

        // Vulkan spec requires these settings:
        createInfo.addressModeU            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        createInfo.addressModeV            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        createInfo.addressModeW            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        createInfo.anisotropyEnable        = VK_FALSE;
        createInfo.unnormalizedCoordinates = VK_FALSE;
    }

    VkSamplerCustomBorderColorCreateInfoEXT customBorderColorInfo = {};
    if (createInfo.addressModeU == VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER ||
        createInfo.addressModeV == VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER ||
        createInfo.addressModeW == VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER)
    {
        ASSERT((contextVk->getRenderer()->getFeatures().supportsCustomBorderColor.enabled));
        customBorderColorInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CUSTOM_BORDER_COLOR_CREATE_INFO_EXT;

        customBorderColorInfo.customBorderColor.float32[0] = mBorderColor.red;
        customBorderColorInfo.customBorderColor.float32[1] = mBorderColor.green;
        customBorderColorInfo.customBorderColor.float32[2] = mBorderColor.blue;
        customBorderColorInfo.customBorderColor.float32[3] = mBorderColor.alpha;

        if (mBorderColorType == static_cast<uint32_t>(angle::ColorGeneric::Type::Float))
        {
            createInfo.borderColor = VK_BORDER_COLOR_FLOAT_CUSTOM_EXT;
        }
        else
        {
            createInfo.borderColor = VK_BORDER_COLOR_INT_CUSTOM_EXT;
        }

        vk::AddToPNextChain(&createInfo, &customBorderColorInfo);
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
    return memcmp(this, &other, sizeof(SamplerDesc)) == 0;
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

void RenderPassHelper::release(ContextVk *contextVk)
{
    contextVk->addGarbage(&mRenderPass);
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

// DescriptorSetDesc implementation.
void DescriptorSetDesc::updateWriteDesc(const WriteDescriptorDesc &writeDesc)
{
    uint32_t binding              = writeDesc.binding;
    WriteDescriptorDesc &destDesc = mWriteDescriptors[binding];

    ASSERT(writeDesc.descriptorCount > 0);

    destDesc = writeDesc;
}

void DescriptorSetDesc::updateDescriptorSet(Context *context,
                                            UpdateDescriptorSetsBuilder *updateBuilder,
                                            const DescriptorDescHandles *handles,
                                            VkDescriptorSet descriptorSet) const
{
    for (uint32_t writeIndex = 0; writeIndex < static_cast<uint32_t>(mWriteDescriptors.size());
         ++writeIndex)
    {
        const WriteDescriptorDesc &writeDesc = mWriteDescriptors[writeIndex];

        if (writeDesc.descriptorCount == 0)
        {
            continue;
        }

        VkWriteDescriptorSet &writeSet = updateBuilder->allocWriteDescriptorSet();

        writeSet.descriptorCount  = writeDesc.descriptorCount;
        writeSet.descriptorType   = static_cast<VkDescriptorType>(writeDesc.descriptorType);
        writeSet.dstArrayElement  = 0;
        writeSet.dstBinding       = writeIndex;
        writeSet.dstSet           = descriptorSet;
        writeSet.pBufferInfo      = nullptr;
        writeSet.pImageInfo       = nullptr;
        writeSet.pNext            = nullptr;
        writeSet.pTexelBufferView = nullptr;
        writeSet.sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;

        uint32_t infoDescIndex = writeDesc.descriptorInfoIndex;

        switch (writeSet.descriptorType)
        {
            case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
            case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
            {
                ASSERT(writeDesc.descriptorCount == 1);
                VkBufferView &bufferView  = updateBuilder->allocBufferView();
                bufferView                = handles[infoDescIndex].bufferView;
                writeSet.pTexelBufferView = &bufferView;
                break;
            }
            case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
            case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
            case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
            case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
            {
                VkDescriptorBufferInfo *writeBuffers =
                    updateBuilder->allocDescriptorBufferInfos(writeSet.descriptorCount);
                for (uint32_t arrayElement = 0; arrayElement < writeSet.descriptorCount;
                     ++arrayElement)
                {
                    const DescriptorInfoDesc &infoDesc =
                        mDescriptorInfos[infoDescIndex + arrayElement];
                    VkDescriptorBufferInfo &bufferInfo = writeBuffers[arrayElement];
                    bufferInfo.buffer = handles[infoDescIndex + arrayElement].buffer;
                    bufferInfo.offset = infoDesc.imageViewSerialOrOffset;
                    bufferInfo.range  = infoDesc.imageLayoutOrRange;
                }
                writeSet.pBufferInfo = writeBuffers;
                break;
            }
            case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
            case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
            case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
            case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
            {
                VkDescriptorImageInfo *writeImages =
                    updateBuilder->allocDescriptorImageInfos(writeSet.descriptorCount);
                for (uint32_t arrayElement = 0; arrayElement < writeSet.descriptorCount;
                     ++arrayElement)
                {
                    const DescriptorInfoDesc &infoDesc =
                        mDescriptorInfos[infoDescIndex + arrayElement];
                    VkDescriptorImageInfo &imageInfo = writeImages[arrayElement];

                    ImageLayout imageLayout = static_cast<ImageLayout>(infoDesc.imageLayoutOrRange);

                    imageInfo.imageLayout = ConvertImageLayoutToVkImageLayout(context, imageLayout);
                    imageInfo.imageView   = handles[infoDescIndex + arrayElement].imageView;
                    imageInfo.sampler     = handles[infoDescIndex + arrayElement].sampler;
                }
                writeSet.pImageInfo = writeImages;
                break;
            }

            default:
                UNREACHABLE();
                break;
        }
    }
}

void DescriptorSetDesc::streamOut(std::ostream &ostr) const
{
    ostr << mWriteDescriptors.size() << " write descriptor descs:\n";

    for (uint32_t index = 0; index < static_cast<uint32_t>(mWriteDescriptors.size()); ++index)
    {
        const WriteDescriptorDesc &writeDesc = mWriteDescriptors[index];
        ostr << static_cast<int>(writeDesc.binding) << ": "
             << static_cast<int>(writeDesc.descriptorCount) << " "
             << kDescriptorTypeNameMap[writeDesc.descriptorType] << " descriptors: ";

        for (uint32_t offset = 0; offset < writeDesc.descriptorCount; ++offset)
        {
            const DescriptorInfoDesc &infoDesc =
                mDescriptorInfos[writeDesc.descriptorInfoIndex + offset];
            ostr << "{" << infoDesc.imageLayoutOrRange << ", " << infoDesc.imageSubresourceRange
                 << ", " << infoDesc.imageViewSerialOrOffset << ", "
                 << infoDesc.samplerOrBufferSerial << "}";
        }

        ostr << "\n";
    }
}

// DescriptorSetDescBuilder implementation.
DescriptorSetDescBuilder::DescriptorSetDescBuilder() = default;

DescriptorSetDescBuilder::~DescriptorSetDescBuilder()
{
    ASSERT(mUsedImages.empty());
    ASSERT(mUsedBufferBlocks.empty());
    ASSERT(mUsedBufferHelpers.empty());
}

DescriptorSetDescBuilder::DescriptorSetDescBuilder(const DescriptorSetDescBuilder &other)
    : mDesc(other.mDesc),
      mHandles(other.mHandles),
      mDynamicOffsets(other.mDynamicOffsets),
      mCurrentInfoIndex(other.mCurrentInfoIndex),
      mUsedImages(other.mUsedImages),
      mUsedBufferBlocks(other.mUsedBufferBlocks),
      mUsedBufferHelpers(other.mUsedBufferHelpers)
{}

DescriptorSetDescBuilder &DescriptorSetDescBuilder::operator=(const DescriptorSetDescBuilder &other)
{
    mDesc              = other.mDesc;
    mHandles           = other.mHandles;
    mDynamicOffsets    = other.mDynamicOffsets;
    mCurrentInfoIndex  = other.mCurrentInfoIndex;
    mUsedImages        = other.mUsedImages;
    mUsedBufferBlocks  = other.mUsedBufferBlocks;
    mUsedBufferHelpers = other.mUsedBufferHelpers;
    return *this;
}

void DescriptorSetDescBuilder::reset()
{
    mDesc.reset();
    mHandles.clear();
    mDynamicOffsets.clear();
    mCurrentInfoIndex = 0;
    mUsedImages.clear();
    mUsedBufferBlocks.clear();
    mUsedBufferHelpers.clear();
}

void DescriptorSetDescBuilder::updateWriteDesc(uint32_t bindingIndex,
                                               VkDescriptorType descriptorType,
                                               uint32_t descriptorCount)
{
    if (mDesc.hasWriteDescAtIndex(bindingIndex))
    {
        uint32_t infoIndex          = mDesc.getInfoDescIndex(bindingIndex);
        uint32_t oldDescriptorCount = mDesc.getDescriptorSetCount(bindingIndex);
        if (descriptorCount != oldDescriptorCount)
        {
            ASSERT(infoIndex + oldDescriptorCount == mCurrentInfoIndex);
            ASSERT(descriptorCount > oldDescriptorCount);
            uint32_t additionalDescriptors = descriptorCount - oldDescriptorCount;
            mDesc.incrementDescriptorCount(bindingIndex, additionalDescriptors);
            mCurrentInfoIndex += additionalDescriptors;
        }
    }
    else
    {
        WriteDescriptorDesc writeDesc = {};
        SetBitField(writeDesc.binding, bindingIndex);
        SetBitField(writeDesc.descriptorCount, descriptorCount);
        SetBitField(writeDesc.descriptorType, descriptorType);
        SetBitField(writeDesc.descriptorInfoIndex, mCurrentInfoIndex);
        mCurrentInfoIndex += descriptorCount;

        mDesc.updateWriteDesc(writeDesc);
    }
}

void DescriptorSetDescBuilder::updateUniformWrite(uint32_t shaderStageCount)
{
    for (uint32_t shaderIndex = 0; shaderIndex < shaderStageCount; ++shaderIndex)
    {
        updateWriteDesc(shaderIndex, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1);
    }
}

void DescriptorSetDescBuilder::updateUniformBuffer(uint32_t bindingIndex,
                                                   const BufferHelper &bufferHelper,
                                                   VkDeviceSize bufferRange)
{
    DescriptorInfoDesc infoDesc = {};
    SetBitField(infoDesc.imageLayoutOrRange, bufferRange);
    infoDesc.samplerOrBufferSerial = bufferHelper.getBlockSerial().getValue();
    mUsedBufferBlocks.emplace_back(bufferHelper.getBufferBlock());

    uint32_t infoIndex = mDesc.getInfoDescIndex(bindingIndex);

    mDesc.updateInfoDesc(infoIndex, infoDesc);
    mHandles[infoIndex].buffer = bufferHelper.getBuffer().getHandle();
}

void DescriptorSetDescBuilder::updateTransformFeedbackWrite(
    const ShaderInterfaceVariableInfoMap &variableInfoMap,
    uint32_t xfbBufferCount)
{
    updateWriteDesc(variableInfoMap.getXfbBufferBinding(0), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                    xfbBufferCount);
}

void DescriptorSetDescBuilder::updateTransformFeedbackBuffer(
    const Context *context,
    const ShaderInterfaceVariableInfoMap &variableInfoMap,
    uint32_t xfbBufferIndex,
    const BufferHelper &bufferHelper,
    VkDeviceSize bufferOffset,
    VkDeviceSize bufferRange)
{
    uint32_t baseBinding = variableInfoMap.getXfbBufferBinding(0);

    RendererVk *renderer = context->getRenderer();
    VkDeviceSize offsetAlignment =
        renderer->getPhysicalDeviceProperties().limits.minStorageBufferOffsetAlignment;
    // Set the offset as close as possible to the requested offset while remaining aligned.
    VkDeviceSize alignedOffset = (bufferOffset / offsetAlignment) * offsetAlignment;
    VkDeviceSize adjustedRange = bufferRange + (bufferOffset - alignedOffset);

    DescriptorInfoDesc infoDesc = {};
    SetBitField(infoDesc.imageLayoutOrRange, adjustedRange);
    SetBitField(infoDesc.imageViewSerialOrOffset, alignedOffset);
    infoDesc.samplerOrBufferSerial = bufferHelper.getBlockSerial().getValue();
    mUsedBufferBlocks.emplace_back(bufferHelper.getBufferBlock());

    uint32_t infoIndex = mDesc.getInfoDescIndex(baseBinding) + xfbBufferIndex;

    mDesc.updateInfoDesc(infoIndex, infoDesc);
    mHandles[infoIndex].buffer = bufferHelper.getBuffer().getHandle();
}

void DescriptorSetDescBuilder::updateUniformsAndXfb(Context *context,
                                                    const gl::ProgramExecutable &executable,
                                                    const ProgramExecutableVk &executableVk,
                                                    const BufferHelper *currentUniformBuffer,
                                                    const BufferHelper &emptyBuffer,
                                                    bool activeUnpaused,
                                                    TransformFeedbackVk *transformFeedbackVk)
{
    gl::ShaderBitSet linkedStages = executable.getLinkedShaderStages();

    const ShaderInterfaceVariableInfoMap &variableInfoMap = executableVk.getVariableInfoMap();

    for (const gl::ShaderType shaderType : linkedStages)
    {
        uint32_t binding = variableInfoMap.getDefaultUniformBinding(shaderType);
        updateWriteDesc(binding, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1);

        VkDeviceSize bufferRange = executableVk.getDefaultUniformAlignedSize(context, shaderType);
        if (bufferRange == 0)
        {
            updateUniformBuffer(binding, emptyBuffer, emptyBuffer.getSize());
        }
        else
        {
            ASSERT(currentUniformBuffer);
            updateUniformBuffer(binding, *currentUniformBuffer, bufferRange);
        }

        if (transformFeedbackVk && shaderType == gl::ShaderType::Vertex &&
            context->getRenderer()->getFeatures().emulateTransformFeedback.enabled)
        {
            transformFeedbackVk->updateTransformFeedbackDescriptorDesc(
                context, executable, variableInfoMap, emptyBuffer, activeUnpaused, this);
        }
    }
}

void UpdatePreCacheActiveTextures(const gl::ProgramExecutable &executable,
                                  const ProgramExecutableVk &executableVk,
                                  const std::vector<gl::SamplerBinding> &samplerBindings,
                                  const gl::ActiveTextureMask &activeTextures,
                                  const gl::ActiveTextureArray<TextureVk *> &textures,
                                  const gl::SamplerBindingVector &samplers,
                                  DescriptorSetDesc *desc)
{
    desc->reset();

    const ShaderInterfaceVariableInfoMap &variableInfoMap = executableVk.getVariableInfoMap();
    const std::vector<gl::LinkedUniform> &uniforms        = executable.getUniforms();

    for (gl::ShaderType shaderType : executable.getLinkedShaderStages())
    {
        for (uint32_t samplerIndex = 0; samplerIndex < samplerBindings.size(); ++samplerIndex)
        {
            const gl::SamplerBinding &samplerBinding = samplerBindings[samplerIndex];
            uint32_t arraySize = static_cast<uint32_t>(samplerBinding.boundTextureUnits.size());
            bool isSamplerExternalY2Y =
                samplerBinding.samplerType == GL_SAMPLER_EXTERNAL_2D_Y2Y_EXT;

            uint32_t uniformIndex = executable.getUniformIndexFromSamplerIndex(samplerIndex);
            const gl::LinkedUniform &samplerUniform = uniforms[uniformIndex];

            if (!samplerUniform.isActive(shaderType))
            {
                continue;
            }

            const ShaderInterfaceVariableInfo &info = variableInfoMap.getIndexedVariableInfo(
                shaderType, ShaderVariableType::Texture, samplerIndex);
            if (info.isDuplicate)
            {
                continue;
            }

            for (uint32_t arrayElement = 0; arrayElement < arraySize; ++arrayElement)
            {
                GLuint textureUnit = samplerBinding.boundTextureUnits[arrayElement];
                if (!activeTextures.test(textureUnit))
                    continue;
                TextureVk *textureVk = textures[textureUnit];

                DescriptorInfoDesc infoDesc = {};
                infoDesc.binding            = info.binding;

                if (textureVk->getState().getType() == gl::TextureType::Buffer)
                {
                    ImageOrBufferViewSubresourceSerial imageViewSerial =
                        textureVk->getBufferViewSerial();
                    infoDesc.imageViewSerialOrOffset = imageViewSerial.viewSerial.getValue();
                }
                else
                {
                    gl::Sampler *sampler       = samplers[textureUnit].get();
                    const SamplerVk *samplerVk = sampler ? vk::GetImpl(sampler) : nullptr;

                    const SamplerHelper &samplerHelper =
                        samplerVk ? samplerVk->getSampler()
                                  : textureVk->getSampler(isSamplerExternalY2Y);
                    const gl::SamplerState &samplerState =
                        sampler ? sampler->getSamplerState()
                                : textureVk->getState().getSamplerState();

                    ImageOrBufferViewSubresourceSerial imageViewSerial =
                        textureVk->getImageViewSubresourceSerial(samplerState);

                    // Layout is implicit.

                    infoDesc.imageViewSerialOrOffset = imageViewSerial.viewSerial.getValue();
                    infoDesc.samplerOrBufferSerial   = samplerHelper.getSamplerSerial().getValue();
                    memcpy(&infoDesc.imageSubresourceRange, &imageViewSerial.subresource,
                           sizeof(uint32_t));
                }

                desc->updateInfoDesc(static_cast<uint32_t>(textureUnit), infoDesc);
            }
        }
    }
}

angle::Result DescriptorSetDescBuilder::updateFullActiveTextures(
    Context *context,
    const ShaderInterfaceVariableInfoMap &variableInfoMap,
    const gl::ProgramExecutable &executable,
    const gl::ActiveTextureArray<TextureVk *> &textures,
    const gl::SamplerBindingVector &samplers,
    bool emulateSeamfulCubeMapSampling,
    PipelineType pipelineType,
    const SharedDescriptorSetCacheKey &sharedCacheKey)
{
    reset();
    for (gl::ShaderType shaderType : executable.getLinkedShaderStages())
    {
        ANGLE_TRY(updateExecutableActiveTexturesForShader(
            context, shaderType, variableInfoMap, executable, textures, samplers,
            emulateSeamfulCubeMapSampling, pipelineType, sharedCacheKey));
    }

    return angle::Result::Continue;
}

angle::Result DescriptorSetDescBuilder::updateExecutableActiveTexturesForShader(
    Context *context,
    gl::ShaderType shaderType,
    const ShaderInterfaceVariableInfoMap &variableInfoMap,
    const gl::ProgramExecutable &executable,
    const gl::ActiveTextureArray<TextureVk *> &textures,
    const gl::SamplerBindingVector &samplers,
    bool emulateSeamfulCubeMapSampling,
    PipelineType pipelineType,
    const SharedDescriptorSetCacheKey &sharedCacheKey)
{
    const std::vector<gl::SamplerBinding> &samplerBindings = executable.getSamplerBindings();
    const std::vector<gl::LinkedUniform> &uniforms         = executable.getUniforms();
    const gl::ActiveTextureTypeArray &textureTypes         = executable.getActiveSamplerTypes();

    for (uint32_t samplerIndex = 0; samplerIndex < samplerBindings.size(); ++samplerIndex)
    {
        const gl::SamplerBinding &samplerBinding = samplerBindings[samplerIndex];
        uint32_t uniformIndex = executable.getUniformIndexFromSamplerIndex(samplerIndex);
        const gl::LinkedUniform &samplerUniform = uniforms[uniformIndex];

        if (!samplerUniform.isActive(shaderType))
        {
            continue;
        }

        const ShaderInterfaceVariableInfo &info = variableInfoMap.getIndexedVariableInfo(
            shaderType, ShaderVariableType::Texture, samplerIndex);
        if (info.isDuplicate)
        {
            continue;
        }

        uint32_t arraySize       = static_cast<uint32_t>(samplerBinding.boundTextureUnits.size());
        uint32_t descriptorCount = arraySize * gl::ArraySizeProduct(samplerUniform.outerArraySizes);
        VkDescriptorType descriptorType = (samplerBinding.textureType == gl::TextureType::Buffer)
                                              ? VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER
                                              : VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

        updateWriteDesc(info.binding, descriptorType, descriptorCount);

        bool isSamplerExternalY2Y = samplerBinding.samplerType == GL_SAMPLER_EXTERNAL_2D_Y2Y_EXT;

        for (uint32_t arrayElement = 0; arrayElement < arraySize; ++arrayElement)
        {
            GLuint textureUnit   = samplerBinding.boundTextureUnits[arrayElement];
            TextureVk *textureVk = textures[textureUnit];

            DescriptorInfoDesc infoDesc = {};

            uint32_t infoIndex = mDesc.getInfoDescIndex(info.binding) + arrayElement +
                                 samplerUniform.outerArrayOffset;

            if (textureTypes[textureUnit] == gl::TextureType::Buffer)
            {
                ImageOrBufferViewSubresourceSerial imageViewSerial =
                    textureVk->getBufferViewSerial();
                infoDesc.imageViewSerialOrOffset = imageViewSerial.viewSerial.getValue();

                textureVk->onNewDescriptorSet(sharedCacheKey);

                const BufferView *view = nullptr;
                ANGLE_TRY(textureVk->getBufferViewAndRecordUse(context, nullptr, false, &view));
                mHandles[infoIndex].bufferView = view->getHandle();
            }
            else
            {
                gl::Sampler *sampler       = samplers[textureUnit].get();
                const SamplerVk *samplerVk = sampler ? vk::GetImpl(sampler) : nullptr;

                const SamplerHelper &samplerHelper =
                    samplerVk ? samplerVk->getSampler()
                              : textureVk->getSampler(isSamplerExternalY2Y);
                const gl::SamplerState &samplerState =
                    sampler ? sampler->getSamplerState() : textureVk->getState().getSamplerState();

                ImageOrBufferViewSubresourceSerial imageViewSerial =
                    textureVk->getImageViewSubresourceSerial(samplerState);

                textureVk->onNewDescriptorSet(sharedCacheKey);

                ImageLayout imageLayout = textureVk->getImage().getCurrentImageLayout();

                SetBitField(infoDesc.imageLayoutOrRange, imageLayout);
                infoDesc.imageViewSerialOrOffset = imageViewSerial.viewSerial.getValue();
                infoDesc.samplerOrBufferSerial   = samplerHelper.getSamplerSerial().getValue();
                memcpy(&infoDesc.imageSubresourceRange, &imageViewSerial.subresource,
                       sizeof(uint32_t));

                mHandles[infoIndex].sampler = samplerHelper.get().getHandle();

                // __samplerExternal2DY2YEXT cannot be used with
                // emulateSeamfulCubeMapSampling because that's only enabled in GLES == 2.
                // Use the read image view here anyway.
                if (emulateSeamfulCubeMapSampling && !isSamplerExternalY2Y)
                {
                    // If emulating seamful cube mapping, use the fetch image view.  This is
                    // basically the same image view as read, except it's a 2DArray view for
                    // cube maps.
                    const ImageView &imageView = textureVk->getFetchImageView(
                        context, samplerState.getSRGBDecode(), samplerUniform.texelFetchStaticUse);
                    mHandles[infoIndex].imageView = imageView.getHandle();
                }
                else
                {
                    const ImageView &imageView = textureVk->getReadImageView(
                        context, samplerState.getSRGBDecode(), samplerUniform.texelFetchStaticUse,
                        isSamplerExternalY2Y);
                    mHandles[infoIndex].imageView = imageView.getHandle();
                }
            }

            mDesc.updateInfoDesc(infoIndex, infoDesc);
        }
    }

    return angle::Result::Continue;
}

void DescriptorSetDescBuilder::updateShaderBuffers(
    gl::ShaderType shaderType,
    ShaderVariableType variableType,
    const ShaderInterfaceVariableInfoMap &variableInfoMap,
    const gl::BufferVector &buffers,
    const std::vector<gl::InterfaceBlock> &blocks,
    VkDescriptorType descriptorType,
    VkDeviceSize maxBoundBufferRange,
    const BufferHelper &emptyBuffer)
{
    // Initialize the descriptor writes in a first pass. This ensures we can pack the structures
    // corresponding to array elements tightly.
    for (uint32_t bufferIndex = 0; bufferIndex < blocks.size(); ++bufferIndex)
    {
        const gl::InterfaceBlock &block = blocks[bufferIndex];

        if (!block.isActive(shaderType))
        {
            continue;
        }

        const ShaderInterfaceVariableInfo &info =
            variableInfoMap.getIndexedVariableInfo(shaderType, variableType, bufferIndex);
        if (info.isDuplicate)
        {
            continue;
        }

        if (block.isArray && block.arrayElement > 0)
        {
            mDesc.incrementDescriptorCount(info.binding, 1);
            mCurrentInfoIndex++;
        }
        else
        {
            updateWriteDesc(info.binding, descriptorType, 1);
        }
    }

    // Now that we have the proper array elements counts, initialize the info structures.
    for (uint32_t bufferIndex = 0; bufferIndex < blocks.size(); ++bufferIndex)
    {
        const gl::InterfaceBlock &block                           = blocks[bufferIndex];
        const gl::OffsetBindingPointer<gl::Buffer> &bufferBinding = buffers[block.binding];

        if (!block.isActive(shaderType))
        {
            continue;
        }

        const ShaderInterfaceVariableInfo &info =
            variableInfoMap.getIndexedVariableInfo(shaderType, variableType, bufferIndex);
        if (info.isDuplicate)
        {
            continue;
        }

        uint32_t binding       = info.binding;
        uint32_t arrayElement  = block.isArray ? block.arrayElement : 0;
        uint32_t infoDescIndex = mDesc.getInfoDescIndex(binding) + arrayElement;

        if (bufferBinding.get() == nullptr)
        {
            DescriptorInfoDesc emptyDesc = {};
            SetBitField(emptyDesc.imageLayoutOrRange, emptyBuffer.getSize());
            emptyDesc.imageViewSerialOrOffset = 0;
            emptyDesc.samplerOrBufferSerial   = emptyBuffer.getBlockSerial().getValue();

            mDesc.updateInfoDesc(infoDescIndex, emptyDesc);

            mHandles[infoDescIndex].buffer = emptyBuffer.getBuffer().getHandle();

            if (IsDynamicDescriptor(descriptorType))
            {
                mDynamicOffsets[infoDescIndex] = 0;
            }
        }
        else
        {
            // Limit bound buffer size to maximum resource binding size.
            GLsizeiptr boundBufferSize = gl::GetBoundBufferAvailableSize(bufferBinding);
            VkDeviceSize size = std::min<VkDeviceSize>(boundBufferSize, maxBoundBufferRange);

            // Make sure there's no possible under/overflow with binding size.
            static_assert(sizeof(VkDeviceSize) >= sizeof(bufferBinding.getSize()),
                          "VkDeviceSize too small");
            ASSERT(bufferBinding.getSize() >= 0);

            BufferVk *bufferVk         = vk::GetImpl(bufferBinding.get());
            BufferHelper &bufferHelper = bufferVk->getBuffer();

            DescriptorInfoDesc infoDesc = {};
            SetBitField(infoDesc.imageLayoutOrRange, size);
            infoDesc.samplerOrBufferSerial = bufferHelper.getBlockSerial().getValue();

            VkDeviceSize offset = bufferBinding.getOffset() + bufferHelper.getOffset();

            if (IsDynamicDescriptor(descriptorType))
            {
                SetBitField(mDynamicOffsets[infoDescIndex], offset);
                mUsedBufferBlocks.emplace_back(bufferHelper.getBufferBlock());
            }
            else
            {
                SetBitField(infoDesc.imageViewSerialOrOffset, offset);
                mUsedBufferHelpers.emplace_back(&bufferHelper);
            }

            mDesc.updateInfoDesc(infoDescIndex, infoDesc);

            mHandles[infoDescIndex].buffer = bufferHelper.getBuffer().getHandle();
        }
    }
}

void DescriptorSetDescBuilder::updateAtomicCounters(
    gl::ShaderType shaderType,
    const ShaderInterfaceVariableInfoMap &variableInfoMap,
    const gl::BufferVector &buffers,
    const std::vector<gl::AtomicCounterBuffer> &atomicCounterBuffers,
    const VkDeviceSize requiredOffsetAlignment,
    vk::BufferHelper *emptyBuffer)
{
    if (atomicCounterBuffers.empty() || !variableInfoMap.hasAtomicCounterInfo(shaderType))
    {
        return;
    }

    static_assert(!IsDynamicDescriptor(kStorageBufferDescriptorType),
                  "This method needs an update to handle dynamic descriptors");

    uint32_t binding = variableInfoMap.getAtomicCounterBufferBinding(shaderType, 0);

    updateWriteDesc(binding, kStorageBufferDescriptorType,
                    gl::IMPLEMENTATION_MAX_ATOMIC_COUNTER_BUFFER_BINDINGS);

    uint32_t baseInfoIndex = mDesc.getInfoDescIndex(binding);

    DescriptorInfoDesc emptyDesc = {};
    SetBitField(emptyDesc.imageLayoutOrRange, emptyBuffer->getSize());
    emptyDesc.imageViewSerialOrOffset = 0;
    emptyDesc.samplerOrBufferSerial   = emptyBuffer->getBlockSerial().getValue();

    // Bind the empty buffer to every array slot that's unused.
    for (uint32_t arrayElement = 0;
         arrayElement < gl::IMPLEMENTATION_MAX_ATOMIC_COUNTER_BUFFER_BINDINGS; ++arrayElement)
    {
        uint32_t infoIndex = baseInfoIndex + arrayElement;
        mDesc.updateInfoDesc(infoIndex, emptyDesc);
        mHandles[infoIndex].buffer = emptyBuffer->getBuffer().getHandle();
    }

    for (const gl::AtomicCounterBuffer &atomicCounterBuffer : atomicCounterBuffers)
    {
        int arrayElement                                          = atomicCounterBuffer.binding;
        const gl::OffsetBindingPointer<gl::Buffer> &bufferBinding = buffers[arrayElement];

        uint32_t infoIndex = baseInfoIndex + arrayElement;

        if (bufferBinding.get() == nullptr)
        {
            mDesc.updateInfoDesc(infoIndex, emptyDesc);
            mHandles[infoIndex].buffer = emptyBuffer->getBuffer().getHandle();
            continue;
        }

        BufferVk *bufferVk             = vk::GetImpl(bufferBinding.get());
        vk::BufferHelper &bufferHelper = bufferVk->getBuffer();

        VkDeviceSize offset = bufferBinding.getOffset() + bufferHelper.getOffset();

        VkDeviceSize alignedOffset = (offset / requiredOffsetAlignment) * requiredOffsetAlignment;
        VkDeviceSize offsetDiff    = offset - alignedOffset;

        offset = alignedOffset;

        VkDeviceSize range = gl::GetBoundBufferAvailableSize(bufferBinding) + offsetDiff;

        DescriptorInfoDesc infoDesc = {};
        SetBitField(infoDesc.imageLayoutOrRange, range);
        SetBitField(infoDesc.imageViewSerialOrOffset, offset);
        infoDesc.samplerOrBufferSerial = bufferHelper.getBlockSerial().getValue();
        mUsedBufferHelpers.emplace_back(&bufferHelper);

        mDesc.updateInfoDesc(infoIndex, infoDesc);
        mHandles[infoIndex].buffer = bufferHelper.getBuffer().getHandle();
    }
}

angle::Result DescriptorSetDescBuilder::updateImages(
    Context *context,
    gl::ShaderType shaderType,
    const gl::ProgramExecutable &executable,
    const ShaderInterfaceVariableInfoMap &variableInfoMap,
    const gl::ActiveTextureArray<TextureVk *> &activeImages,
    const std::vector<gl::ImageUnit> &imageUnits)
{
    RendererVk *renderer                               = context->getRenderer();
    const std::vector<gl::ImageBinding> &imageBindings = executable.getImageBindings();
    const std::vector<gl::LinkedUniform> &uniforms     = executable.getUniforms();

    if (imageBindings.empty())
    {
        return angle::Result::Continue;
    }

    for (uint32_t imageIndex = 0; imageIndex < imageBindings.size(); ++imageIndex)
    {
        const gl::ImageBinding &imageBinding = imageBindings[imageIndex];
        uint32_t uniformIndex                = executable.getUniformIndexFromImageIndex(imageIndex);
        const gl::LinkedUniform &imageUniform = uniforms[uniformIndex];

        if (!imageUniform.isActive(shaderType))
        {
            continue;
        }

        const ShaderInterfaceVariableInfo &info = variableInfoMap.getIndexedVariableInfo(
            shaderType, ShaderVariableType::Image, imageIndex);
        if (info.isDuplicate)
        {
            continue;
        }

        uint32_t arraySize       = static_cast<uint32_t>(imageBinding.boundImageUnits.size());
        uint32_t descriptorCount = arraySize * gl::ArraySizeProduct(imageUniform.outerArraySizes);
        VkDescriptorType descriptorType = (imageBinding.textureType == gl::TextureType::Buffer)
                                              ? VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER
                                              : VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;

        updateWriteDesc(info.binding, descriptorType, descriptorCount);

        // Texture buffers use buffer views, so they are especially handled.
        if (imageBinding.textureType == gl::TextureType::Buffer)
        {
            // Handle format reinterpretation by looking for a view with the format specified in
            // the shader (if any, instead of the format specified to glTexBuffer).
            const vk::Format *format = nullptr;
            if (imageUniform.imageUnitFormat != GL_NONE)
            {
                format = &renderer->getFormat(imageUniform.imageUnitFormat);
            }

            for (uint32_t arrayElement = 0; arrayElement < arraySize; ++arrayElement)
            {
                GLuint imageUnit     = imageBinding.boundImageUnits[arrayElement];
                TextureVk *textureVk = activeImages[imageUnit];

                uint32_t infoIndex = mDesc.getInfoDescIndex(info.binding) + arrayElement +
                                     imageUniform.outerArrayOffset;

                const vk::BufferView *view = nullptr;
                ANGLE_TRY(textureVk->getBufferViewAndRecordUse(context, format, true, &view));

                DescriptorInfoDesc infoDesc = {};
                infoDesc.imageViewSerialOrOffset =
                    textureVk->getBufferViewSerial().viewSerial.getValue();
                mUsedImages.emplace_back(textureVk);

                mDesc.updateInfoDesc(infoIndex, infoDesc);
                mHandles[infoIndex].bufferView = view->getHandle();
            }
        }
        else
        {
            for (uint32_t arrayElement = 0; arrayElement < arraySize; ++arrayElement)
            {
                GLuint imageUnit             = imageBinding.boundImageUnits[arrayElement];
                const gl::ImageUnit &binding = imageUnits[imageUnit];
                TextureVk *textureVk         = activeImages[imageUnit];

                vk::ImageHelper *image         = &textureVk->getImage();
                const vk::ImageView *imageView = nullptr;

                vk::ImageOrBufferViewSubresourceSerial serial =
                    textureVk->getStorageImageViewSerial(binding);

                ANGLE_TRY(textureVk->getStorageImageView(context, binding, &imageView));

                uint32_t infoIndex = mDesc.getInfoDescIndex(info.binding) + arrayElement +
                                     imageUniform.outerArrayOffset;

                // Note: binding.access is unused because it is implied by the shader.

                DescriptorInfoDesc infoDesc = {};
                SetBitField(infoDesc.imageLayoutOrRange, image->getCurrentImageLayout());
                memcpy(&infoDesc.imageSubresourceRange, &serial.subresource, sizeof(uint32_t));
                infoDesc.imageViewSerialOrOffset = serial.viewSerial.getValue();
                mUsedImages.emplace_back(textureVk);

                mDesc.updateInfoDesc(infoIndex, infoDesc);
                mHandles[infoIndex].imageView = imageView->getHandle();
            }
        }
    }

    return angle::Result::Continue;
}

angle::Result DescriptorSetDescBuilder::updateInputAttachments(
    vk::Context *context,
    gl::ShaderType shaderType,
    const gl::ProgramExecutable &executable,
    const ShaderInterfaceVariableInfoMap &variableInfoMap,
    FramebufferVk *framebufferVk)
{
    if (shaderType != gl::ShaderType::Fragment)
    {
        return angle::Result::Continue;
    }

    const std::vector<gl::LinkedUniform> &uniforms = executable.getUniforms();

    if (!executable.usesFramebufferFetch())
    {
        return angle::Result::Continue;
    }

    const uint32_t baseUniformIndex              = executable.getFragmentInoutRange().low();
    const gl::LinkedUniform &baseInputAttachment = uniforms.at(baseUniformIndex);
    std::string baseMappedName                   = baseInputAttachment.mappedName;

    const ShaderInterfaceVariableInfo &baseInfo =
        variableInfoMap.getFramebufferFetchInfo(shaderType);
    if (baseInfo.isDuplicate)
    {
        return angle::Result::Continue;
    }

    uint32_t baseBinding = baseInfo.binding - baseInputAttachment.location;

    for (size_t colorIndex : framebufferVk->getState().getColorAttachmentsMask())
    {
        uint32_t binding = baseBinding + static_cast<uint32_t>(colorIndex);
        updateWriteDesc(binding, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1);

        RenderTargetVk *renderTargetVk = framebufferVk->getColorDrawRenderTarget(colorIndex);
        const vk::ImageView *imageView = nullptr;

        ANGLE_TRY(renderTargetVk->getImageView(context, &imageView));

        uint32_t infoIndex = mDesc.getInfoDescIndex(binding);

        DescriptorInfoDesc infoDesc = {};

        // We just need any layout that represents GENERAL. The serial is also not totally precise.
        ImageOrBufferViewSubresourceSerial serial = renderTargetVk->getDrawSubresourceSerial();
        SetBitField(infoDesc.imageLayoutOrRange, ImageLayout::FragmentShaderWrite);
        infoDesc.imageViewSerialOrOffset = serial.viewSerial.getValue();
        memcpy(&infoDesc.imageSubresourceRange, &serial.subresource, sizeof(uint32_t));

        mDesc.updateInfoDesc(infoIndex, infoDesc);
        mHandles[infoIndex].imageView = imageView->getHandle();
    }

    return angle::Result::Continue;
}

void DescriptorSetDescBuilder::updateDescriptorSet(Context *context,
                                                   UpdateDescriptorSetsBuilder *updateBuilder,
                                                   VkDescriptorSet descriptorSet) const
{
    mDesc.updateDescriptorSet(context, updateBuilder, mHandles.data(), descriptorSet);
}

void DescriptorSetDescBuilder::updateImagesAndBuffersWithSharedCacheKey(
    const SharedDescriptorSetCacheKey &sharedCacheKey)
{
    if (sharedCacheKey)
    {
        // A new cache entry has been created. We record this cache key in the images and buffers so
        // that the descriptorSet cache can be destroyed when buffer/image is destroyed.
        for (TextureVk *image : mUsedImages)
        {
            image->onNewDescriptorSet(sharedCacheKey);
        }
        for (BufferBlock *bufferBlock : mUsedBufferBlocks)
        {
            bufferBlock->onNewDescriptorSet(sharedCacheKey);
        }
        for (BufferHelper *bufferHelper : mUsedBufferHelpers)
        {
            bufferHelper->onNewDescriptorSet(sharedCacheKey);
        }
    }

    mUsedImages.clear();
    mUsedBufferBlocks.clear();
    mUsedBufferHelpers.clear();
}

// SharedCacheKeyManager implementation.
template <class SharedCacheKeyT>
void SharedCacheKeyManager<SharedCacheKeyT>::addKey(const SharedCacheKeyT &key)
{
    // If there is invalid key in the array, use it instead of keep expanding the array
    for (SharedCacheKeyT &sharedCacheKey : mSharedCacheKeys)
    {
        if (*sharedCacheKey.get() == nullptr)
        {
            sharedCacheKey = key;
            return;
        }
    }
    mSharedCacheKeys.emplace_back(key);
}

template <class SharedCacheKeyT>
void SharedCacheKeyManager<SharedCacheKeyT>::releaseKeys(ContextVk *contextVk)
{
    for (SharedCacheKeyT &sharedCacheKey : mSharedCacheKeys)
    {
        if (*sharedCacheKey.get() != nullptr)
        {
            // Immediate destroy the cached object and the key itself when first releaseRef call is
            // made
            ReleaseCachedObject(contextVk, *(*sharedCacheKey.get()));
            *sharedCacheKey.get() = nullptr;
        }
    }
    mSharedCacheKeys.clear();
}

template <class SharedCacheKeyT>
void SharedCacheKeyManager<SharedCacheKeyT>::destroyKeys(RendererVk *renderer)
{
    for (SharedCacheKeyT &sharedCacheKey : mSharedCacheKeys)
    {
        // destroy the cache key
        if (*sharedCacheKey.get() != nullptr)
        {
            // Immediate destroy the cached object and the key
            DestroyCachedObject(renderer, *(*sharedCacheKey.get()));
            *sharedCacheKey.get() = nullptr;
        }
    }
    mSharedCacheKeys.clear();
}

template <class SharedCacheKeyT>
void SharedCacheKeyManager<SharedCacheKeyT>::clear()
{
    // Caller must have already freed all caches
    assertAllEntriesDestroyed();
    mSharedCacheKeys.clear();
}

template <class SharedCacheKeyT>
bool SharedCacheKeyManager<SharedCacheKeyT>::containsKey(const SharedCacheKeyT &key) const
{
    for (const SharedCacheKeyT &sharedCacheKey : mSharedCacheKeys)
    {
        if (key == sharedCacheKey)
        {
            return true;
        }
    }
    return false;
}

template <class SharedCacheKeyT>
void SharedCacheKeyManager<SharedCacheKeyT>::assertAllEntriesDestroyed()
{
    // Caller must have already freed all caches
    for (SharedCacheKeyT &sharedCacheKey : mSharedCacheKeys)
    {
        ASSERT(*sharedCacheKey.get() == nullptr);
    }
}

// Explict instantiate for FramebufferCacheManager
template class SharedCacheKeyManager<SharedFramebufferCacheKey>;
// Explict instantiate for DescriptorSetCacheManager
template class SharedCacheKeyManager<SharedDescriptorSetCacheKey>;

// PipelineCacheAccess implementation.
std::unique_lock<std::mutex> PipelineCacheAccess::getLock()
{
    if (mMutex == nullptr)
    {
        return std::unique_lock<std::mutex>();
    }

    return std::unique_lock<std::mutex>(*mMutex);
}

VkResult PipelineCacheAccess::createGraphicsPipeline(vk::Context *context,
                                                     const VkGraphicsPipelineCreateInfo &createInfo,
                                                     vk::Pipeline *pipelineOut)
{
    std::unique_lock<std::mutex> lock = getLock();

    return pipelineOut->initGraphics(context->getDevice(), createInfo, *mPipelineCache);
}

VkResult PipelineCacheAccess::createComputePipeline(vk::Context *context,
                                                    const VkComputePipelineCreateInfo &createInfo,
                                                    vk::Pipeline *pipelineOut)
{
    std::unique_lock<std::mutex> lock = getLock();

    return pipelineOut->initCompute(context->getDevice(), createInfo, *mPipelineCache);
}

void PipelineCacheAccess::merge(RendererVk *renderer, const vk::PipelineCache &pipelineCache)
{
    ASSERT(mMutex != nullptr);

    std::unique_lock<std::mutex> lock = getLock();

    mPipelineCache->merge(renderer->getDevice(), 1, pipelineCache.ptr());
}
}  // namespace vk

// FramebufferCache implementation.
void FramebufferCache::destroy(RendererVk *rendererVk)
{
    rendererVk->accumulateCacheStats(VulkanCacheType::Framebuffer, mCacheStats);
    for (auto &entry : mPayload)
    {
        vk::FramebufferHelper &tmpFB = entry.second;
        tmpFB.destroy(rendererVk);
    }
    mPayload.clear();
}

bool FramebufferCache::get(ContextVk *contextVk,
                           const vk::FramebufferDesc &desc,
                           vk::Framebuffer &framebuffer)
{
    ASSERT(!contextVk->getFeatures().supportsImagelessFramebuffer.enabled);

    auto iter = mPayload.find(desc);
    if (iter != mPayload.end())
    {
        framebuffer.setHandle(iter->second.getFramebuffer().getHandle());
        mCacheStats.hit();
        return true;
    }

    mCacheStats.miss();
    return false;
}

void FramebufferCache::insert(ContextVk *contextVk,
                              const vk::FramebufferDesc &desc,
                              vk::FramebufferHelper &&framebufferHelper)
{
    ASSERT(!contextVk->getFeatures().supportsImagelessFramebuffer.enabled);

    mPayload.emplace(desc, std::move(framebufferHelper));
}

void FramebufferCache::erase(ContextVk *contextVk, const vk::FramebufferDesc &desc)
{
    ASSERT(!contextVk->getFeatures().supportsImagelessFramebuffer.enabled);

    auto iter = mPayload.find(desc);
    if (iter != mPayload.end())
    {
        vk::FramebufferHelper &tmpFB = iter->second;
        tmpFB.release(contextVk);
        mPayload.erase(desc);
    }
}

// RenderPassCache implementation.
RenderPassCache::RenderPassCache() = default;

RenderPassCache::~RenderPassCache()
{
    ASSERT(mPayload.empty());
}

void RenderPassCache::destroy(ContextVk *contextVk)
{
    RendererVk *renderer = contextVk->getRenderer();

    renderer->accumulateCacheStats(VulkanCacheType::CompatibleRenderPass,
                                   mCompatibleRenderPassCacheStats);
    renderer->accumulateCacheStats(VulkanCacheType::RenderPassWithOps,
                                   mRenderPassWithOpsCacheStats);

    VkDevice device = renderer->getDevice();

    // Make sure there are no jobs referencing the render pass cache.
    contextVk->getShareGroup()->waitForCurrentMonolithicPipelineCreationTask();

    for (auto &outerIt : mPayload)
    {
        for (auto &innerIt : outerIt.second)
        {
            innerIt.second.destroy(device);
        }
    }
    mPayload.clear();
}

void RenderPassCache::clear(ContextVk *contextVk)
{
    // Make sure there are no jobs referencing the render pass cache.
    contextVk->getShareGroup()->waitForCurrentMonolithicPipelineCreationTask();

    for (auto &outerIt : mPayload)
    {
        for (auto &innerIt : outerIt.second)
        {
            innerIt.second.release(contextVk);
        }
    }
    mPayload.clear();
}

angle::Result RenderPassCache::addCompatibleRenderPass(ContextVk *contextVk,
                                                       const vk::RenderPassDesc &desc,
                                                       const vk::RenderPass **renderPassOut)
{
    // This API is only used by getCompatibleRenderPass() to create a compatible render pass.  The
    // following does not participate in render pass compatibility, so could take any value:
    //
    // - Load and store ops
    // - Attachment layouts
    // - Existance of resolve attachment (if single subpass)
    //
    // The values chosen here are arbitrary.
    //
    vk::AttachmentOpsArray ops;

    vk::PackedAttachmentIndex colorIndexVk(0);
    for (uint32_t colorIndexGL = 0; colorIndexGL < desc.colorAttachmentRange(); ++colorIndexGL)
    {
        if (!desc.isColorAttachmentEnabled(colorIndexGL))
        {
            continue;
        }

        const vk::ImageLayout imageLayout = vk::ImageLayout::ColorWrite;
        ops.initWithLoadStore(colorIndexVk, imageLayout, imageLayout);
        ++colorIndexVk;
    }

    if (desc.hasDepthStencilAttachment())
    {
        const vk::ImageLayout imageLayout = vk::ImageLayout::DepthWriteStencilWrite;
        ops.initWithLoadStore(colorIndexVk, imageLayout, imageLayout);
    }

    return getRenderPassWithOpsImpl(contextVk, desc, ops, false, renderPassOut);
}

angle::Result RenderPassCache::getRenderPassWithOps(ContextVk *contextVk,
                                                    const vk::RenderPassDesc &desc,
                                                    const vk::AttachmentOpsArray &attachmentOps,
                                                    const vk::RenderPass **renderPassOut)
{
    return getRenderPassWithOpsImpl(contextVk, desc, attachmentOps, true, renderPassOut);
}

angle::Result RenderPassCache::getRenderPassWithOpsImpl(ContextVk *contextVk,
                                                        const vk::RenderPassDesc &desc,
                                                        const vk::AttachmentOpsArray &attachmentOps,
                                                        bool updatePerfCounters,
                                                        const vk::RenderPass **renderPassOut)
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
            mRenderPassWithOpsCacheStats.hit();
            return angle::Result::Continue;
        }
    }
    else
    {
        auto emplaceResult = mPayload.emplace(desc, InnerCache());
        outerIt            = emplaceResult.first;
    }

    mRenderPassWithOpsCacheStats.missAndIncrementSize();
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
template <typename Hash>
void GraphicsPipelineCache<Hash>::destroy(ContextVk *contextVk)
{
    if (kDumpPipelineCacheGraph && !mPayload.empty())
    {
        vk::DumpPipelineCacheGraph<Hash>(contextVk, mPayload);
    }

    accumulateCacheStats(contextVk->getRenderer());

    VkDevice device = contextVk->getDevice();

    for (auto &item : mPayload)
    {
        vk::PipelineHelper &pipeline = item.second;
        pipeline.destroy(device);
    }

    mPayload.clear();
}

template <typename Hash>
void GraphicsPipelineCache<Hash>::release(ContextVk *contextVk)
{
    if (kDumpPipelineCacheGraph && !mPayload.empty())
    {
        vk::DumpPipelineCacheGraph<Hash>(contextVk, mPayload);
    }

    for (auto &item : mPayload)
    {
        vk::PipelineHelper &pipeline = item.second;
        pipeline.release(contextVk);
    }

    mPayload.clear();
}

template <typename Hash>
angle::Result GraphicsPipelineCache<Hash>::createPipeline(
    ContextVk *contextVk,
    vk::PipelineCacheAccess *pipelineCache,
    const vk::RenderPass &compatibleRenderPass,
    const vk::PipelineLayout &pipelineLayout,
    const vk::ShaderModuleMap &shaders,
    const vk::SpecializationConstants &specConsts,
    PipelineSource source,
    const vk::GraphicsPipelineDesc &desc,
    const vk::GraphicsPipelineDesc **descPtrOut,
    vk::PipelineHelper **pipelineOut)
{
    vk::Pipeline newPipeline;
    vk::CacheLookUpFeedback feedback = vk::CacheLookUpFeedback::None;

    // This "if" is left here for the benefit of VulkanPipelineCachePerfTest.
    if (contextVk != nullptr)
    {
        constexpr vk::GraphicsPipelineSubset kSubset =
            GraphicsPipelineCacheTypeHelper<Hash>::kSubset;

        ANGLE_VK_TRY(contextVk, desc.initializePipeline(
                                    contextVk, pipelineCache, kSubset, compatibleRenderPass,
                                    pipelineLayout, shaders, specConsts, &newPipeline, &feedback));
    }

    addToCache(source, desc, std::move(newPipeline), feedback, descPtrOut, pipelineOut);
    return angle::Result::Continue;
}

template <typename Hash>
angle::Result GraphicsPipelineCache<Hash>::linkLibraries(
    ContextVk *contextVk,
    vk::PipelineCacheAccess *pipelineCache,
    const vk::GraphicsPipelineDesc &desc,
    const vk::PipelineLayout &pipelineLayout,
    vk::PipelineHelper *vertexInputPipeline,
    vk::PipelineHelper *shadersPipeline,
    vk::PipelineHelper *fragmentOutputPipeline,
    const vk::GraphicsPipelineDesc **descPtrOut,
    vk::PipelineHelper **pipelineOut)
{
    vk::Pipeline newPipeline;
    vk::CacheLookUpFeedback feedback = vk::CacheLookUpFeedback::None;

    ANGLE_TRY(vk::InitializePipelineFromLibraries(
        contextVk, pipelineCache, pipelineLayout, *vertexInputPipeline, *shadersPipeline,
        *fragmentOutputPipeline, &newPipeline, &feedback));

    addToCache(PipelineSource::DrawLinked, desc, std::move(newPipeline), feedback, descPtrOut,
               pipelineOut);
    (*pipelineOut)->setLinkedLibraryReferences(shadersPipeline);

    return angle::Result::Continue;
}

template <typename Hash>
void GraphicsPipelineCache<Hash>::addToCache(PipelineSource source,
                                             const vk::GraphicsPipelineDesc &desc,
                                             vk::Pipeline &&pipeline,
                                             vk::CacheLookUpFeedback feedback,
                                             const vk::GraphicsPipelineDesc **descPtrOut,
                                             vk::PipelineHelper **pipelineOut)
{
    ASSERT(mPayload.find(desc) == mPayload.end());

    mCacheStats.missAndIncrementSize();

    switch (source)
    {
        case PipelineSource::WarmUp:
            feedback = feedback == vk::CacheLookUpFeedback::Hit
                           ? vk::CacheLookUpFeedback::WarmUpHit
                           : vk::CacheLookUpFeedback::WarmUpMiss;
            break;
        case PipelineSource::DrawLinked:
            feedback = feedback == vk::CacheLookUpFeedback::Hit
                           ? vk::CacheLookUpFeedback::LinkedDrawHit
                           : vk::CacheLookUpFeedback::LinkedDrawMiss;
            break;
        case PipelineSource::Utils:
            feedback = feedback == vk::CacheLookUpFeedback::Hit
                           ? vk::CacheLookUpFeedback::UtilsHit
                           : vk::CacheLookUpFeedback::UtilsMiss;
            break;
        default:
            break;
    }

    auto insertedItem = mPayload.emplace(std::piecewise_construct, std::forward_as_tuple(desc),
                                         std::forward_as_tuple(std::move(pipeline), feedback));
    *descPtrOut       = &insertedItem.first->first;
    *pipelineOut      = &insertedItem.first->second;
}

template <typename Hash>
void GraphicsPipelineCache<Hash>::populate(const vk::GraphicsPipelineDesc &desc,
                                           vk::Pipeline &&pipeline)
{
    auto item = mPayload.find(desc);
    if (item != mPayload.end())
    {
        return;
    }

    // Note: this function is only used for testing.
    mPayload.emplace(std::piecewise_construct, std::forward_as_tuple(desc),
                     std::forward_as_tuple(std::move(pipeline), vk::CacheLookUpFeedback::None));
}

// Instantiate the pipeline cache functions
template void GraphicsPipelineCache<GraphicsPipelineDescCompleteHash>::destroy(
    ContextVk *contextVk);
template void GraphicsPipelineCache<GraphicsPipelineDescCompleteHash>::release(
    ContextVk *contextVk);
template angle::Result GraphicsPipelineCache<GraphicsPipelineDescCompleteHash>::createPipeline(
    ContextVk *contextVk,
    vk::PipelineCacheAccess *pipelineCache,
    const vk::RenderPass &compatibleRenderPass,
    const vk::PipelineLayout &pipelineLayout,
    const vk::ShaderModuleMap &shaders,
    const vk::SpecializationConstants &specConsts,
    PipelineSource source,
    const vk::GraphicsPipelineDesc &desc,
    const vk::GraphicsPipelineDesc **descPtrOut,
    vk::PipelineHelper **pipelineOut);
template angle::Result GraphicsPipelineCache<GraphicsPipelineDescCompleteHash>::linkLibraries(
    ContextVk *contextVk,
    vk::PipelineCacheAccess *pipelineCache,
    const vk::GraphicsPipelineDesc &desc,
    const vk::PipelineLayout &pipelineLayout,
    vk::PipelineHelper *vertexInputPipeline,
    vk::PipelineHelper *shadersPipeline,
    vk::PipelineHelper *fragmentOutputPipeline,
    const vk::GraphicsPipelineDesc **descPtrOut,
    vk::PipelineHelper **pipelineOut);
template void GraphicsPipelineCache<GraphicsPipelineDescCompleteHash>::populate(
    const vk::GraphicsPipelineDesc &desc,
    vk::Pipeline &&pipeline);

template void GraphicsPipelineCache<GraphicsPipelineDescVertexInputHash>::destroy(
    ContextVk *contextVk);
template void GraphicsPipelineCache<GraphicsPipelineDescVertexInputHash>::release(
    ContextVk *contextVk);
template angle::Result GraphicsPipelineCache<GraphicsPipelineDescVertexInputHash>::createPipeline(
    ContextVk *contextVk,
    vk::PipelineCacheAccess *pipelineCache,
    const vk::RenderPass &compatibleRenderPass,
    const vk::PipelineLayout &pipelineLayout,
    const vk::ShaderModuleMap &shaders,
    const vk::SpecializationConstants &specConsts,
    PipelineSource source,
    const vk::GraphicsPipelineDesc &desc,
    const vk::GraphicsPipelineDesc **descPtrOut,
    vk::PipelineHelper **pipelineOut);
template void GraphicsPipelineCache<GraphicsPipelineDescVertexInputHash>::populate(
    const vk::GraphicsPipelineDesc &desc,
    vk::Pipeline &&pipeline);

template void GraphicsPipelineCache<GraphicsPipelineDescShadersHash>::destroy(ContextVk *contextVk);
template void GraphicsPipelineCache<GraphicsPipelineDescShadersHash>::release(ContextVk *contextVk);
template angle::Result GraphicsPipelineCache<GraphicsPipelineDescShadersHash>::createPipeline(
    ContextVk *contextVk,
    vk::PipelineCacheAccess *pipelineCache,
    const vk::RenderPass &compatibleRenderPass,
    const vk::PipelineLayout &pipelineLayout,
    const vk::ShaderModuleMap &shaders,
    const vk::SpecializationConstants &specConsts,
    PipelineSource source,
    const vk::GraphicsPipelineDesc &desc,
    const vk::GraphicsPipelineDesc **descPtrOut,
    vk::PipelineHelper **pipelineOut);
template void GraphicsPipelineCache<GraphicsPipelineDescShadersHash>::populate(
    const vk::GraphicsPipelineDesc &desc,
    vk::Pipeline &&pipeline);

template void GraphicsPipelineCache<GraphicsPipelineDescFragmentOutputHash>::destroy(
    ContextVk *contextVk);
template void GraphicsPipelineCache<GraphicsPipelineDescFragmentOutputHash>::release(
    ContextVk *contextVk);
template angle::Result
GraphicsPipelineCache<GraphicsPipelineDescFragmentOutputHash>::createPipeline(
    ContextVk *contextVk,
    vk::PipelineCacheAccess *pipelineCache,
    const vk::RenderPass &compatibleRenderPass,
    const vk::PipelineLayout &pipelineLayout,
    const vk::ShaderModuleMap &shaders,
    const vk::SpecializationConstants &specConsts,
    PipelineSource source,
    const vk::GraphicsPipelineDesc &desc,
    const vk::GraphicsPipelineDesc **descPtrOut,
    vk::PipelineHelper **pipelineOut);
template void GraphicsPipelineCache<GraphicsPipelineDescFragmentOutputHash>::populate(
    const vk::GraphicsPipelineDesc &desc,
    vk::Pipeline &&pipeline);

// DescriptorSetLayoutCache implementation.
DescriptorSetLayoutCache::DescriptorSetLayoutCache() = default;

DescriptorSetLayoutCache::~DescriptorSetLayoutCache()
{
    ASSERT(mPayload.empty());
}

void DescriptorSetLayoutCache::destroy(RendererVk *rendererVk)
{
    rendererVk->accumulateCacheStats(VulkanCacheType::DescriptorSetLayout, mCacheStats);

    VkDevice device = rendererVk->getDevice();

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
        mCacheStats.hit();
        return angle::Result::Continue;
    }

    mCacheStats.missAndIncrementSize();
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

void PipelineLayoutCache::destroy(RendererVk *rendererVk)
{
    accumulateCacheStats(rendererVk);

    VkDevice device = rendererVk->getDevice();

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
        mCacheStats.hit();
        return angle::Result::Continue;
    }

    mCacheStats.missAndIncrementSize();
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

    const vk::PackedPushConstantRange &descPushConstantRange = desc.getPushConstantRange();
    VkPushConstantRange pushConstantRange;
    pushConstantRange.stageFlags = descPushConstantRange.stageMask;
    pushConstantRange.offset     = descPushConstantRange.offset;
    pushConstantRange.size       = descPushConstantRange.size;

    // No pipeline layout found. We must create a new one.
    VkPipelineLayoutCreateInfo createInfo = {};
    createInfo.sType                      = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    createInfo.flags                      = 0;
    createInfo.setLayoutCount             = static_cast<uint32_t>(setLayoutHandles.size());
    createInfo.pSetLayouts                = setLayoutHandles.data();
    if (pushConstantRange.size > 0)
    {
        createInfo.pushConstantRangeCount = 1;
        createInfo.pPushConstantRanges    = &pushConstantRange;
    }

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
    ASSERT(mExternalFormatPayload.empty() && mVkFormatPayload.empty());
}

void SamplerYcbcrConversionCache::destroy(RendererVk *rendererVk)
{
    rendererVk->accumulateCacheStats(VulkanCacheType::SamplerYcbcrConversion, mCacheStats);

    VkDevice device = rendererVk->getDevice();

    for (auto &iter : mExternalFormatPayload)
    {
        vk::SamplerYcbcrConversion &samplerYcbcrConversion = iter.second;
        samplerYcbcrConversion.destroy(device);

        rendererVk->onDeallocateHandle(vk::HandleType::SamplerYcbcrConversion);
    }

    for (auto &iter : mVkFormatPayload)
    {
        vk::SamplerYcbcrConversion &samplerYcbcrConversion = iter.second;
        samplerYcbcrConversion.destroy(device);

        rendererVk->onDeallocateHandle(vk::HandleType::SamplerYcbcrConversion);
    }

    mExternalFormatPayload.clear();
    mVkFormatPayload.clear();
}

angle::Result SamplerYcbcrConversionCache::getSamplerYcbcrConversion(
    vk::Context *context,
    const vk::YcbcrConversionDesc &ycbcrConversionDesc,
    VkSamplerYcbcrConversion *vkSamplerYcbcrConversionOut)
{
    ASSERT(ycbcrConversionDesc.valid());
    ASSERT(vkSamplerYcbcrConversionOut);

    SamplerYcbcrConversionMap &payload =
        (ycbcrConversionDesc.getExternalFormat() != 0) ? mExternalFormatPayload : mVkFormatPayload;
    const auto iter = payload.find(ycbcrConversionDesc);
    if (iter != payload.end())
    {
        vk::SamplerYcbcrConversion &samplerYcbcrConversion = iter->second;
        mCacheStats.hit();
        *vkSamplerYcbcrConversionOut = samplerYcbcrConversion.getHandle();
        return angle::Result::Continue;
    }

    mCacheStats.missAndIncrementSize();

    // Create the VkSamplerYcbcrConversion
    vk::SamplerYcbcrConversion wrappedSamplerYcbcrConversion;
    ANGLE_TRY(ycbcrConversionDesc.init(context, &wrappedSamplerYcbcrConversion));

    auto insertedItem = payload.emplace(
        ycbcrConversionDesc, vk::SamplerYcbcrConversion(std::move(wrappedSamplerYcbcrConversion)));
    vk::SamplerYcbcrConversion &insertedSamplerYcbcrConversion = insertedItem.first->second;
    *vkSamplerYcbcrConversionOut = insertedSamplerYcbcrConversion.getHandle();

    context->getRenderer()->onAllocateHandle(vk::HandleType::SamplerYcbcrConversion);

    return angle::Result::Continue;
}

// SamplerCache implementation.
SamplerCache::SamplerCache() = default;

SamplerCache::~SamplerCache()
{
    ASSERT(mPayload.empty());
}

void SamplerCache::destroy(RendererVk *rendererVk)
{
    rendererVk->accumulateCacheStats(VulkanCacheType::Sampler, mCacheStats);

    VkDevice device = rendererVk->getDevice();

    for (auto &iter : mPayload)
    {
        vk::RefCountedSampler &sampler = iter.second;
        ASSERT(!sampler.isReferenced());
        sampler.get().get().destroy(device);

        rendererVk->onDeallocateHandle(vk::HandleType::Sampler);
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
        mCacheStats.hit();
        return angle::Result::Continue;
    }

    mCacheStats.missAndIncrementSize();
    vk::SamplerHelper samplerHelper(contextVk);
    ANGLE_TRY(desc.init(contextVk, &samplerHelper.get()));

    vk::RefCountedSampler newSampler(std::move(samplerHelper));
    auto insertedItem                      = mPayload.emplace(desc, std::move(newSampler));
    vk::RefCountedSampler &insertedSampler = insertedItem.first->second;
    samplerOut->set(&insertedSampler);

    contextVk->getRenderer()->onAllocateHandle(vk::HandleType::Sampler);

    return angle::Result::Continue;
}
}  // namespace rx
