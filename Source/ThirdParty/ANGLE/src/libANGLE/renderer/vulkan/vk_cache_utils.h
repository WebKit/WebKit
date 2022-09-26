//
// Copyright 2018 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// vk_cache_utils.h:
//    Contains the classes for the Pipeline State Object cache as well as the RenderPass cache.
//    Also contains the structures for the packed descriptions for the RenderPass and Pipeline.
//

#ifndef LIBANGLE_RENDERER_VULKAN_VK_CACHE_UTILS_H_
#define LIBANGLE_RENDERER_VULKAN_VK_CACHE_UTILS_H_

#include "common/Color.h"
#include "common/FixedVector.h"
#include "libANGLE/Uniform.h"
#include "libANGLE/renderer/ShaderInterfaceVariableInfoMap.h"
#include "libANGLE/renderer/vulkan/ResourceVk.h"
#include "libANGLE/renderer/vulkan/vk_utils.h"

namespace gl
{
class ProgramExecutable;
}  // namespace gl

namespace rx
{
class ShaderInterfaceVariableInfoMap;
class UpdateDescriptorSetsBuilder;

// Some descriptor set and pipeline layout constants.
//
// The set/binding assignment is done as following:
//
// - Set 0 contains the ANGLE driver uniforms at binding 0.  Note that driver uniforms are updated
//   only under rare circumstances, such as viewport or depth range change.  However, there is only
//   one binding in this set.  This set is placed before Set 1 containing transform feedback
//   buffers, so that switching between xfb and non-xfb programs doesn't require rebinding this set.
//   Otherwise, as the layout of Set 1 changes (due to addition and removal of xfb buffers), and all
//   subsequent sets need to be rebound (due to Vulkan pipeline layout validation rules), we would
//   have needed to invalidateGraphicsDriverUniforms().
// - Set 1 contains uniform blocks created to encompass default uniforms.  1 binding is used per
//   pipeline stage.  Additionally, transform feedback buffers are bound from binding 2 and up.
// - Set 2 contains all textures (including texture buffers).
// - Set 3 contains all other shader resources, such as uniform and storage blocks, atomic counter
//   buffers, images and image buffers.

enum class DescriptorSetIndex : uint32_t
{
    Internal       = 0,         // Internal shaders
    UniformsAndXfb = Internal,  // Uniforms set index
    Texture        = 1,         // Textures set index
    ShaderResource = 2,         // Other shader resources set index

    InvalidEnum = 3,
    EnumCount   = InvalidEnum,
};

class PipelineCacheAccess;

namespace vk
{
class BufferHelper;
class DynamicDescriptorPool;
class ImageHelper;
class SamplerHelper;
enum class ImageLayout;

using RefCountedDescriptorSetLayout    = RefCounted<DescriptorSetLayout>;
using RefCountedPipelineLayout         = RefCounted<PipelineLayout>;
using RefCountedSamplerYcbcrConversion = RefCounted<SamplerYcbcrConversion>;

// Packed Vk resource descriptions.
// Most Vk types use many more bits than required to represent the underlying data.
// Since ANGLE wants to cache things like RenderPasses and Pipeline State Objects using
// hashing (and also needs to check equality) we can optimize these operations by
// using fewer bits. Hence the packed types.
//
// One implementation note: these types could potentially be improved by using even
// fewer bits. For example, boolean values could be represented by a single bit instead
// of a uint8_t. However at the current time there are concerns about the portability
// of bitfield operators, and complexity issues with using bit mask operations. This is
// something we will likely want to investigate as the Vulkan implementation progresses.
//
// Second implementation note: the struct packing is also a bit fragile, and some of the
// packing requirements depend on using alignas and field ordering to get the result of
// packing nicely into the desired space. This is something we could also potentially fix
// with a redesign to use bitfields or bit mask operations.

// Enable struct padding warnings for the code below since it is used in caches.
ANGLE_ENABLE_STRUCT_PADDING_WARNINGS

enum ResourceAccess
{
    Unused,
    ReadOnly,
    Write,
};

inline void UpdateAccess(ResourceAccess *oldAccess, ResourceAccess newAccess)
{
    if (newAccess > *oldAccess)
    {
        *oldAccess = newAccess;
    }
}

enum class RenderPassLoadOp
{
    Load     = VK_ATTACHMENT_LOAD_OP_LOAD,
    Clear    = VK_ATTACHMENT_LOAD_OP_CLEAR,
    DontCare = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
    None,
};
enum class RenderPassStoreOp
{
    Store    = VK_ATTACHMENT_STORE_OP_STORE,
    DontCare = VK_ATTACHMENT_STORE_OP_DONT_CARE,
    None,
};

// There can be a maximum of IMPLEMENTATION_MAX_DRAW_BUFFERS color and resolve attachments, plus one
// depth/stencil attachment and one depth/stencil resolve attachment.
constexpr size_t kMaxFramebufferAttachments = gl::IMPLEMENTATION_MAX_DRAW_BUFFERS * 2 + 2;
template <typename T>
using FramebufferAttachmentArray = std::array<T, kMaxFramebufferAttachments>;
template <typename T>
using FramebufferAttachmentsVector = angle::FixedVector<T, kMaxFramebufferAttachments>;
using FramebufferAttachmentMask    = angle::BitSet<kMaxFramebufferAttachments>;

constexpr size_t kMaxFramebufferNonResolveAttachments = gl::IMPLEMENTATION_MAX_DRAW_BUFFERS + 1;
template <typename T>
using FramebufferNonResolveAttachmentArray = std::array<T, kMaxFramebufferNonResolveAttachments>;
using FramebufferNonResolveAttachmentMask  = angle::BitSet16<kMaxFramebufferNonResolveAttachments>;

class alignas(4) RenderPassDesc final
{
  public:
    RenderPassDesc();
    ~RenderPassDesc();
    RenderPassDesc(const RenderPassDesc &other);
    RenderPassDesc &operator=(const RenderPassDesc &other);

    // Set format for an enabled GL color attachment.
    void packColorAttachment(size_t colorIndexGL, angle::FormatID formatID);
    // Mark a GL color attachment index as disabled.
    void packColorAttachmentGap(size_t colorIndexGL);
    // The caller must pack the depth/stencil attachment last, which is packed right after the color
    // attachments (including gaps), i.e. with an index starting from |colorAttachmentRange()|.
    void packDepthStencilAttachment(angle::FormatID angleFormatID);
    void updateDepthStencilAccess(ResourceAccess access);
    // Indicate that a color attachment should have a corresponding resolve attachment.
    void packColorResolveAttachment(size_t colorIndexGL);
    // Remove the resolve attachment.  Used when optimizing blit through resolve attachment to
    // temporarily pack a resolve attachment and then remove it.
    void removeColorResolveAttachment(size_t colorIndexGL);
    // Indicate that a color attachment should take its data from the resolve attachment initially.
    void packColorUnresolveAttachment(size_t colorIndexGL);
    void removeColorUnresolveAttachment(size_t colorIndexGL);
    // Indicate that a depth/stencil attachment should have a corresponding resolve attachment.
    void packDepthStencilResolveAttachment();
    // Indicate that a depth/stencil attachment should take its data from the resolve attachment
    // initially.
    void packDepthStencilUnresolveAttachment(bool unresolveDepth, bool unresolveStencil);
    void removeDepthStencilUnresolveAttachment();

    void setWriteControlMode(gl::SrgbWriteControlMode mode);

    size_t hash() const;

    // Color attachments are in [0, colorAttachmentRange()), with possible gaps.
    size_t colorAttachmentRange() const { return mColorAttachmentRange; }
    size_t depthStencilAttachmentIndex() const { return colorAttachmentRange(); }

    bool isColorAttachmentEnabled(size_t colorIndexGL) const;
    bool hasDepthStencilAttachment() const;
    gl::DrawBufferMask getColorResolveAttachmentMask() const { return mColorResolveAttachmentMask; }
    bool hasColorResolveAttachment(size_t colorIndexGL) const
    {
        return mColorResolveAttachmentMask.test(colorIndexGL);
    }
    gl::DrawBufferMask getColorUnresolveAttachmentMask() const
    {
        return mColorUnresolveAttachmentMask;
    }
    bool hasColorUnresolveAttachment(size_t colorIndexGL) const
    {
        return mColorUnresolveAttachmentMask.test(colorIndexGL);
    }
    bool hasDepthStencilResolveAttachment() const { return mResolveDepthStencil; }
    bool hasDepthStencilUnresolveAttachment() const { return mUnresolveDepth || mUnresolveStencil; }
    bool hasDepthUnresolveAttachment() const { return mUnresolveDepth; }
    bool hasStencilUnresolveAttachment() const { return mUnresolveStencil; }
    gl::SrgbWriteControlMode getSRGBWriteControlMode() const
    {
        return static_cast<gl::SrgbWriteControlMode>(mSrgbWriteControl);
    }

    // Get the number of attachments in the Vulkan render pass, i.e. after removing disabled
    // color attachments.
    size_t attachmentCount() const;

    void setSamples(GLint samples) { mSamples = static_cast<uint8_t>(samples); }
    uint8_t samples() const { return mSamples; }

    void setViewCount(GLsizei viewCount) { mViewCount = static_cast<uint8_t>(viewCount); }
    uint8_t viewCount() const { return mViewCount; }

    void setFramebufferFetchMode(bool hasFramebufferFetch)
    {
        mHasFramebufferFetch = hasFramebufferFetch;
    }
    bool hasFramebufferFetch() const { return mHasFramebufferFetch; }

    void updateRenderToTexture(bool isRenderToTexture) { mIsRenderToTexture = isRenderToTexture; }
    bool isRenderToTexture() const { return mIsRenderToTexture; }

    angle::FormatID operator[](size_t index) const
    {
        ASSERT(index < gl::IMPLEMENTATION_MAX_DRAW_BUFFERS + 1);
        return static_cast<angle::FormatID>(mAttachmentFormats[index]);
    }

  private:
    uint8_t mSamples;
    uint8_t mColorAttachmentRange;

    // Multiview
    uint8_t mViewCount;

    // sRGB
    uint8_t mSrgbWriteControl : 1;

    // Framebuffer fetch
    uint8_t mHasFramebufferFetch : 1;

    // Multisampled render to texture
    uint8_t mIsRenderToTexture : 1;
    uint8_t mResolveDepthStencil : 1;
    uint8_t mUnresolveDepth : 1;
    uint8_t mUnresolveStencil : 1;

    // Available space for expansion.
    uint8_t mPadding1 : 2;
    uint8_t mPadding2;

    // Whether each color attachment has a corresponding resolve attachment.  Color resolve
    // attachments can be used to optimize resolve through glBlitFramebuffer() as well as support
    // GL_EXT_multisampled_render_to_texture and GL_EXT_multisampled_render_to_texture2.
    gl::DrawBufferMask mColorResolveAttachmentMask;

    // Whether each color attachment with a corresponding resolve attachment should be initialized
    // with said resolve attachment in an initial subpass.  This is an optimization to avoid
    // loadOp=LOAD on the implicit multisampled image used with multisampled-render-to-texture
    // render targets.  This operation is referred to as "unresolve".
    //
    // Unused when VK_EXT_multisampled_render_to_single_sampled is available.
    gl::DrawBufferMask mColorUnresolveAttachmentMask;

