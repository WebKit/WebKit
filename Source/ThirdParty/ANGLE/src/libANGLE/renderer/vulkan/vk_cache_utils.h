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
#include "libANGLE/renderer/vulkan/vk_utils.h"

namespace rx
{

namespace vk
{
class ImageHelper;

using RenderPassAndSerial = ObjectAndSerial<RenderPass>;
using PipelineAndSerial   = ObjectAndSerial<Pipeline>;

using RefCountedDescriptorSetLayout = RefCounted<DescriptorSetLayout>;
using RefCountedPipelineLayout      = RefCounted<PipelineLayout>;

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
// something likely we will want to investigate as the Vulkan implementation progresses.
//
// Second implementation note: the struct packing is also a bit fragile, and some of the
// packing requirements depend on using alignas and field ordering to get the result of
// packing nicely into the desired space. This is something we could also potentially fix
// with a redesign to use bitfields or bit mask operations.

// Enable struct padding warnings for the code below since it is used in caches.
ANGLE_ENABLE_STRUCT_PADDING_WARNINGS

class alignas(4) RenderPassDesc final
{
  public:
    RenderPassDesc();
    ~RenderPassDesc();
    RenderPassDesc(const RenderPassDesc &other);
    RenderPassDesc &operator=(const RenderPassDesc &other);

    // The caller must pack the depth/stencil attachment last.
    void packAttachment(const Format &format);

    size_t hash() const;

    size_t colorAttachmentCount() const;
    size_t attachmentCount() const;

    void setSamples(GLint samples);

    uint8_t samples() const { return mSamples; }

    angle::FormatID operator[](size_t index) const
    {
        ASSERT(index < gl::IMPLEMENTATION_MAX_DRAW_BUFFERS + 1);
        return static_cast<angle::FormatID>(mAttachmentFormats[index]);
    }

  private:
    uint8_t mSamples;
    uint8_t mColorAttachmentCount : 4;
    uint8_t mDepthStencilAttachmentCount : 4;
    gl::AttachmentArray<uint8_t> mAttachmentFormats;
};

bool operator==(const RenderPassDesc &lhs, const RenderPassDesc &rhs);

constexpr size_t kRenderPassDescSize = sizeof(RenderPassDesc);
static_assert(kRenderPassDescSize == 12, "Size check failed");

struct PackedAttachmentOpsDesc final
{
    // VkAttachmentLoadOp is in range [0, 2], and VkAttachmentStoreOp is in range [0, 1].
    uint16_t loadOp : 2;
    uint16_t storeOp : 1;
    uint16_t stencilLoadOp : 2;
    uint16_t stencilStoreOp : 1;

    // 5-bits to force pad the structure to exactly 2 bytes.  Note that we currently don't support
    // any of the extension layouts, whose values start at 1'000'000'000.
    uint16_t initialLayout : 5;
    uint16_t finalLayout : 5;
};

static_assert(sizeof(PackedAttachmentOpsDesc) == 2, "Size check failed");

class AttachmentOpsArray final
{
  public:
    AttachmentOpsArray();
    ~AttachmentOpsArray();
    AttachmentOpsArray(const AttachmentOpsArray &other);
    AttachmentOpsArray &operator=(const AttachmentOpsArray &other);

    const PackedAttachmentOpsDesc &operator[](size_t index) const;
    PackedAttachmentOpsDesc &operator[](size_t index);

    // Initializes an attachment op with whatever values. Used for compatible RenderPass checks.
    void initDummyOp(size_t index, VkImageLayout initialLayout, VkImageLayout finalLayout);
    // Initialize an attachment op with all load and store operations.
    void initWithLoadStore(size_t index, VkImageLayout initialLayout, VkImageLayout finalLayout);

    size_t hash() const;

  private:
    gl::AttachmentArray<PackedAttachmentOpsDesc> mOps;
};

bool operator==(const AttachmentOpsArray &lhs, const AttachmentOpsArray &rhs);

static_assert(sizeof(AttachmentOpsArray) == 20, "Size check failed");

struct PackedAttribDesc final
{
    uint8_t format;

    // TODO(http://anglebug.com/2672): Emulate divisors greater than UBYTE_MAX.
    uint8_t divisor;