    // Color attachment formats are stored with their GL attachment indices.  The depth/stencil
    // attachment formats follow the last enabled color attachment.  When creating a render pass,
    // the disabled attachments are removed and the resulting attachments are packed.
    //
    // The attachment indices provided as input to various functions in this file are thus GL
    // attachment indices.  These indices are marked as such, e.g. colorIndexGL.  The render pass
    // (and corresponding framebuffer object) lists the packed attachments, with the corresponding
    // indices marked with Vk, e.g. colorIndexVk.  The subpass attachment references create the
    // link between the two index spaces.  The subpass declares attachment references with GL
    // indices (which corresponds to the location decoration of shader outputs).  The attachment
    // references then contain the Vulkan indices or VK_ATTACHMENT_UNUSED.
    //
    // For example, if GL uses color attachments 0 and 3, then there are two render pass
    // attachments (indexed 0 and 1) and 4 subpass attachments:
    //
    //  - Subpass attachment 0 -> Renderpass attachment 0
    //  - Subpass attachment 1 -> VK_ATTACHMENT_UNUSED
    //  - Subpass attachment 2 -> VK_ATTACHMENT_UNUSED
    //  - Subpass attachment 3 -> Renderpass attachment 1
    //
    // The resolve attachments are packed after the non-resolve attachments.  They use the same
    // formats, so they are not specified in this array.
    FramebufferNonResolveAttachmentArray<uint8_t> mAttachmentFormats;
};

bool operator==(const RenderPassDesc &lhs, const RenderPassDesc &rhs);

constexpr size_t kRenderPassDescSize = sizeof(RenderPassDesc);
static_assert(kRenderPassDescSize == 16, "Size check failed");

enum class CacheLookUpFeedback
{
    None,
    Hit,
    Miss,
    WarmUpHit,
    WarmUpMiss,
};

struct PackedAttachmentOpsDesc final
{
    // RenderPassLoadOp is in range [0, 3], and RenderPassStoreOp is in range [0, 2].
    uint16_t loadOp : 2;
    uint16_t storeOp : 2;
    uint16_t stencilLoadOp : 2;
    uint16_t stencilStoreOp : 2;
    // If a corresponding resolve attachment exists, storeOp may already be DONT_CARE, and it's
    // unclear whether the attachment was invalidated or not.  This information is passed along here
    // so that the resolve attachment's storeOp can be set to DONT_CARE if the attachment is
    // invalidated, and if possible removed from the list of resolve attachments altogether.  Note
    // that the latter may not be possible if the render pass has multiple subpasses due to Vulkan
    // render pass compatibility rules.
    uint16_t isInvalidated : 1;
    uint16_t isStencilInvalidated : 1;
    uint16_t padding1 : 6;

    // 4-bits to force pad the structure to exactly 2 bytes.  Note that we currently don't support
    // any of the extension layouts, whose values start at 1'000'000'000.
    uint16_t initialLayout : 4;
    uint16_t finalLayout : 4;
    uint16_t padding2 : 8;
};

static_assert(sizeof(PackedAttachmentOpsDesc) == 4, "Size check failed");

class PackedAttachmentIndex;

class AttachmentOpsArray final
{
  public:
    AttachmentOpsArray();
    ~AttachmentOpsArray();
    AttachmentOpsArray(const AttachmentOpsArray &other);
    AttachmentOpsArray &operator=(const AttachmentOpsArray &other);

    const PackedAttachmentOpsDesc &operator[](PackedAttachmentIndex index) const;
    PackedAttachmentOpsDesc &operator[](PackedAttachmentIndex index);

    // Initialize an attachment op with all load and store operations.
    void initWithLoadStore(PackedAttachmentIndex index,
                           ImageLayout initialLayout,
                           ImageLayout finalLayout);

    void setLayouts(PackedAttachmentIndex index,
                    ImageLayout initialLayout,
                    ImageLayout finalLayout);
    void setOps(PackedAttachmentIndex index, RenderPassLoadOp loadOp, RenderPassStoreOp storeOp);
    void setStencilOps(PackedAttachmentIndex index,
                       RenderPassLoadOp loadOp,
                       RenderPassStoreOp storeOp);

    void setClearOp(PackedAttachmentIndex index);
    void setClearStencilOp(PackedAttachmentIndex index);

    size_t hash() const;

  private:
    gl::AttachmentArray<PackedAttachmentOpsDesc> mOps;
};

bool operator==(const AttachmentOpsArray &lhs, const AttachmentOpsArray &rhs);

static_assert(sizeof(AttachmentOpsArray) == 40, "Size check failed");

struct PackedAttribDesc final
{
    uint8_t format;
    uint8_t divisor;

    // Desktop drivers support
    uint16_t offset : kAttributeOffsetMaxBits;

    uint16_t compressed : 1;
};

constexpr size_t kPackedAttribDescSize = sizeof(PackedAttribDesc);
static_assert(kPackedAttribDescSize == 4, "Size mismatch");

struct VertexInputAttributes final
{
    PackedAttribDesc attribs[gl::MAX_VERTEX_ATTRIBS];
};

constexpr size_t kVertexInputAttributesSize = sizeof(VertexInputAttributes);
static_assert(kVertexInputAttributesSize == 64, "Size mismatch");

struct PackedRasterizationAndLogicOpStateBits final
{
    // Note: Currently only 2 subpasses possible.
    uint16_t subpass : 1;
    uint16_t depthClampEnable : 1;
    uint16_t sampleShadingEnable : 1;
    uint16_t alphaToCoverageEnable : 1;
    uint16_t alphaToOneEnable : 1;
    uint16_t rasterizationSamples : 6;
    uint16_t logicOpEnable : 1;
    uint16_t logicOp : 4;
};

constexpr size_t kPackedRasterizationAndLogicOpStateBitsSize =
    sizeof(PackedRasterizationAndLogicOpStateBits);
static_assert(kPackedRasterizationAndLogicOpStateBitsSize == 2, "Size check failed");

struct PackedInputAssemblyAndMiscState final
{
    uint32_t topology : 4;
    uint32_t patchVertices : 6;
    uint32_t surfaceRotation : 1;
    uint32_t viewportNegativeOneToOne : 1;
    uint32_t depthBoundsTest : 1;
    // 11-bit normalized instead of float to align the struct.
    uint32_t minSampleShading : 11;
    uint32_t blendEnableMask : 8;
};

constexpr size_t kPackedInputAssemblyAndMiscStateSize = sizeof(PackedInputAssemblyAndMiscState);
static_assert(kPackedInputAssemblyAndMiscStateSize == 4, "Size check failed");

struct PackedInputAssemblyAndRasterizationStateInfo final
{
    PackedRasterizationAndLogicOpStateBits bits;
    // Note: Only up to 16xMSAA is supported in the Vulkan backend.
    uint16_t sampleMask;
    PackedInputAssemblyAndMiscState misc;
};

constexpr size_t kPackedInputAssemblyAndRasterizationStateSize =
    sizeof(PackedInputAssemblyAndRasterizationStateInfo);
static_assert(kPackedInputAssemblyAndRasterizationStateSize == 8, "Size check failed");

struct StencilOps final
{
    uint8_t fail : 4;
    uint8_t pass : 4;
    uint8_t depthFail : 4;
    uint8_t compare : 4;
};

constexpr size_t kStencilOpsSize = sizeof(StencilOps);
static_assert(kStencilOpsSize == 2, "Size check failed");

struct PackedStencilOpState final
{
    StencilOps ops;
};

constexpr size_t kPackedStencilOpSize = sizeof(PackedStencilOpState);
static_assert(kPackedStencilOpSize == 2, "Size check failed");

struct PackedColorBlendAttachmentState final
{
    uint16_t srcColorBlendFactor : 5;
    uint16_t dstColorBlendFactor : 5;
    uint16_t colorBlendOp : 6;
    uint16_t srcAlphaBlendFactor : 5;
    uint16_t dstAlphaBlendFactor : 5;
    uint16_t alphaBlendOp : 6;
};

constexpr size_t kPackedColorBlendAttachmentStateSize = sizeof(PackedColorBlendAttachmentState);
static_assert(kPackedColorBlendAttachmentStateSize == 4, "Size check failed");

struct PackedColorBlendStateInfo final
{
    uint8_t colorWriteMaskBits[gl::IMPLEMENTATION_MAX_DRAW_BUFFERS / 2];
    PackedColorBlendAttachmentState attachments[gl::IMPLEMENTATION_MAX_DRAW_BUFFERS];
};

constexpr size_t kPackedColorBlendStateSize = sizeof(PackedColorBlendStateInfo);
static_assert(kPackedColorBlendStateSize == 36, "Size check failed");

struct PackedDitherAndWorkarounds final
{
    static_assert(gl::IMPLEMENTATION_MAX_DRAW_BUFFERS <= 8,
                  "2 bits per draw buffer is needed for dither emulation");
    uint16_t emulatedDitherControl;
    uint16_t nonZeroStencilWriteMaskWorkaround : 1;
    uint16_t unused : 15;
};

// State that is dynamic in VK_EXT_extended_dynamic_state and 2.  These are placed at the end of the
// pipeline description so they can be excluded from hash when the extension is present.
//
// The hash function takes the input as a multiple of 4 bytes.  VK_EXT_extended_dynamic_state2 has
// too few bits to have a dedicated entry, and can be included with the bits for
// VK_EXT_extended_dynamic_state.  Additionally, both extensions are promoted to core in Vulkan 1.3,
// so eventually they are either both present or none.
struct PackedDynamicState1And2 final
{
    // From VK_EXT_extended_dynamic_state
    uint32_t cullMode : 4;
    uint32_t frontFace : 4;

    uint32_t depthCompareOp : 4;

    uint32_t depthTest : 1;
    uint32_t depthWrite : 1;
    uint32_t stencilTest : 1;

    // From VK_EXT_extended_dynamic_state2
    uint32_t rasterizerDiscardEnable : 1;
    uint32_t depthBiasEnable : 1;
    uint32_t primitiveRestartEnable : 1;

    // Store support for VK_EXT_extended_dynamic_state/2 in the bits wasted here for padding.  This
    // is to support GraphicsPipelineDesc::hash(), allowing it to exclude this state from the hash.
    uint32_t supportsDynamicState1 : 1;
    uint32_t supportsDynamicState2 : 1;
    uint32_t forceStaticVertexStrideState : 1;

    uint32_t padding : 11;
};

constexpr size_t kPackedDynamicState1And2Size = sizeof(PackedDynamicState1And2);
static_assert(kPackedDynamicState1And2Size == 4, "Size check failed");

struct PackedDynamicState1 final
{
    PackedStencilOpState front;
    PackedStencilOpState back;