    // Can only take 11 bits on NV.
    uint16_t offset;

    // Although technically stride can be any value in ES 2.0, in practice supporting stride
    // greater than MAX_USHORT should not be that helpful. Note that stride limits are
    // introduced in ES 3.1.
    uint16_t stride;
};

constexpr size_t kPackedAttribDescSize = sizeof(PackedAttribDesc);
static_assert(kPackedAttribDescSize == 6, "Size mismatch");

struct VertexInputAttributes final
{
    PackedAttribDesc attribs[gl::MAX_VERTEX_ATTRIBS];
};

constexpr size_t kVertexInputAttributesSize = sizeof(VertexInputAttributes);
static_assert(kVertexInputAttributesSize == 96, "Size mismatch");

struct RasterizationStateBits final
{
    uint32_t depthClampEnable : 4;
    uint32_t rasterizationDiscardEnable : 4;
    uint32_t polygonMode : 4;
    uint32_t cullMode : 4;
    uint32_t frontFace : 4;
    uint32_t depthBiasEnable : 4;
    uint32_t rasterizationSamples : 4;
    uint32_t sampleShadingEnable : 1;
    uint32_t alphaToCoverageEnable : 1;
    uint32_t alphaToOneEnable : 2;
};

constexpr size_t kRasterizationStateBitsSize = sizeof(RasterizationStateBits);
static_assert(kRasterizationStateBitsSize == 4, "Size check failed");

struct PackedRasterizationAndMultisampleStateInfo final
{
    RasterizationStateBits bits;
    // Padded to ensure there's no gaps in this structure or those that use it.
    float minSampleShading;
    uint32_t sampleMask[gl::MAX_SAMPLE_MASK_WORDS];
    // Note: depth bias clamp is only exposed in a 3.1 extension, but left here for completeness.
    float depthBiasClamp;
    float depthBiasConstantFactor;
    float depthBiasSlopeFactor;
    float lineWidth;
};

constexpr size_t kPackedRasterizationAndMultisampleStateSize =
    sizeof(PackedRasterizationAndMultisampleStateInfo);
static_assert(kPackedRasterizationAndMultisampleStateSize == 32, "Size check failed");

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
    uint8_t compareMask;
    uint8_t writeMask;
};

constexpr size_t kPackedStencilOpSize = sizeof(PackedStencilOpState);
static_assert(kPackedStencilOpSize == 4, "Size check failed");

struct DepthStencilEnableFlags final
{
    uint8_t depthTest : 2;  // these only need one bit each. the extra is used as padding.
    uint8_t depthWrite : 2;
    uint8_t depthBoundsTest : 2;
    uint8_t stencilTest : 2;
};

constexpr size_t kDepthStencilEnableFlagsSize = sizeof(DepthStencilEnableFlags);
static_assert(kDepthStencilEnableFlagsSize == 1, "Size check failed");

struct PackedDepthStencilStateInfo final
{
    DepthStencilEnableFlags enable;
    uint8_t frontStencilReference;
    uint8_t backStencilReference;
    uint8_t depthCompareOp;  // only needs 4 bits. extra used as padding.
    float minDepthBounds;
    float maxDepthBounds;
    PackedStencilOpState front;
    PackedStencilOpState back;
};

constexpr size_t kPackedDepthStencilStateSize = sizeof(PackedDepthStencilStateInfo);
static_assert(kPackedDepthStencilStateSize == 20, "Size check failed");

struct LogicOpState final
{
    uint8_t opEnable : 1;
    uint8_t op : 7;
};

constexpr size_t kLogicOpStateSize = sizeof(LogicOpState);
static_assert(kLogicOpStateSize == 1, "Size check failed");

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

struct PrimitiveState final
{
    uint16_t topology : 15;
    uint16_t restartEnable : 1;
};

constexpr size_t kPrimitiveStateSize = sizeof(PrimitiveState);
static_assert(kPrimitiveStateSize == 2, "Size check failed");

struct PackedInputAssemblyAndColorBlendStateInfo final
{
    uint8_t colorWriteMaskBits[gl::IMPLEMENTATION_MAX_DRAW_BUFFERS / 2];
    PackedColorBlendAttachmentState attachments[gl::IMPLEMENTATION_MAX_DRAW_BUFFERS];
    float blendConstants[4];
    LogicOpState logic;
    uint8_t blendEnableMask;
    PrimitiveState primitive;
};

constexpr size_t kPackedInputAssemblyAndColorBlendStateSize =
    sizeof(PackedInputAssemblyAndColorBlendStateInfo);
static_assert(kPackedInputAssemblyAndColorBlendStateSize == 56, "Size check failed");

constexpr size_t kGraphicsPipelineDescSumOfSizes =
    kVertexInputAttributesSize + kPackedInputAssemblyAndColorBlendStateSize +
    kPackedRasterizationAndMultisampleStateSize + kPackedDepthStencilStateSize +
    kRenderPassDescSize + sizeof(VkViewport) + sizeof(VkRect2D);

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