    // Although technically stride can be any value in ES 2.0, in practice supporting stride
    // greater than MAX_USHORT should not be that helpful. Note that stride limits are
    // introduced in ES 3.1.
    uint16_t vertexStrides[gl::MAX_VERTEX_ATTRIBS];
};

constexpr size_t kPackedDynamicState1Size = sizeof(PackedDynamicState1);
static_assert(kPackedDynamicState1Size == 36, "Size check failed");

struct PackedDynamicState final
{
    PackedDynamicState1And2 ds1And2;
    PackedDynamicState1 ds1;
};

constexpr size_t kPackedDynamicStateSize = sizeof(PackedDynamicState);
static_assert(kPackedDynamicStateSize == 40, "Size check failed");

constexpr size_t kGraphicsPipelineDescSumOfSizes =
    kVertexInputAttributesSize + kRenderPassDescSize +
    kPackedInputAssemblyAndRasterizationStateSize + kPackedColorBlendStateSize +
    sizeof(PackedDitherAndWorkarounds) + kPackedDynamicStateSize;

// Number of dirty bits in the dirty bit set.
constexpr size_t kGraphicsPipelineDirtyBitBytes = 4;
constexpr static size_t kNumGraphicsPipelineDirtyBits =
    kGraphicsPipelineDescSumOfSizes / kGraphicsPipelineDirtyBitBytes;
static_assert(kNumGraphicsPipelineDirtyBits <= 64, "Too many pipeline dirty bits");

// Set of dirty bits. Each bit represents kGraphicsPipelineDirtyBitBytes in the desc.
using GraphicsPipelineTransitionBits = angle::BitSet<kNumGraphicsPipelineDirtyBits>;

// State changes are applied through the update methods. Each update method can also have a
// sibling method that applies the update without marking a state transition. The non-transition
// update methods are used for internal shader pipelines. Not every non-transition update method
// is implemented yet as not every state is used in internal shaders.
class GraphicsPipelineDesc final
{
  public:
    // Use aligned allocation and free so we can use the alignas keyword.
    void *operator new(std::size_t size);
    void operator delete(void *ptr);

    GraphicsPipelineDesc();
    ~GraphicsPipelineDesc();
    GraphicsPipelineDesc(const GraphicsPipelineDesc &other);
    GraphicsPipelineDesc &operator=(const GraphicsPipelineDesc &other);

    size_t hash() const;
    bool operator==(const GraphicsPipelineDesc &other) const;

    void initDefaults(const ContextVk *contextVk);

    // For custom comparisons.
    template <typename T>
    const T *getPtr() const
    {
        return reinterpret_cast<const T *>(this);
    }

    angle::Result initializePipeline(ContextVk *contextVk,
                                     PipelineCacheAccess *pipelineCache,
                                     const RenderPass &compatibleRenderPass,
                                     const PipelineLayout &pipelineLayout,
                                     const gl::AttributesMask &activeAttribLocationsMask,
                                     const gl::ComponentTypeMask &programAttribsTypeMask,
                                     const gl::DrawBufferMask &missingOutputsMask,
                                     const ShaderAndSerialMap &shaders,
                                     const SpecializationConstants &specConsts,
                                     Pipeline *pipelineOut,
                                     CacheLookUpFeedback *feedbackOut) const;

    // Vertex input state. For ES 3.1 this should be separated into binding and attribute.
    void updateVertexInput(ContextVk *contextVk,
                           GraphicsPipelineTransitionBits *transition,
                           uint32_t attribIndex,
                           GLuint stride,
                           GLuint divisor,
                           angle::FormatID format,
                           bool compressed,
                           GLuint relativeOffset);

    // Input assembly info
    void setTopology(gl::PrimitiveMode drawMode);
    void updateTopology(GraphicsPipelineTransitionBits *transition, gl::PrimitiveMode drawMode);
    void updatePrimitiveRestartEnabled(GraphicsPipelineTransitionBits *transition,
                                       bool primitiveRestartEnabled);

    // Viewport states
    void updateDepthClipControl(GraphicsPipelineTransitionBits *transition, bool negativeOneToOne);

    // Raster states
    void updateCullMode(GraphicsPipelineTransitionBits *transition,
                        const gl::RasterizerState &rasterState);
    void updateFrontFace(GraphicsPipelineTransitionBits *transition,
                         const gl::RasterizerState &rasterState,
                         bool invertFrontFace);
    void updateRasterizerDiscardEnabled(GraphicsPipelineTransitionBits *transition,
                                        bool rasterizerDiscardEnabled);

    // Multisample states
    uint32_t getRasterizationSamples() const;
    void setRasterizationSamples(uint32_t rasterizationSamples);
    void updateRasterizationSamples(GraphicsPipelineTransitionBits *transition,
                                    uint32_t rasterizationSamples);
    void updateAlphaToCoverageEnable(GraphicsPipelineTransitionBits *transition, bool enable);
    void updateAlphaToOneEnable(GraphicsPipelineTransitionBits *transition, bool enable);
    void updateSampleMask(GraphicsPipelineTransitionBits *transition,
                          uint32_t maskNumber,
                          uint32_t mask);

    void updateSampleShading(GraphicsPipelineTransitionBits *transition, bool enable, float value);

    // RenderPass description.
    const RenderPassDesc &getRenderPassDesc() const { return mRenderPassDesc; }

    void setRenderPassDesc(const RenderPassDesc &renderPassDesc);
    void updateRenderPassDesc(GraphicsPipelineTransitionBits *transition,
                              const RenderPassDesc &renderPassDesc);
    void setRenderPassSampleCount(GLint samples);
    void setRenderPassFramebufferFetchMode(bool hasFramebufferFetch);
    bool getRenderPassFramebufferFetchMode() const { return mRenderPassDesc.hasFramebufferFetch(); }

    void setRenderPassColorAttachmentFormat(size_t colorIndexGL, angle::FormatID formatID);

    // Blend states
    void setSingleBlend(uint32_t colorIndexGL,
                        bool enabled,
                        VkBlendOp op,
                        VkBlendFactor srcFactor,
                        VkBlendFactor dstFactor);
    void updateBlendEnabled(GraphicsPipelineTransitionBits *transition,
                            gl::DrawBufferMask blendEnabledMask);
    void updateBlendFuncs(GraphicsPipelineTransitionBits *transition,
                          const gl::BlendStateExt &blendStateExt,
                          gl::DrawBufferMask attachmentMask);
    void updateBlendEquations(GraphicsPipelineTransitionBits *transition,
                              const gl::BlendStateExt &blendStateExt,
                              gl::DrawBufferMask attachmentMask);
    void resetBlendFuncsAndEquations(GraphicsPipelineTransitionBits *transition,
                                     const gl::BlendStateExt &blendStateExt,
                                     gl::DrawBufferMask previousAttachmentsMask,
                                     gl::DrawBufferMask newAttachmentsMask);
    void setColorWriteMasks(gl::BlendStateExt::ColorMaskStorage::Type colorMasks,
                            const gl::DrawBufferMask &alphaMask,
                            const gl::DrawBufferMask &enabledDrawBuffers);
    void setSingleColorWriteMask(uint32_t colorIndexGL, VkColorComponentFlags colorComponentFlags);
    void updateColorWriteMasks(GraphicsPipelineTransitionBits *transition,
                               gl::BlendStateExt::ColorMaskStorage::Type colorMasks,
                               const gl::DrawBufferMask &alphaMask,
                               const gl::DrawBufferMask &enabledDrawBuffers);

    // Logic op
    void updateLogicOpEnabled(GraphicsPipelineTransitionBits *transition, bool enable);
    void updateLogicOp(GraphicsPipelineTransitionBits *transition, VkLogicOp logicOp);

    // Depth/stencil states.
    void setDepthTestEnabled(bool enabled);
    void setDepthWriteEnabled(bool enabled);
    void setDepthFunc(VkCompareOp op);
    void setDepthClampEnabled(bool enabled);
    void setStencilTestEnabled(bool enabled);
    void setStencilFrontFuncs(VkCompareOp compareOp);
    void setStencilBackFuncs(VkCompareOp compareOp);
    void setStencilFrontOps(VkStencilOp failOp, VkStencilOp passOp, VkStencilOp depthFailOp);
    void setStencilBackOps(VkStencilOp failOp, VkStencilOp passOp, VkStencilOp depthFailOp);
    void setStencilFrontWriteMask(uint8_t mask);
    void setStencilBackWriteMask(uint8_t mask);
    void updateDepthTestEnabled(GraphicsPipelineTransitionBits *transition,
                                const gl::DepthStencilState &depthStencilState,
                                const gl::Framebuffer *drawFramebuffer);
    void updateDepthFunc(GraphicsPipelineTransitionBits *transition,
                         const gl::DepthStencilState &depthStencilState);
    void updateDepthWriteEnabled(GraphicsPipelineTransitionBits *transition,
                                 const gl::DepthStencilState &depthStencilState,
                                 const gl::Framebuffer *drawFramebuffer);
    void updateStencilTestEnabled(GraphicsPipelineTransitionBits *transition,
                                  const gl::DepthStencilState &depthStencilState,
                                  const gl::Framebuffer *drawFramebuffer);
    void updateStencilFrontFuncs(GraphicsPipelineTransitionBits *transition,
                                 const gl::DepthStencilState &depthStencilState);
    void updateStencilBackFuncs(GraphicsPipelineTransitionBits *transition,
                                const gl::DepthStencilState &depthStencilState);
    void updateStencilFrontOps(GraphicsPipelineTransitionBits *transition,
                               const gl::DepthStencilState &depthStencilState);
    void updateStencilBackOps(GraphicsPipelineTransitionBits *transition,
                              const gl::DepthStencilState &depthStencilState);

    // Depth offset.
    void updatePolygonOffsetFillEnabled(GraphicsPipelineTransitionBits *transition, bool enabled);

    // Tessellation
    void updatePatchVertices(GraphicsPipelineTransitionBits *transition, GLuint value);

    // Subpass
    void resetSubpass(GraphicsPipelineTransitionBits *transition);
    void nextSubpass(GraphicsPipelineTransitionBits *transition);
    void setSubpass(uint32_t subpass);
    uint32_t getSubpass() const;

    void updateSurfaceRotation(GraphicsPipelineTransitionBits *transition,
                               const SurfaceRotation surfaceRotation);
    bool getSurfaceRotation() const
    {
        return mInputAssemblyAndRasterizationStateInfo.misc.surfaceRotation;
    }

    void updateEmulatedDitherControl(GraphicsPipelineTransitionBits *transition, uint16_t value);
    uint32_t getEmulatedDitherControl() const
    {
        return mDitherAndWorkarounds.emulatedDitherControl;
    }

    void updateNonZeroStencilWriteMaskWorkaround(GraphicsPipelineTransitionBits *transition,
                                                 bool enabled);

    void setSupportsDynamicStateForTest(bool supports)
    {
        mDynamicState.ds1And2.supportsDynamicState1 = supports;
        mDynamicState.ds1And2.supportsDynamicState2 = supports;
    }

    // Helpers to dump the state
    const VertexInputAttributes &getVertexInputAttribsForLog() const { return mVertexInputAttribs; }
    const RenderPassDesc &getRenderPassDescForLog() const { return mRenderPassDesc; }
    const PackedInputAssemblyAndRasterizationStateInfo
    getInputAssemblyAndRasterizationStateInfoForLog() const
    {
        return mInputAssemblyAndRasterizationStateInfo;
    }
    const PackedColorBlendStateInfo &getColorBlendStateInfoForLog() const
    {
        return mColorBlendStateInfo;
    }
    const PackedDitherAndWorkarounds &getDitherForLog() const { return mDitherAndWorkarounds; }
    const PackedDynamicState &getDynamicStateForLog() const { return mDynamicState; }

  private:
    void updateSubpass(GraphicsPipelineTransitionBits *transition, uint32_t subpass);

    VertexInputAttributes mVertexInputAttribs;
    RenderPassDesc mRenderPassDesc;
    PackedInputAssemblyAndRasterizationStateInfo mInputAssemblyAndRasterizationStateInfo;
    PackedColorBlendStateInfo mColorBlendStateInfo;
    PackedDitherAndWorkarounds mDitherAndWorkarounds;
    PackedDynamicState mDynamicState;
};

// Verify the packed pipeline description has no gaps in the packing.
// This is not guaranteed by the spec, but is validated by a compile-time check.
// No gaps or padding at the end ensures that hashing and memcmp checks will not run
// into uninitialized memory regions.
constexpr size_t kGraphicsPipelineDescSize = sizeof(GraphicsPipelineDesc);
static_assert(kGraphicsPipelineDescSize == kGraphicsPipelineDescSumOfSizes, "Size mismatch");

constexpr uint32_t kMaxDescriptorSetLayoutBindings =
    std::max(gl::IMPLEMENTATION_MAX_ACTIVE_TEXTURES,
             gl::IMPLEMENTATION_MAX_UNIFORM_BUFFER_BINDINGS);

using DescriptorSetLayoutBindingVector =
    angle::FixedVector<VkDescriptorSetLayoutBinding, kMaxDescriptorSetLayoutBindings>;

// A packed description of a descriptor set layout. Use similarly to RenderPassDesc and
// GraphicsPipelineDesc. Currently we only need to differentiate layouts based on sampler and ubo
// usage. In the future we could generalize this.
class DescriptorSetLayoutDesc final
{
  public:
    DescriptorSetLayoutDesc();
    ~DescriptorSetLayoutDesc();
    DescriptorSetLayoutDesc(const DescriptorSetLayoutDesc &other);
    DescriptorSetLayoutDesc &operator=(const DescriptorSetLayoutDesc &other);

    size_t hash() const;
    bool operator==(const DescriptorSetLayoutDesc &other) const;

    void update(uint32_t bindingIndex,
                VkDescriptorType descriptorType,
                uint32_t count,
                VkShaderStageFlags stages,
                const Sampler *immutableSampler);

    void unpackBindings(DescriptorSetLayoutBindingVector *bindings,
                        std::vector<VkSampler> *immutableSamplers) const;

    bool empty() const { return *this == DescriptorSetLayoutDesc(); }

  private:
    // There is a small risk of an issue if the sampler cache is evicted but not the descriptor
    // cache we would have an invalid handle here. Thus propose follow-up work:
    // TODO: https://issuetracker.google.com/issues/159156775: Have immutable sampler use serial
    struct PackedDescriptorSetBinding
    {
        uint8_t type;    // Stores a packed VkDescriptorType descriptorType.
        uint8_t stages;  // Stores a packed VkShaderStageFlags.
        uint16_t count;  // Stores a packed uint32_t descriptorCount.
        uint32_t pad;
        VkSampler immutableSampler;
    };

    // 4x 32bit
    static_assert(sizeof(PackedDescriptorSetBinding) == 16, "Unexpected size");

    // This is a compact representation of a descriptor set layout.
    std::array<PackedDescriptorSetBinding, kMaxDescriptorSetLayoutBindings>
        mPackedDescriptorSetLayout;
};

// The following are for caching descriptor set layouts. Limited to max three descriptor set
// layouts. This can be extended in the future.
constexpr size_t kMaxDescriptorSetLayouts = 3;

struct PackedPushConstantRange
{
    uint8_t offset;
    uint8_t size;
    uint16_t stageMask;
};

static_assert(sizeof(PackedPushConstantRange) == sizeof(uint32_t), "Unexpected Size");

template <typename T>
using DescriptorSetArray              = angle::PackedEnumMap<DescriptorSetIndex, T>;
using DescriptorSetLayoutPointerArray = DescriptorSetArray<BindingPointer<DescriptorSetLayout>>;

class PipelineLayoutDesc final
{
  public:
    PipelineLayoutDesc();
    ~PipelineLayoutDesc();
    PipelineLayoutDesc(const PipelineLayoutDesc &other);
    PipelineLayoutDesc &operator=(const PipelineLayoutDesc &rhs);

    size_t hash() const;
    bool operator==(const PipelineLayoutDesc &other) const;

    void updateDescriptorSetLayout(DescriptorSetIndex setIndex,
                                   const DescriptorSetLayoutDesc &desc);
    void updatePushConstantRange(VkShaderStageFlags stageMask, uint32_t offset, uint32_t size);

    const PackedPushConstantRange &getPushConstantRange() const { return mPushConstantRange; }

  private:
    DescriptorSetArray<DescriptorSetLayoutDesc> mDescriptorSetLayouts;
    PackedPushConstantRange mPushConstantRange;
    [[maybe_unused]] uint32_t mPadding;

    // Verify the arrays are properly packed.
    static_assert(sizeof(decltype(mDescriptorSetLayouts)) ==
                      (sizeof(DescriptorSetLayoutDesc) * kMaxDescriptorSetLayouts),
                  "Unexpected size");
};

// Verify the structure is properly packed.
static_assert(sizeof(PipelineLayoutDesc) == sizeof(DescriptorSetArray<DescriptorSetLayoutDesc>) +
                                                sizeof(PackedPushConstantRange) + sizeof(uint32_t),
              "Unexpected Size");

class YcbcrConversionDesc final
{
  public:
    YcbcrConversionDesc();
    ~YcbcrConversionDesc();
    YcbcrConversionDesc(const YcbcrConversionDesc &other);
    YcbcrConversionDesc &operator=(const YcbcrConversionDesc &other);

    size_t hash() const;
    bool operator==(const YcbcrConversionDesc &other) const;

    bool valid() const { return mExternalOrVkFormat != 0; }
    void reset();
    void update(RendererVk *rendererVk,
                uint64_t externalFormat,
                VkSamplerYcbcrModelConversion conversionModel,
                VkSamplerYcbcrRange colorRange,
                VkChromaLocation xChromaOffset,
                VkChromaLocation yChromaOffset,
                VkFilter chromaFilter,
                VkComponentMapping components,
                angle::FormatID intendedFormatID);
    VkFilter getChromaFilter() const { return static_cast<VkFilter>(mChromaFilter); }
    bool updateChromaFilter(RendererVk *rendererVk, VkFilter filter);
    void updateConversionModel(VkSamplerYcbcrModelConversion conversionModel);
    uint64_t getExternalFormat() const { return mIsExternalFormat ? mExternalOrVkFormat : 0; }

    angle::Result init(Context *context, SamplerYcbcrConversion *conversionOut) const;

  private:
    // If the sampler needs to convert the image content (e.g. from YUV to RGB) then
    // mExternalOrVkFormat will be non-zero. The value is either the external format
    // as returned by vkGetAndroidHardwareBufferPropertiesANDROID or a YUV VkFormat.
    // For VkSamplerYcbcrConversion, mExternalOrVkFormat along with mIsExternalFormat,
    // mConversionModel and mColorRange works as a Serial() used elsewhere in ANGLE.
    uint64_t mExternalOrVkFormat;
    // 1 bit to identify if external format is used
    uint32_t mIsExternalFormat : 1;
    // 3 bits to identify conversion model
    uint32_t mConversionModel : 3;
    // 1 bit to identify color component range
    uint32_t mColorRange : 1;
    // 1 bit to identify x chroma location
    uint32_t mXChromaOffset : 1;
    // 1 bit to identify y chroma location
    uint32_t mYChromaOffset : 1;
    // 1 bit to identify chroma filtering
    uint32_t mChromaFilter : 1;
    // 3 bit to identify R component swizzle
    uint32_t mRSwizzle : 3;
    // 3 bit to identify G component swizzle
    uint32_t mGSwizzle : 3;
    // 3 bit to identify B component swizzle
    uint32_t mBSwizzle : 3;
    // 3 bit to identify A component swizzle
    uint32_t mASwizzle : 3;
    uint32_t mPadding : 12;
    uint32_t mReserved;
};

static_assert(sizeof(YcbcrConversionDesc) == 16, "Unexpected YcbcrConversionDesc size");

// Packed sampler description for the sampler cache.
class SamplerDesc final
{
  public:
    SamplerDesc();
    SamplerDesc(ContextVk *contextVk,
                const gl::SamplerState &samplerState,
                bool stencilMode,
                const YcbcrConversionDesc *ycbcrConversionDesc,
                angle::FormatID intendedFormatID);
    ~SamplerDesc();

    SamplerDesc(const SamplerDesc &other);
    SamplerDesc &operator=(const SamplerDesc &rhs);

    void update(ContextVk *contextVk,
                const gl::SamplerState &samplerState,
                bool stencilMode,
                const YcbcrConversionDesc *ycbcrConversionDesc,
                angle::FormatID intendedFormatID);
    void reset();
    angle::Result init(ContextVk *contextVk, Sampler *sampler) const;

    size_t hash() const;
    bool operator==(const SamplerDesc &other) const;

  private:
    // 32*4 bits for floating point data.
    // Note: anisotropy enabled is implicitly determined by maxAnisotropy and caps.
    float mMipLodBias;
    float mMaxAnisotropy;
    float mMinLod;
    float mMaxLod;

    // 16*8 bits to uniquely identify a YCbCr conversion sampler.
    YcbcrConversionDesc mYcbcrConversionDesc;

    // 16 bits for modes + states.
    // 1 bit per filter (only 2 possible values in GL: linear/nearest)
    uint16_t mMagFilter : 1;
    uint16_t mMinFilter : 1;
    uint16_t mMipmapMode : 1;

    // 3 bits per address mode (5 possible values)
    uint16_t mAddressModeU : 3;
    uint16_t mAddressModeV : 3;
    uint16_t mAddressModeW : 3;

    // 1 bit for compare enabled (2 possible values)
    uint16_t mCompareEnabled : 1;

    // 3 bits for compare op. (8 possible values)
    uint16_t mCompareOp : 3;

    // Values from angle::ColorGeneric::Type. Float is 0 and others are 1.
    uint16_t mBorderColorType : 1;

    uint16_t mPadding : 15;

    // 16*8 bits for BorderColor
    angle::ColorF mBorderColor;

    // 32 bits reserved for future use.
    uint32_t mReserved;
};

static_assert(sizeof(SamplerDesc) == 56, "Unexpected SamplerDesc size");

// Disable warnings about struct padding.
ANGLE_DISABLE_STRUCT_PADDING_WARNINGS

class PipelineHelper;

struct GraphicsPipelineTransition
{
    GraphicsPipelineTransition();
    GraphicsPipelineTransition(const GraphicsPipelineTransition &other);
    GraphicsPipelineTransition(GraphicsPipelineTransitionBits bits,
                               const GraphicsPipelineDesc *desc,
                               PipelineHelper *pipeline);