    void initDefaults();

    // For custom comparisons.
    template <typename T>
    const T *getPtr() const
    {
        return reinterpret_cast<const T *>(this);
    }

    angle::Result initializePipeline(vk::Context *context,
                                     const vk::PipelineCache &pipelineCacheVk,
                                     const RenderPass &compatibleRenderPass,
                                     const PipelineLayout &pipelineLayout,
                                     const gl::AttributesMask &activeAttribLocationsMask,
                                     const ShaderModule *vertexModule,
                                     const ShaderModule *fragmentModule,
                                     Pipeline *pipelineOut) const;

    // Vertex input state. For ES 3.1 this should be separated into binding and attribute.
    void updateVertexInput(GraphicsPipelineTransitionBits *transition,
                           uint32_t attribIndex,
                           GLuint stride,
                           GLuint divisor,
                           VkFormat format,
                           GLuint relativeOffset);

    // Input assembly info
    void updateTopology(GraphicsPipelineTransitionBits *transition, gl::PrimitiveMode drawMode);

    // Raster states
    void updateCullMode(GraphicsPipelineTransitionBits *transition,
                        const gl::RasterizerState &rasterState);
    void updateFrontFace(GraphicsPipelineTransitionBits *transition,
                         const gl::RasterizerState &rasterState,
                         bool invertFrontFace);
    void updateLineWidth(GraphicsPipelineTransitionBits *transition, float lineWidth);

    // RenderPass description.
    const RenderPassDesc &getRenderPassDesc() const { return mRenderPassDesc; }

    void setRenderPassDesc(const RenderPassDesc &renderPassDesc);
    void updateRenderPassDesc(GraphicsPipelineTransitionBits *transition,
                              const RenderPassDesc &renderPassDesc);

    // Blend states
    void updateBlendEnabled(GraphicsPipelineTransitionBits *transition, bool isBlendEnabled);
    void updateBlendColor(GraphicsPipelineTransitionBits *transition, const gl::ColorF &color);
    void updateBlendFuncs(GraphicsPipelineTransitionBits *transition,
                          const gl::BlendState &blendState);
    void updateBlendEquations(GraphicsPipelineTransitionBits *transition,
                              const gl::BlendState &blendState);
    void setColorWriteMask(VkColorComponentFlags colorComponentFlags,
                           const gl::DrawBufferMask &alphaMask);
    void setSingleColorWriteMask(uint32_t colorIndex, VkColorComponentFlags colorComponentFlags);
    void updateColorWriteMask(GraphicsPipelineTransitionBits *transition,
                              VkColorComponentFlags colorComponentFlags,
                              const gl::DrawBufferMask &alphaMask);