    GraphicsPipelineTransitionBits bits;
    const GraphicsPipelineDesc *desc;
    PipelineHelper *target;
};

ANGLE_INLINE GraphicsPipelineTransition::GraphicsPipelineTransition() = default;

ANGLE_INLINE GraphicsPipelineTransition::GraphicsPipelineTransition(
    const GraphicsPipelineTransition &other) = default;

ANGLE_INLINE GraphicsPipelineTransition::GraphicsPipelineTransition(
    GraphicsPipelineTransitionBits bits,
    const GraphicsPipelineDesc *desc,
    PipelineHelper *pipeline)
    : bits(bits), desc(desc), target(pipeline)
{}

ANGLE_INLINE bool GraphicsPipelineTransitionMatch(GraphicsPipelineTransitionBits bitsA,
                                                  GraphicsPipelineTransitionBits bitsB,
                                                  const GraphicsPipelineDesc &descA,
                                                  const GraphicsPipelineDesc &descB)
{
    if (bitsA != bitsB)
        return false;

    // We currently mask over 4 bytes of the pipeline description with each dirty bit.
    // We could consider using 8 bytes and a mask of 32 bits. This would make some parts
    // of the code faster. The for loop below would scan over twice as many bits per iteration.
    // But there may be more collisions between the same dirty bit masks leading to different
    // transitions. Thus there may be additional cost when applications use many transitions.
    // We should revisit this in the future and investigate using different bit widths.
    static_assert(sizeof(uint32_t) == kGraphicsPipelineDirtyBitBytes, "Size mismatch");

    const uint32_t *rawPtrA = descA.getPtr<uint32_t>();
    const uint32_t *rawPtrB = descB.getPtr<uint32_t>();

    for (size_t dirtyBit : bitsA)
    {
        if (rawPtrA[dirtyBit] != rawPtrB[dirtyBit])
            return false;
    }

    return true;
}

class PipelineHelper final : public Resource
{
  public:
    PipelineHelper();
    ~PipelineHelper() override;
    inline explicit PipelineHelper(Pipeline &&pipeline, CacheLookUpFeedback feedback);

    void destroy(VkDevice device);
    void release(ContextVk *contextVk);

    bool valid() const { return mPipeline.valid(); }
    Pipeline &getPipeline() { return mPipeline; }

    ANGLE_INLINE bool findTransition(GraphicsPipelineTransitionBits bits,
                                     const GraphicsPipelineDesc &desc,
                                     PipelineHelper **pipelineOut) const
    {
        // Search could be improved using sorting or hashing.
        for (const GraphicsPipelineTransition &transition : mTransitions)
        {
            if (GraphicsPipelineTransitionMatch(transition.bits, bits, *transition.desc, desc))
            {
                *pipelineOut = transition.target;
                return true;
            }
        }

        return false;
    }

    void addTransition(GraphicsPipelineTransitionBits bits,
                       const GraphicsPipelineDesc *desc,
                       PipelineHelper *pipeline);

    const std::vector<GraphicsPipelineTransition> getTransitions() const { return mTransitions; }

    void setCacheLookUpFeedback(CacheLookUpFeedback feedback)
    {
        ASSERT(mCacheLookUpFeedback == CacheLookUpFeedback::None);
        mCacheLookUpFeedback = feedback;
    }
    CacheLookUpFeedback getCacheLookUpFeedback() const { return mCacheLookUpFeedback; }

  private:
    std::vector<GraphicsPipelineTransition> mTransitions;
    Pipeline mPipeline;
    CacheLookUpFeedback mCacheLookUpFeedback = CacheLookUpFeedback::None;
};

class FramebufferHelper : public Resource
{
  public:
    FramebufferHelper();
    ~FramebufferHelper() override;

    FramebufferHelper(FramebufferHelper &&other);
    FramebufferHelper &operator=(FramebufferHelper &&other);

    angle::Result init(ContextVk *contextVk, const VkFramebufferCreateInfo &createInfo);
    void destroy(RendererVk *rendererVk);
    void release(ContextVk *contextVk);

    bool valid() { return mFramebuffer.valid(); }

    const Framebuffer &getFramebuffer() const
    {
        ASSERT(mFramebuffer.valid());
        return mFramebuffer;
    }

    Framebuffer &getFramebuffer()
    {
        ASSERT(mFramebuffer.valid());
        return mFramebuffer;
    }

  private:
    // Vulkan object.
    Framebuffer mFramebuffer;
};

ANGLE_INLINE PipelineHelper::PipelineHelper(Pipeline &&pipeline, CacheLookUpFeedback feedback)
    : mPipeline(std::move(pipeline)), mCacheLookUpFeedback(feedback)
{}

struct ImageSubresourceRange
{
    // GL max is 1000 (fits in 10 bits).
    uint32_t level : 10;
    // Max 31 levels (2 ** 5 - 1). Can store levelCount-1 if we need to save another bit.
    uint32_t levelCount : 5;
    // Implementation max is 2048 (11 bits).
    uint32_t layer : 12;
    // One of vk::LayerMode values.  If 0, it means all layers.  Otherwise it's the count of layers
    // which is usually 1, except for multiview in which case it can be up to
    // gl::IMPLEMENTATION_MAX_2D_ARRAY_TEXTURE_LAYERS.
    uint32_t layerMode : 3;
    // Values from vk::SrgbDecodeMode.  Unused with draw views.
    uint32_t srgbDecodeMode : 1;
    // For read views: Values from gl::SrgbOverride, either Default or SRGB.
    // For draw views: Values from gl::SrgbWriteControlMode.
    uint32_t srgbMode : 1;

    static_assert(gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS < (1 << 5),
                  "Not enough bits for level count");
    static_assert(gl::IMPLEMENTATION_MAX_2D_ARRAY_TEXTURE_LAYERS <= (1 << 12),
                  "Not enough bits for layer index");
    static_assert(gl::IMPLEMENTATION_ANGLE_MULTIVIEW_MAX_VIEWS <= (1 << 3),
                  "Not enough bits for layer count");
};

static_assert(sizeof(ImageSubresourceRange) == sizeof(uint32_t), "Size mismatch");

inline bool operator==(const ImageSubresourceRange &a, const ImageSubresourceRange &b)
{
    return a.level == b.level && a.levelCount == b.levelCount && a.layer == b.layer &&
           a.layerMode == b.layerMode && a.srgbDecodeMode == b.srgbDecodeMode &&
           a.srgbMode == b.srgbMode;
}

constexpr ImageSubresourceRange kInvalidImageSubresourceRange = {0, 0, 0, 0, 0, 0};

struct ImageOrBufferViewSubresourceSerial
{
    ImageOrBufferViewSerial viewSerial;
    ImageSubresourceRange subresource;
};

inline bool operator==(const ImageOrBufferViewSubresourceSerial &a,
                       const ImageOrBufferViewSubresourceSerial &b)
{
    return a.viewSerial == b.viewSerial && a.subresource == b.subresource;
}

constexpr ImageOrBufferViewSubresourceSerial kInvalidImageOrBufferViewSubresourceSerial = {
    kInvalidImageOrBufferViewSerial, kInvalidImageSubresourceRange};

// Always starts with array element zero, with descriptorCount descriptors.
struct WriteDescriptorDesc
{
    uint8_t binding;              // Redundant: determined by the containing WriteDesc array.
    uint8_t descriptorCount;      // Number of array elements in this descriptor write.
    uint8_t descriptorType;       // Packed VkDescriptorType.
    uint8_t descriptorInfoIndex;  // Base index into an array of DescriptorInfoDescs.
};

static_assert(sizeof(WriteDescriptorDesc) == 4, "Size mismatch");

struct DescriptorInfoDesc
{
    uint32_t samplerOrBufferSerial;
    uint32_t imageViewSerialOrOffset;
    uint32_t imageLayoutOrRange;  // Packed VkImageLayout
    uint32_t imageSubresourceRange;
};

static_assert(sizeof(DescriptorInfoDesc) == 16, "Size mismatch");

// Generic description of a descriptor set. Used as a key when indexing descriptor set caches. The
// key storage is an angle:FixedVector. Beyond a certain fixed size we'll end up using heap memory
// to store keys. Currently we specialize the structure for three use cases: uniforms, textures,
// and other shader resources. Because of the way the specialization works we can't currently cache
// programs that use some types of resources.
static constexpr size_t kFastDescriptorSetDescLimit = 8;

struct DescriptorDescHandles
{
    VkBuffer buffer;
    VkSampler sampler;
    VkImageView imageView;
    VkBufferView bufferView;
};

class DescriptorSetDesc
{
  public:
    DescriptorSetDesc()  = default;
    ~DescriptorSetDesc() = default;

    DescriptorSetDesc(const DescriptorSetDesc &other)
        : mWriteDescriptors(other.mWriteDescriptors), mDescriptorInfos(other.mDescriptorInfos)
    {}

    DescriptorSetDesc &operator=(const DescriptorSetDesc &other)
    {
        mWriteDescriptors = other.mWriteDescriptors;
        mDescriptorInfos  = other.mDescriptorInfos;
        return *this;
    }

    size_t hash() const;

    void reset()
    {
        mWriteDescriptors.clear();
        mDescriptorInfos.clear();
    }

    size_t getKeySizeBytes() const
    {
        return mWriteDescriptors.size() * sizeof(WriteDescriptorDesc) +
               mDescriptorInfos.size() * sizeof(DescriptorInfoDesc);
    }

    bool operator==(const DescriptorSetDesc &other) const
    {
        return (mDescriptorInfos == other.mDescriptorInfos);
    }

    bool hasWriteDescAtIndex(uint32_t bindingIndex) const
    {
        return bindingIndex < mWriteDescriptors.size() &&
               mWriteDescriptors[bindingIndex].descriptorCount > 0;
    }

    // Returns the info desc offset.
    void updateWriteDesc(const WriteDescriptorDesc &writeDesc);

    void updateInfoDesc(uint32_t infoDescIndex, const DescriptorInfoDesc &infoDesc)
    {
        mDescriptorInfos[infoDescIndex] = infoDesc;
    }

    void incrementDescriptorCount(uint32_t bindingIndex, uint32_t count)
    {
        // Validate we have no subsequent writes.
        ASSERT(hasWriteDescAtIndex(bindingIndex));
        mWriteDescriptors[bindingIndex].descriptorCount += count;
    }

    uint32_t getDescriptorSetCount(uint32_t bindingIndex) const
    {
        ASSERT(hasWriteDescAtIndex(bindingIndex));
        return mWriteDescriptors[bindingIndex].descriptorCount;
    }

    void updateDescriptorSet(UpdateDescriptorSetsBuilder *updateBuilder,
                             const DescriptorDescHandles *handles,
                             VkDescriptorSet descriptorSet) const;

    bool empty() const { return mWriteDescriptors.size() == 0; }

    void streamOut(std::ostream &os) const;

    uint32_t getInfoDescIndex(uint32_t bindingIndex) const
    {
        return mWriteDescriptors[bindingIndex].descriptorInfoIndex;
    }

  private:
    // After a preliminary minimum size, use heap memory.
    angle::FastMap<WriteDescriptorDesc, kFastDescriptorSetDescLimit> mWriteDescriptors;
    angle::FastMap<DescriptorInfoDesc, kFastDescriptorSetDescLimit> mDescriptorInfos;
};

class DescriptorPoolHelper;
using RefCountedDescriptorPoolHelper = RefCounted<DescriptorPoolHelper>;

// SharedDescriptorSetCacheKey.
// Because DescriptorSet must associate with a pool, we need to define a structure that wraps both.
struct DescriptorSetDescAndPool
{
    DescriptorSetDesc mDesc;
    DynamicDescriptorPool *mPool;
};
using DescriptorSetAndPoolPointer = std::unique_ptr<DescriptorSetDescAndPool>;
using SharedDescriptorSetCacheKey = std::shared_ptr<DescriptorSetAndPoolPointer>;
ANGLE_INLINE const SharedDescriptorSetCacheKey
CreateSharedDescriptorSetCacheKey(const DescriptorSetDesc &desc, DynamicDescriptorPool *pool)
{
    DescriptorSetAndPoolPointer DescriptorAndPoolPointer =
        std::make_unique<DescriptorSetDescAndPool>(DescriptorSetDescAndPool{desc, pool});
    return std::make_shared<DescriptorSetAndPoolPointer>(std::move(DescriptorAndPoolPointer));
}

constexpr VkDescriptorType kStorageBufferDescriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;

// Manages a descriptor set desc with a few helper routines and also stores object handles.
class DescriptorSetDescBuilder final
{
  public:
    DescriptorSetDescBuilder();
    ~DescriptorSetDescBuilder();

    DescriptorSetDescBuilder(const DescriptorSetDescBuilder &other);
    DescriptorSetDescBuilder &operator=(const DescriptorSetDescBuilder &other);

    const DescriptorSetDesc &getDesc() const { return mDesc; }

    void reset();

    // Specific helpers for uniforms/xfb descriptors.
    void updateUniformWrite(uint32_t shaderStageCount);
    void updateUniformBuffer(uint32_t shaderIndex,
                             const BufferHelper &bufferHelper,
                             VkDeviceSize bufferRange);
    void updateTransformFeedbackWrite(const ShaderInterfaceVariableInfoMap &variableInfoMap,
                                      uint32_t xfbBufferCount);
    void updateTransformFeedbackBuffer(const Context *context,
                                       const ShaderInterfaceVariableInfoMap &variableInfoMap,
                                       uint32_t xfbBufferIndex,
                                       const BufferHelper &bufferHelper,
                                       VkDeviceSize bufferOffset,
                                       VkDeviceSize bufferRange);

    void updateUniformsAndXfb(Context *context,
                              const gl::ProgramExecutable &executable,
                              const ProgramExecutableVk &executableVk,
                              const BufferHelper *currentUniformBuffer,
                              const BufferHelper &emptyBuffer,
                              bool activeUnpaused,
                              TransformFeedbackVk *transformFeedbackVk);

    // Specific helpers for shader resource descriptors.
    void updateShaderBuffers(gl::ShaderType shaderType,
                             ShaderVariableType variableType,
                             const ShaderInterfaceVariableInfoMap &variableInfoMap,
                             const gl::BufferVector &buffers,
                             const std::vector<gl::InterfaceBlock> &blocks,
                             VkDescriptorType descriptorType,
                             VkDeviceSize maxBoundBufferRange,
                             const BufferHelper &emptyBuffer);
    void updateAtomicCounters(gl::ShaderType shaderType,
                              const ShaderInterfaceVariableInfoMap &variableInfoMap,
                              const gl::BufferVector &buffers,
                              const std::vector<gl::AtomicCounterBuffer> &atomicCounterBuffers,
                              const VkDeviceSize requiredOffsetAlignment,
                              vk::BufferHelper *emptyBuffer);
    angle::Result updateImages(Context *context,
                               gl::ShaderType shaderType,
                               const gl::ProgramExecutable &executable,
                               const ShaderInterfaceVariableInfoMap &variableInfoMap,
                               const gl::ActiveTextureArray<TextureVk *> &activeImages,
                               const std::vector<gl::ImageUnit> &imageUnits);
    angle::Result updateInputAttachments(vk::Context *context,
                                         gl::ShaderType shaderType,
                                         const gl::ProgramExecutable &executable,
                                         const ShaderInterfaceVariableInfoMap &variableInfoMap,
                                         FramebufferVk *framebufferVk);

    // Specific helpers for image descriptors.
    void updatePreCacheActiveTextures(const gl::ActiveTextureMask &activeTextures,
                                      const gl::ActiveTextureArray<TextureVk *> &textures,
                                      const gl::SamplerBindingVector &samplers);

    angle::Result updateFullActiveTextures(Context *context,
                                           const ShaderInterfaceVariableInfoMap &variableInfoMap,
                                           const gl::ProgramExecutable &executable,
                                           const gl::ActiveTextureArray<TextureVk *> &textures,
                                           const gl::SamplerBindingVector &samplers,
                                           bool emulateSeamfulCubeMapSampling,
                                           PipelineType pipelineType,
                                           const SharedDescriptorSetCacheKey &sharedCacheKey);

    void updateDescriptorSet(UpdateDescriptorSetsBuilder *updateBuilder,
                             VkDescriptorSet descriptorSet) const;

    // If sharedCacheKey is not null, it means a new cache entry for descriptoret has been created.
    // This function will store the new shared cache key in the buffer or textures that the
    // descriptorSet is created so that the descriptorSet cache can be destroyed when any of these
    // is release or destroyed.
    void updateImagesAndBuffersWithSharedCacheKey(
        const SharedDescriptorSetCacheKey &sharedCacheKey);

    const uint32_t *getDynamicOffsets() const { return mDynamicOffsets.data(); }
    size_t getDynamicOffsetsSize() const { return mDynamicOffsets.size(); }

  private:
    angle::Result updateExecutableActiveTexturesForShader(
        Context *context,
        gl::ShaderType shaderType,
        const ShaderInterfaceVariableInfoMap &variableInfoMap,
        const gl::ProgramExecutable &executable,
        const gl::ActiveTextureArray<TextureVk *> &textures,
        const gl::SamplerBindingVector &samplers,
        bool emulateSeamfulCubeMapSampling,
        PipelineType pipelineType,
        const SharedDescriptorSetCacheKey &sharedCacheKey);

    void updateWriteDesc(uint32_t bindingIndex,
                         VkDescriptorType descriptorType,
                         uint32_t descriptorCount);

    DescriptorSetDesc mDesc;
    angle::FastMap<DescriptorDescHandles, kFastDescriptorSetDescLimit> mHandles;
    angle::FastMap<uint32_t, kFastDescriptorSetDescLimit> mDynamicOffsets;
    uint32_t mCurrentInfoIndex = 0;

    // Track textures and buffers that used for this descriptorSet.
    std::vector<TextureVk *> mUsedImages;
    std::vector<BufferBlock *> mUsedBufferBlocks;
    std::vector<BufferHelper *> mUsedBufferHelpers;
};

// Specialized update for textures.
void UpdatePreCacheActiveTextures(const std::vector<gl::SamplerBinding> &samplerBindings,
                                  const gl::ActiveTextureMask &activeTextures,
                                  const gl::ActiveTextureArray<TextureVk *> &textures,
                                  const gl::SamplerBindingVector &samplers,
                                  DescriptorSetDesc *desc);

// In the FramebufferDesc object:
//  - Depth/stencil serial is at index 0
//  - Color serials are at indices [1, gl::IMPLEMENTATION_MAX_DRAW_BUFFERS]
//  - Depth/stencil resolve attachment is at index gl::IMPLEMENTATION_MAX_DRAW_BUFFERS+1
//  - Resolve attachments are at indices [gl::IMPLEMENTATION_MAX_DRAW_BUFFERS+2,
//                                        gl::IMPLEMENTATION_MAX_DRAW_BUFFERS*2+1]
constexpr size_t kFramebufferDescDepthStencilIndex = 0;
constexpr size_t kFramebufferDescColorIndexOffset  = kFramebufferDescDepthStencilIndex + 1;
constexpr size_t kFramebufferDescDepthStencilResolveIndexOffset =
    kFramebufferDescColorIndexOffset + gl::IMPLEMENTATION_MAX_DRAW_BUFFERS;
constexpr size_t kFramebufferDescColorResolveIndexOffset =
    kFramebufferDescDepthStencilResolveIndexOffset + 1;

// Enable struct padding warnings for the code below since it is used in caches.
ANGLE_ENABLE_STRUCT_PADDING_WARNINGS

class FramebufferDesc
{
  public:
    FramebufferDesc();
    ~FramebufferDesc();

    FramebufferDesc(const FramebufferDesc &other);
    FramebufferDesc &operator=(const FramebufferDesc &other);

    void updateColor(uint32_t index, ImageOrBufferViewSubresourceSerial serial);
    void updateColorResolve(uint32_t index, ImageOrBufferViewSubresourceSerial serial);
    void updateUnresolveMask(FramebufferNonResolveAttachmentMask unresolveMask);
    void updateDepthStencil(ImageOrBufferViewSubresourceSerial serial);
    void updateDepthStencilResolve(ImageOrBufferViewSubresourceSerial serial);
    ANGLE_INLINE void setWriteControlMode(gl::SrgbWriteControlMode mode)
    {
        mSrgbWriteControlMode = static_cast<uint16_t>(mode);
    }
    void updateIsMultiview(bool isMultiview) { mIsMultiview = isMultiview; }
    size_t hash() const;

    bool operator==(const FramebufferDesc &other) const;

    uint32_t attachmentCount() const;

    ImageOrBufferViewSubresourceSerial getColorImageViewSerial(uint32_t index)
    {
        ASSERT(kFramebufferDescColorIndexOffset + index < mSerials.size());
        return mSerials[kFramebufferDescColorIndexOffset + index];
    }

    FramebufferNonResolveAttachmentMask getUnresolveAttachmentMask() const;
    ANGLE_INLINE gl::SrgbWriteControlMode getWriteControlMode() const
    {
        return (mSrgbWriteControlMode == 1) ? gl::SrgbWriteControlMode::Linear
                                            : gl::SrgbWriteControlMode::Default;
    }

    void updateLayerCount(uint32_t layerCount);
    uint32_t getLayerCount() const { return mLayerCount; }
    void setFramebufferFetchMode(bool hasFramebufferFetch);
    bool hasFramebufferFetch() const { return mHasFramebufferFetch; }

    bool isMultiview() const { return mIsMultiview; }

    void updateRenderToTexture(bool isRenderToTexture);

  private:
    void reset();
    void update(uint32_t index, ImageOrBufferViewSubresourceSerial serial);

    // Note: this is an exclusive index. If there is one index it will be "1".
    // Maximum value is 18
    uint16_t mMaxIndex : 5;
    uint16_t mHasFramebufferFetch : 1;
    static_assert(gl::IMPLEMENTATION_MAX_FRAMEBUFFER_LAYERS < (1 << 9) - 1,
                  "Not enough bits for mLayerCount");

    uint16_t mLayerCount : 9;

    uint16_t mSrgbWriteControlMode : 1;

    // If the render pass contains an initial subpass to unresolve a number of attachments, the
    // subpass description is derived from the following mask, specifying which attachments need
    // to be unresolved.  Includes both color and depth/stencil attachments.
    uint16_t mUnresolveAttachmentMask : kMaxFramebufferNonResolveAttachments;