    // Depth/stencil states.
    void setDepthTestEnabled(bool enabled);
    void setDepthWriteEnabled(bool enabled);
    void setDepthFunc(VkCompareOp op);
    void setStencilTestEnabled(bool enabled);
    void setStencilFrontFuncs(uint8_t reference, VkCompareOp compareOp, uint8_t compareMask);
    void setStencilBackFuncs(uint8_t reference, VkCompareOp compareOp, uint8_t compareMask);
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
                                 GLint ref,
                                 const gl::DepthStencilState &depthStencilState);
    void updateStencilBackFuncs(GraphicsPipelineTransitionBits *transition,
                                GLint ref,
                                const gl::DepthStencilState &depthStencilState);
    void updateStencilFrontOps(GraphicsPipelineTransitionBits *transition,
                               const gl::DepthStencilState &depthStencilState);
    void updateStencilBackOps(GraphicsPipelineTransitionBits *transition,
                              const gl::DepthStencilState &depthStencilState);
    void updateStencilFrontWriteMask(GraphicsPipelineTransitionBits *transition,
                                     const gl::DepthStencilState &depthStencilState,
                                     const gl::Framebuffer *drawFramebuffer);
    void updateStencilBackWriteMask(GraphicsPipelineTransitionBits *transition,
                                    const gl::DepthStencilState &depthStencilState,
                                    const gl::Framebuffer *drawFramebuffer);

    // Depth offset.
    void updatePolygonOffsetFillEnabled(GraphicsPipelineTransitionBits *transition, bool enabled);
    void updatePolygonOffset(GraphicsPipelineTransitionBits *transition,
                             const gl::RasterizerState &rasterState);

    // Viewport and scissor.
    void setViewport(const VkViewport &viewport);
    void updateViewport(GraphicsPipelineTransitionBits *transition, const VkViewport &viewport);
    void updateDepthRange(GraphicsPipelineTransitionBits *transition,
                          float nearPlane,
                          float farPlane);
    void setScissor(const VkRect2D &scissor);
    void updateScissor(GraphicsPipelineTransitionBits *transition, const VkRect2D &scissor);

  private:
    VertexInputAttributes mVertexInputAttribs;
    RenderPassDesc mRenderPassDesc;
    PackedRasterizationAndMultisampleStateInfo mRasterizationAndMultisampleStateInfo;
    PackedDepthStencilStateInfo mDepthStencilStateInfo;
    PackedInputAssemblyAndColorBlendStateInfo mInputAssemblyAndColorBlendStateInfo;
    VkViewport mViewport;
    VkRect2D mScissor;
};

// Verify the packed pipeline description has no gaps in the packing.
// This is not guaranteed by the spec, but is validated by a compile-time check.
// No gaps or padding at the end ensures that hashing and memcmp checks will not run
// into uninitialized memory regions.
constexpr size_t kGraphicsPipelineDescSize = sizeof(GraphicsPipelineDesc);
static_assert(kGraphicsPipelineDescSize == kGraphicsPipelineDescSumOfSizes, "Size mismatch");

constexpr uint32_t kMaxDescriptorSetLayoutBindings = gl::IMPLEMENTATION_MAX_ACTIVE_TEXTURES;

using DescriptorSetLayoutBindingVector =
    angle::FixedVector<VkDescriptorSetLayoutBinding, kMaxDescriptorSetLayoutBindings>;

// A packed description of a descriptor set layout. Use similarly to RenderPassDesc and
// GraphicsPipelineDesc. Currently we only need to differentiate layouts based on sampler usage. In
// the future we could generalize this.
class DescriptorSetLayoutDesc final
{
  public:
    DescriptorSetLayoutDesc();
    ~DescriptorSetLayoutDesc();
    DescriptorSetLayoutDesc(const DescriptorSetLayoutDesc &other);
    DescriptorSetLayoutDesc &operator=(const DescriptorSetLayoutDesc &other);

    size_t hash() const;
    bool operator==(const DescriptorSetLayoutDesc &other) const;

    void update(uint32_t bindingIndex, VkDescriptorType type, uint32_t count);

    void unpackBindings(DescriptorSetLayoutBindingVector *bindings) const;

  private:
    struct PackedDescriptorSetBinding
    {
        uint16_t type;   // Stores a packed VkDescriptorType descriptorType.
        uint16_t count;  // Stores a packed uint32_t descriptorCount.
        // We currently make all descriptors available in the VS and FS shaders.
    };

    static_assert(sizeof(PackedDescriptorSetBinding) == sizeof(uint32_t), "Unexpected size");

    // This is a compact representation of a descriptor set layout.
    std::array<PackedDescriptorSetBinding, kMaxDescriptorSetLayoutBindings>
        mPackedDescriptorSetLayout;
};

// The following are for caching descriptor set layouts. Limited to max two descriptor set layouts
// and one push constant per shader stage. This can be extended in the future.
constexpr size_t kMaxDescriptorSetLayouts = 3;
constexpr size_t kMaxPushConstantRanges   = angle::EnumSize<gl::ShaderType>();

struct PackedPushConstantRange
{
    uint32_t offset;
    uint32_t size;
};

template <typename T>
using DescriptorSetLayoutArray = std::array<T, kMaxDescriptorSetLayouts>;
using DescriptorSetLayoutPointerArray =
    DescriptorSetLayoutArray<BindingPointer<DescriptorSetLayout>>;
template <typename T>
using PushConstantRangeArray = std::array<T, kMaxPushConstantRanges>;

class PipelineLayoutDesc final
{
  public:
    PipelineLayoutDesc();
    ~PipelineLayoutDesc();
    PipelineLayoutDesc(const PipelineLayoutDesc &other);
    PipelineLayoutDesc &operator=(const PipelineLayoutDesc &rhs);

    size_t hash() const;
    bool operator==(const PipelineLayoutDesc &other) const;

    void updateDescriptorSetLayout(uint32_t setIndex, const DescriptorSetLayoutDesc &desc);
    void updatePushConstantRange(gl::ShaderType shaderType, uint32_t offset, uint32_t size);

    const PushConstantRangeArray<PackedPushConstantRange> &getPushConstantRanges() const;

  private:
    DescriptorSetLayoutArray<DescriptorSetLayoutDesc> mDescriptorSetLayouts;
    PushConstantRangeArray<PackedPushConstantRange> mPushConstantRanges;

    // Verify the arrays are properly packed.
    static_assert(sizeof(decltype(mDescriptorSetLayouts)) ==
                      (sizeof(DescriptorSetLayoutDesc) * kMaxDescriptorSetLayouts),
                  "Unexpected size");
    static_assert(sizeof(decltype(mPushConstantRanges)) ==
                      (sizeof(PackedPushConstantRange) * kMaxPushConstantRanges),
                  "Unexpected size");
};

// Verify the structure is properly packed.
static_assert(sizeof(PipelineLayoutDesc) ==
                  (sizeof(DescriptorSetLayoutArray<DescriptorSetLayoutDesc>) +
                   sizeof(std::array<PackedPushConstantRange, kMaxPushConstantRanges>)),
              "Unexpected Size");

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

class PipelineHelper final : angle::NonCopyable
{
  public:
    PipelineHelper();
    ~PipelineHelper();
    inline explicit PipelineHelper(Pipeline &&pipeline);

    void destroy(VkDevice device);

    void updateSerial(Serial serial) { mSerial = serial; }
    bool valid() const { return mPipeline.valid(); }
    Serial getSerial() const { return mSerial; }
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

  private:
    std::vector<GraphicsPipelineTransition> mTransitions;
    Serial mSerial;
    Pipeline mPipeline;
};

ANGLE_INLINE PipelineHelper::PipelineHelper(Pipeline &&pipeline) : mPipeline(std::move(pipeline)) {}

}  // namespace vk
}  // namespace rx

// Introduce a std::hash for a RenderPassDesc
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
}  // namespace std

namespace rx
{
// TODO(jmadill): Add cache trimming/eviction.
class RenderPassCache final : angle::NonCopyable
{
  public:
    RenderPassCache();
    ~RenderPassCache();

    void destroy(VkDevice device);

    ANGLE_INLINE angle::Result getCompatibleRenderPass(vk::Context *context,
                                                       Serial serial,
                                                       const vk::RenderPassDesc &desc,
                                                       vk::RenderPass **renderPassOut)
    {
        auto outerIt = mPayload.find(desc);
        if (outerIt != mPayload.end())
        {
            InnerCache &innerCache = outerIt->second;
            ASSERT(!innerCache.empty());

            // Find the first element and return it.
            innerCache.begin()->second.updateSerial(serial);
            *renderPassOut = &innerCache.begin()->second.get();
            return angle::Result::Continue;
        }

        return addRenderPass(context, serial, desc, renderPassOut);
    }