    // Whether this is a multisampled-render-to-single-sampled framebuffer.  Only used when using
    // VK_EXT_multisampled_render_to_single_sampled.  Only one bit is used and the rest is padding.
    uint16_t mIsRenderToTexture : 15 - kMaxFramebufferNonResolveAttachments;

    uint16_t mIsMultiview : 1;

    FramebufferAttachmentArray<ImageOrBufferViewSubresourceSerial> mSerials;
};

constexpr size_t kFramebufferDescSize = sizeof(FramebufferDesc);
static_assert(kFramebufferDescSize == 148, "Size check failed");

// Disable warnings about struct padding.
ANGLE_DISABLE_STRUCT_PADDING_WARNINGS

// SharedFramebufferCacheKey
using FramebufferDescPointer    = std::unique_ptr<FramebufferDesc>;
using SharedFramebufferCacheKey = std::shared_ptr<FramebufferDescPointer>;
ANGLE_INLINE const SharedFramebufferCacheKey
CreateSharedFramebufferCacheKey(const FramebufferDesc &desc)
{
    FramebufferDescPointer framebufferDescPointer = std::make_unique<FramebufferDesc>(desc);
    return std::make_shared<FramebufferDescPointer>(std::move(framebufferDescPointer));
}

// The SamplerHelper allows a Sampler to be coupled with a serial.
// Must be included before we declare SamplerCache.
class SamplerHelper final : angle::NonCopyable
{
  public:
    SamplerHelper(ContextVk *contextVk);
    ~SamplerHelper();

    explicit SamplerHelper(SamplerHelper &&samplerHelper);
    SamplerHelper &operator=(SamplerHelper &&rhs);

    bool valid() const { return mSampler.valid(); }
    const Sampler &get() const { return mSampler; }
    Sampler &get() { return mSampler; }
    SamplerSerial getSamplerSerial() const { return mSamplerSerial; }

  private:
    Sampler mSampler;
    SamplerSerial mSamplerSerial;
};

using RefCountedSampler = RefCounted<SamplerHelper>;
using SamplerBinding    = BindingPointer<SamplerHelper>;

class RenderPassHelper final : angle::NonCopyable
{
  public:
    RenderPassHelper();
    ~RenderPassHelper();

    RenderPassHelper(RenderPassHelper &&other);
    RenderPassHelper &operator=(RenderPassHelper &&other);

    void destroy(VkDevice device);
    void release(ContextVk *contextVk);

    const RenderPass &getRenderPass() const;
    RenderPass &getRenderPass();

    const RenderPassPerfCounters &getPerfCounters() const;
    RenderPassPerfCounters &getPerfCounters();

  private:
    RenderPass mRenderPass;
    RenderPassPerfCounters mPerfCounters;
};

// Helper class manages the lifetime of various cache objects so that the cache entry can be
// destroyed when one of the components becomes invalid.
template <class SharedCacheKeyT>
class SharedCacheKeyManager
{
  public:
    SharedCacheKeyManager() = default;
    ~SharedCacheKeyManager() { ASSERT(empty()); }
    // Store the pointer to the cache key and retains it
    void addKey(const SharedCacheKeyT &key);
    // Iterate over the descriptor array and release the descriptor and cache.
    void releaseKeys(ContextVk *contextVk);
    // Iterate over the descriptor array and destroy the descriptor and cache.
    void destroyKeys(RendererVk *renderer);
    void clear();

    // The following APIs are expected to be used for assertion only
    bool containsKey(const SharedCacheKeyT &key) const;
    bool empty() const { return mSharedCacheKeys.empty(); }
    void assertAllEntriesDestroyed();

  private:
    // Tracks an array of cache keys with refcounting. Note this owns one refcount of
    // SharedCacheKeyT object.
    std::vector<SharedCacheKeyT> mSharedCacheKeys;
};

using FramebufferCacheManager   = SharedCacheKeyManager<SharedFramebufferCacheKey>;
using DescriptorSetCacheManager = SharedCacheKeyManager<SharedDescriptorSetCacheKey>;
}  // namespace vk
}  // namespace rx

// Introduce std::hash for the above classes.
namespace std
{
template <>
struct hash<rx::vk::RenderPassDesc>
{
    size_t operator()(const rx::vk::RenderPassDesc &key) const { return key.hash(); }
};

template <>
struct hash<rx::vk::AttachmentOpsArray>
{
    size_t operator()(const rx::vk::AttachmentOpsArray &key) const { return key.hash(); }
};

template <>
struct hash<rx::vk::GraphicsPipelineDesc>
{
    size_t operator()(const rx::vk::GraphicsPipelineDesc &key) const { return key.hash(); }
};

template <>
struct hash<rx::vk::DescriptorSetLayoutDesc>
{
    size_t operator()(const rx::vk::DescriptorSetLayoutDesc &key) const { return key.hash(); }
};

template <>
struct hash<rx::vk::PipelineLayoutDesc>
{
    size_t operator()(const rx::vk::PipelineLayoutDesc &key) const { return key.hash(); }
};

template <>
struct hash<rx::vk::ImageSubresourceRange>
{
    size_t operator()(const rx::vk::ImageSubresourceRange &key) const
    {
        return *reinterpret_cast<const uint32_t *>(&key);
    }
};

template <>
struct hash<rx::vk::DescriptorSetDesc>
{
    size_t operator()(const rx::vk::DescriptorSetDesc &key) const { return key.hash(); }
};

template <>
struct hash<rx::vk::FramebufferDesc>
{
    size_t operator()(const rx::vk::FramebufferDesc &key) const { return key.hash(); }
};

template <>
struct hash<rx::vk::YcbcrConversionDesc>
{
    size_t operator()(const rx::vk::YcbcrConversionDesc &key) const { return key.hash(); }
};

template <>
struct hash<rx::vk::SamplerDesc>
{
    size_t operator()(const rx::vk::SamplerDesc &key) const { return key.hash(); }
};

// See Resource Serial types defined in vk_utils.h.
#define ANGLE_HASH_VK_SERIAL(Type)                               \
    template <>                                                  \
    struct hash<rx::vk::Type##Serial>                            \
    {                                                            \
        size_t operator()(const rx::vk::Type##Serial &key) const \
        {                                                        \
            return key.getValue();                               \
        }                                                        \
    };

ANGLE_VK_SERIAL_OP(ANGLE_HASH_VK_SERIAL)

}  // namespace std

namespace rx
{
// Cache types for various Vulkan objects
enum class VulkanCacheType
{
    CompatibleRenderPass,
    RenderPassWithOps,
    GraphicsPipeline,
    PipelineLayout,
    Sampler,
    SamplerYcbcrConversion,
    DescriptorSetLayout,
    DriverUniformsDescriptors,
    TextureDescriptors,
    UniformsAndXfbDescriptors,
    ShaderResourcesDescriptors,
    Framebuffer,
    DescriptorMetaCache,
    EnumCount
};

// Base class for all caches. Provides cache hit and miss counters.
class CacheStats final : angle::NonCopyable
{
  public:
    CacheStats() { reset(); }
    ~CacheStats() {}

    CacheStats(const CacheStats &rhs)
        : mHitCount(rhs.mHitCount), mMissCount(rhs.mMissCount), mSize(rhs.mSize)
    {}

    CacheStats &operator=(const CacheStats &rhs)
    {
        mHitCount  = rhs.mHitCount;
        mMissCount = rhs.mMissCount;
        mSize      = rhs.mSize;
        return *this;
    }

    ANGLE_INLINE void hit() { mHitCount++; }
    ANGLE_INLINE void miss() { mMissCount++; }
    ANGLE_INLINE void incrementSize() { mSize++; }
    ANGLE_INLINE void decrementSize() { mSize--; }
    ANGLE_INLINE void missAndIncrementSize()
    {
        mMissCount++;
        mSize++;
    }
    ANGLE_INLINE void accumulate(const CacheStats &stats)
    {
        mHitCount += stats.mHitCount;
        mMissCount += stats.mMissCount;
        mSize += stats.mSize;
    }

    uint32_t getHitCount() const { return mHitCount; }
    uint32_t getMissCount() const { return mMissCount; }

    ANGLE_INLINE double getHitRatio() const
    {
        if (mHitCount + mMissCount == 0)
        {
            return 0;
        }
        else
        {
            return static_cast<double>(mHitCount) / (mHitCount + mMissCount);
        }
    }

    ANGLE_INLINE uint32_t getSize() const { return mSize; }
    ANGLE_INLINE void setSize(uint32_t size) { mSize = size; }

    void reset()
    {
        mHitCount  = 0;
        mMissCount = 0;
        mSize      = 0;
    }

    void resetHitAndMissCount()
    {
        mHitCount  = 0;
        mMissCount = 0;
    }

    void accumulateCacheStats(VulkanCacheType cacheType, const CacheStats &cacheStats)
    {
        mHitCount += cacheStats.getHitCount();
        mMissCount += cacheStats.getMissCount();
    }

  private:
    uint32_t mHitCount;
    uint32_t mMissCount;
    uint32_t mSize;
};

template <VulkanCacheType CacheType>
class HasCacheStats : angle::NonCopyable
{
  public:
    template <typename Accumulator>
    void accumulateCacheStats(Accumulator *accum)
    {
        accum->accumulateCacheStats(CacheType, mCacheStats);
        mCacheStats.reset();
    }

    void getCacheStats(CacheStats *accum) const { accum->accumulate(mCacheStats); }

  protected:
    HasCacheStats()          = default;
    virtual ~HasCacheStats() = default;

    CacheStats mCacheStats;
};

using VulkanCacheStats = angle::PackedEnumMap<VulkanCacheType, CacheStats>;

// FramebufferVk Cache
class FramebufferCache final : angle::NonCopyable
{
  public:
    FramebufferCache() = default;
    ~FramebufferCache() { ASSERT(mPayload.empty()); }

    void destroy(RendererVk *rendererVk);

    bool get(ContextVk *contextVk, const vk::FramebufferDesc &desc, vk::Framebuffer &framebuffer);
    void insert(const vk::FramebufferDesc &desc, vk::FramebufferHelper &&framebufferHelper);
    void erase(ContextVk *contextVk, const vk::FramebufferDesc &desc);

    size_t getSize() const { return mPayload.size(); }
    bool empty() const { return mPayload.empty(); }

  private:
    angle::HashMap<vk::FramebufferDesc, vk::FramebufferHelper> mPayload;
    CacheStats mCacheStats;
};

// TODO(jmadill): Add cache trimming/eviction.
class RenderPassCache final : angle::NonCopyable
{
  public:
    RenderPassCache();
    ~RenderPassCache();

    void destroy(RendererVk *rendererVk);
    void clear(ContextVk *contextVk);

    ANGLE_INLINE angle::Result getCompatibleRenderPass(ContextVk *contextVk,
                                                       const vk::RenderPassDesc &desc,
                                                       vk::RenderPass **renderPassOut)
    {
        auto outerIt = mPayload.find(desc);
        if (outerIt != mPayload.end())
        {
            InnerCache &innerCache = outerIt->second;
            ASSERT(!innerCache.empty());

            // Find the first element and return it.
            *renderPassOut = &innerCache.begin()->second.getRenderPass();
            mCompatibleRenderPassCacheStats.hit();
            return angle::Result::Continue;
        }

        mCompatibleRenderPassCacheStats.missAndIncrementSize();
        return addRenderPass(contextVk, desc, renderPassOut);
    }