    angle::Result getRenderPassWithOps(vk::Context *context,
                                       Serial serial,
                                       const vk::RenderPassDesc &desc,
                                       const vk::AttachmentOpsArray &attachmentOps,
                                       vk::RenderPass **renderPassOut);

  private:
    angle::Result addRenderPass(vk::Context *context,
                                Serial serial,
                                const vk::RenderPassDesc &desc,
                                vk::RenderPass **renderPassOut);

    // Use a two-layer caching scheme. The top level matches the "compatible" RenderPass elements.
    // The second layer caches the attachment load/store ops and initial/final layout.
    using InnerCache = std::unordered_map<vk::AttachmentOpsArray, vk::RenderPassAndSerial>;
    using OuterCache = std::unordered_map<vk::RenderPassDesc, InnerCache>;

    OuterCache mPayload;
};

// TODO(jmadill): Add cache trimming/eviction.
class GraphicsPipelineCache final : angle::NonCopyable
{
  public:
    GraphicsPipelineCache();
    ~GraphicsPipelineCache();

    void destroy(VkDevice device);
    void release(RendererVk *renderer);

    void populate(const vk::GraphicsPipelineDesc &desc, vk::Pipeline &&pipeline);

    ANGLE_INLINE angle::Result getPipeline(vk::Context *context,
                                           const vk::PipelineCache &pipelineCacheVk,
                                           const vk::RenderPass &compatibleRenderPass,
                                           const vk::PipelineLayout &pipelineLayout,
                                           const gl::AttributesMask &activeAttribLocationsMask,
                                           const vk::ShaderModule *vertexModule,
                                           const vk::ShaderModule *fragmentModule,
                                           const vk::GraphicsPipelineDesc &desc,
                                           const vk::GraphicsPipelineDesc **descPtrOut,
                                           vk::PipelineHelper **pipelineOut)
    {
        auto item = mPayload.find(desc);
        if (item != mPayload.end())
        {
            *descPtrOut  = &item->first;
            *pipelineOut = &item->second;
            return angle::Result::Continue;
        }

        return insertPipeline(context, pipelineCacheVk, compatibleRenderPass, pipelineLayout,
                              activeAttribLocationsMask, vertexModule, fragmentModule, desc,
                              descPtrOut, pipelineOut);
    }

  private:
    angle::Result insertPipeline(vk::Context *context,
                                 const vk::PipelineCache &pipelineCacheVk,
                                 const vk::RenderPass &compatibleRenderPass,
                                 const vk::PipelineLayout &pipelineLayout,
                                 const gl::AttributesMask &activeAttribLocationsMask,
                                 const vk::ShaderModule *vertexModule,
                                 const vk::ShaderModule *fragmentModule,
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

    void destroy(VkDevice device);

    angle::Result getDescriptorSetLayout(
        vk::Context *context,
        const vk::DescriptorSetLayoutDesc &desc,
        vk::BindingPointer<vk::DescriptorSetLayout> *descriptorSetLayoutOut);

  private:
    std::unordered_map<vk::DescriptorSetLayoutDesc, vk::RefCountedDescriptorSetLayout> mPayload;
};

class PipelineLayoutCache final : angle::NonCopyable
{
  public:
    PipelineLayoutCache();
    ~PipelineLayoutCache();

    void destroy(VkDevice device);

    angle::Result getPipelineLayout(vk::Context *context,
                                    const vk::PipelineLayoutDesc &desc,
                                    const vk::DescriptorSetLayoutPointerArray &descriptorSetLayouts,
                                    vk::BindingPointer<vk::PipelineLayout> *pipelineLayoutOut);

  private:
    std::unordered_map<vk::PipelineLayoutDesc, vk::RefCountedPipelineLayout> mPayload;
};

// Some descriptor set and pipeline layout constants.
constexpr uint32_t kVertexUniformsBindingIndex       = 0;
constexpr uint32_t kFragmentUniformsBindingIndex     = 1;
constexpr uint32_t kUniformsDescriptorSetIndex       = 0;
constexpr uint32_t kTextureDescriptorSetIndex        = 1;
constexpr uint32_t kDriverUniformsDescriptorSetIndex = 2;

}  // namespace rx

#endif  // LIBANGLE_RENDERER_VULKAN_VK_CACHE_UTILS_H_