    angle::Result getRenderPassWithOps(ContextVk *contextVk,
                                       const vk::RenderPassDesc &desc,
                                       const vk::AttachmentOpsArray &attachmentOps,
                                       vk::RenderPass **renderPassOut);

  private:
    angle::Result getRenderPassWithOpsImpl(ContextVk *contextVk,
                                           const vk::RenderPassDesc &desc,
                                           const vk::AttachmentOpsArray &attachmentOps,
                                           bool updatePerfCounters,
                                           vk::RenderPass **renderPassOut);

    angle::Result addRenderPass(ContextVk *contextVk,
                                const vk::RenderPassDesc &desc,
                                vk::RenderPass **renderPassOut);

    // Use a two-layer caching scheme. The top level matches the "compatible" RenderPass elements.
    // The second layer caches the attachment load/store ops and initial/final layout.
    // Switch to `std::unordered_map` to retain pointer stability.
    using InnerCache = std::unordered_map<vk::AttachmentOpsArray, vk::RenderPassHelper>;
    using OuterCache = std::unordered_map<vk::RenderPassDesc, InnerCache>;

    OuterCache mPayload;
    CacheStats mCompatibleRenderPassCacheStats;
    CacheStats mRenderPassWithOpsCacheStats;
};

// A class that encapsulates the vk::PipelineCache and associated mutex.  The mutex may be nullptr
// if synchronization is not necessary.
class PipelineCacheAccess
{
  public:
    PipelineCacheAccess()  = default;
    ~PipelineCacheAccess() = default;

    void init(const vk::PipelineCache *pipelineCache, std::mutex *mutex)
    {
        mPipelineCache = pipelineCache;
        mMutex         = mutex;
    }

    angle::Result createGraphicsPipeline(vk::Context *context,
                                         const VkGraphicsPipelineCreateInfo &createInfo,
                                         vk::Pipeline *pipelineOut);
    angle::Result createComputePipeline(vk::Context *context,
                                        const VkComputePipelineCreateInfo &createInfo,
                                        vk::Pipeline *pipelineOut);

    void merge(RendererVk *renderer, const vk::PipelineCache &pipelineCache);

  private:
    std::unique_lock<std::mutex> getLock();

    const vk::PipelineCache *mPipelineCache = nullptr;
    std::mutex *mMutex;
};

enum class PipelineSource
{
    WarmUp,
    Draw,
    Utils,
};

// TODO(jmadill): Add cache trimming/eviction.
class GraphicsPipelineCache final : public HasCacheStats<VulkanCacheType::GraphicsPipeline>
{
  public:
    GraphicsPipelineCache();
    ~GraphicsPipelineCache() override;

    void destroy(RendererVk *rendererVk);
    void release(ContextVk *contextVk);

    void populate(const vk::GraphicsPipelineDesc &desc, vk::Pipeline &&pipeline);

    ANGLE_INLINE angle::Result getPipeline(ContextVk *contextVk,
                                           PipelineCacheAccess *pipelineCache,
                                           const vk::RenderPass &compatibleRenderPass,
                                           const vk::PipelineLayout &pipelineLayout,
                                           const gl::AttributesMask &activeAttribLocationsMask,
                                           const gl::ComponentTypeMask &programAttribsTypeMask,
                                           const gl::DrawBufferMask &missingOutputsMask,
                                           const vk::ShaderAndSerialMap &shaders,
                                           const vk::SpecializationConstants &specConsts,
                                           PipelineSource source,
                                           const vk::GraphicsPipelineDesc &desc,
                                           const vk::GraphicsPipelineDesc **descPtrOut,
                                           vk::PipelineHelper **pipelineOut)
    {
        auto item = mPayload.find(desc);
        if (item != mPayload.end())
        {
            *descPtrOut  = &item->first;
            *pipelineOut = &item->second;
            mCacheStats.hit();
            return angle::Result::Continue;
        }

        mCacheStats.missAndIncrementSize();
        return insertPipeline(contextVk, pipelineCache, compatibleRenderPass, pipelineLayout,
                              activeAttribLocationsMask, programAttribsTypeMask, missingOutputsMask,
                              shaders, specConsts, source, desc, descPtrOut, pipelineOut);
    }

    // Helper for VulkanPipelineCachePerf that resets the object without destroying any object.
    void reset();

  private:
    angle::Result insertPipeline(ContextVk *contextVk,
                                 PipelineCacheAccess *pipelineCache,
                                 const vk::RenderPass &compatibleRenderPass,
                                 const vk::PipelineLayout &pipelineLayout,
                                 const gl::AttributesMask &activeAttribLocationsMask,
                                 const gl::ComponentTypeMask &programAttribsTypeMask,
                                 const gl::DrawBufferMask &missingOutputsMask,
                                 const vk::ShaderAndSerialMap &shaders,
                                 const vk::SpecializationConstants &specConsts,
                                 PipelineSource source,
                                 const vk::GraphicsPipelineDesc &desc,
                                 const vk::GraphicsPipelineDesc **descPtrOut,
                                 vk::PipelineHelper **pipelineOut);

    std::unordered_map<vk::GraphicsPipelineDesc, vk::PipelineHelper> mPayload;
};

class DescriptorSetLayoutCache final : angle::NonCopyable
{
  public:
    DescriptorSetLayoutCache();
    ~DescriptorSetLayoutCache();

    void destroy(RendererVk *rendererVk);

    angle::Result getDescriptorSetLayout(
        vk::Context *context,
        const vk::DescriptorSetLayoutDesc &desc,
        vk::BindingPointer<vk::DescriptorSetLayout> *descriptorSetLayoutOut);

  private:
    std::unordered_map<vk::DescriptorSetLayoutDesc, vk::RefCountedDescriptorSetLayout> mPayload;
    CacheStats mCacheStats;
};

class PipelineLayoutCache final : public HasCacheStats<VulkanCacheType::PipelineLayout>
{
  public:
    PipelineLayoutCache();
    ~PipelineLayoutCache() override;

    void destroy(RendererVk *rendererVk);

    angle::Result getPipelineLayout(vk::Context *context,
                                    const vk::PipelineLayoutDesc &desc,
                                    const vk::DescriptorSetLayoutPointerArray &descriptorSetLayouts,
                                    vk::BindingPointer<vk::PipelineLayout> *pipelineLayoutOut);

  private:
    std::unordered_map<vk::PipelineLayoutDesc, vk::RefCountedPipelineLayout> mPayload;
};

class SamplerCache final : public HasCacheStats<VulkanCacheType::Sampler>
{
  public:
    SamplerCache();
    ~SamplerCache() override;

    void destroy(RendererVk *rendererVk);

    angle::Result getSampler(ContextVk *contextVk,
                             const vk::SamplerDesc &desc,
                             vk::SamplerBinding *samplerOut);

  private:
    std::unordered_map<vk::SamplerDesc, vk::RefCountedSampler> mPayload;
};

// YuvConversion Cache
class SamplerYcbcrConversionCache final
    : public HasCacheStats<VulkanCacheType::SamplerYcbcrConversion>
{
  public:
    SamplerYcbcrConversionCache();
    ~SamplerYcbcrConversionCache() override;

    void destroy(RendererVk *rendererVk);

    angle::Result getSamplerYcbcrConversion(vk::Context *context,
                                            const vk::YcbcrConversionDesc &ycbcrConversionDesc,
                                            VkSamplerYcbcrConversion *vkSamplerYcbcrConversionOut);

  private:
    using SamplerYcbcrConversionMap =
        std::unordered_map<vk::YcbcrConversionDesc, vk::SamplerYcbcrConversion>;
    SamplerYcbcrConversionMap mExternalFormatPayload;
    SamplerYcbcrConversionMap mVkFormatPayload;
};

// Descriptor Set Cache
class DescriptorSetCache final : angle::NonCopyable
{
  public:
    DescriptorSetCache() = default;
    ~DescriptorSetCache() { ASSERT(mPayload.empty()); }

    DescriptorSetCache(DescriptorSetCache &&other) : DescriptorSetCache()
    {
        *this = std::move(other);
    }

    DescriptorSetCache &operator=(DescriptorSetCache &&other)
    {
        std::swap(mPayload, other.mPayload);
        return *this;
    }

    void resetCache() { mPayload.clear(); }

    ANGLE_INLINE bool getDescriptorSet(const vk::DescriptorSetDesc &desc,
                                       VkDescriptorSet *descriptorSetOut,
                                       vk::RefCountedDescriptorPoolHelper **poolOut)
    {
        auto iter = mPayload.find(desc);
        if (iter != mPayload.end())
        {
            *descriptorSetOut = iter->second->getDescriptorSet();
            *poolOut          = iter->second->getPool();
            return true;
        }
        return false;
    }

    ANGLE_INLINE void insertDescriptorSet(const vk::DescriptorSetDesc &desc,
                                          VkDescriptorSet descriptorSet,
                                          vk::RefCountedDescriptorPoolHelper *pool)
    {
        mPayload.emplace(desc, std::make_unique<dsCacheEntry>(descriptorSet, pool));
    }

    ANGLE_INLINE void eraseDescriptorSet(const vk::DescriptorSetDesc &desc)
    {
        mPayload.erase(desc);
    }

    ANGLE_INLINE size_t getTotalCacheSize() const { return mPayload.size(); }

    size_t getTotalCacheKeySizeBytes() const
    {
        size_t totalSize = 0;
        for (const auto &iter : mPayload)
        {
            const vk::DescriptorSetDesc &desc = iter.first;
            totalSize += desc.getKeySizeBytes();
        }
        return totalSize;
    }

    bool empty() const { return mPayload.empty(); }

  private:
    class dsCacheEntry
    {
      public:
        dsCacheEntry(VkDescriptorSet descriptorSet, vk::RefCountedDescriptorPoolHelper *pool)
            : mDescriptorSet(descriptorSet), mPool(pool)
        {}
        VkDescriptorSet getDescriptorSet() const { return mDescriptorSet; }
        vk::RefCountedDescriptorPoolHelper *getPool() const { return mPool; }

      private:
        VkDescriptorSet mDescriptorSet;
        // Weak pointer to the pool this descriptorSet allocated from. The RefCount is tracking if
        // this pool is bound as the current pool in any ProgramExecutableVk or not, so we should
        // not add refcount from the cache.
        vk::RefCountedDescriptorPoolHelper *mPool;
    };
    angle::HashMap<vk::DescriptorSetDesc, std::unique_ptr<dsCacheEntry>> mPayload;
};

// Only 1 driver uniform binding is used.
constexpr uint32_t kReservedDriverUniformBindingCount = 1;
// There is 1 default uniform binding used per stage.  Currently, a maximum of three stages are
// supported.
constexpr uint32_t kReservedPerStageDefaultUniformBindingCount = 1;
constexpr uint32_t kReservedDefaultUniformBindingCount         = 3;
}  // namespace rx

#endif  // LIBANGLE_RENDERER_VULKAN_VK_CACHE_UTILS_H_
