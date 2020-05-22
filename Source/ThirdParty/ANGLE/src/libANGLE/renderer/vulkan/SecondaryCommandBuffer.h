//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// SecondaryCommandBuffer:
//    Lightweight, CPU-Side command buffers used to hold command state until
//    it has to be submitted to GPU.
//

#ifndef LIBANGLE_RENDERER_VULKAN_SECONDARYCOMMANDBUFFERVK_H_
#define LIBANGLE_RENDERER_VULKAN_SECONDARYCOMMANDBUFFERVK_H_

#include "common/PoolAlloc.h"
#include "libANGLE/renderer/vulkan/vk_headers.h"
#include "libANGLE/renderer/vulkan/vk_wrapper.h"

namespace rx
{

namespace vk
{

namespace priv
{
// NOTE: Please keep command-related enums, stucts, functions
//  and other code dealing with commands in alphabetical order
//  This simplifies searching and updating commands.
enum class CommandID : uint16_t
{
    // Invalid cmd used to mark end of sequence of commands
    Invalid = 0,
    BeginDebugUtilsLabel,
    BeginQuery,
    BeginTransformFeedback,
    BindComputePipeline,
    BindDescriptorSets,
    BindGraphicsPipeline,
    BindIndexBuffer,
    BindTransformFeedbackBuffers,
    BindVertexBuffers,
    BlitImage,
    BufferBarrier,
    ClearAttachments,
    ClearColorImage,
    ClearDepthStencilImage,
    CopyBuffer,
    CopyBufferToImage,
    CopyImage,
    CopyImageToBuffer,
    Dispatch,
    DispatchIndirect,
    Draw,
    DrawIndexed,
    DrawIndexedBaseVertex,
    DrawIndexedIndirect,
    DrawIndexedInstanced,
    DrawIndexedInstancedBaseVertex,
    DrawIndexedInstancedBaseVertexBaseInstance,
    DrawIndirect,
    DrawInstanced,
    DrawInstancedBaseInstance,
    EndDebugUtilsLabel,
    EndQuery,
    EndTransformFeedback,
    ExecutionBarrier,
    FillBuffer,
    ImageBarrier,
    InsertDebugUtilsLabel,
    MemoryBarrier,
    PipelineBarrier,
    PushConstants,
    ResetEvent,
    ResetQueryPool,
    ResolveImage,
    SetEvent,
    WaitEvents,
    WriteTimestamp,
};

#define VERIFY_4_BYTE_ALIGNMENT(StructName) \
    static_assert((sizeof(StructName) % 4) == 0, "Check StructName alignment");

// Structs to encapsulate parameters for different commands
// This makes it easy to know the size of params & to copy params
// TODO: Could optimize the size of some of these structs through bit-packing
//  and customizing sizing based on limited parameter sets used by ANGLE
struct BeginQueryParams
{
    VkQueryPool queryPool;
    uint32_t query;
    VkQueryControlFlags flags;
};
VERIFY_4_BYTE_ALIGNMENT(BeginQueryParams)

struct BeginTransformFeedbackParams
{
    uint32_t bufferCount;
};
VERIFY_4_BYTE_ALIGNMENT(BeginTransformFeedbackParams)

struct BindDescriptorSetParams
{
    VkPipelineLayout layout;
    VkPipelineBindPoint pipelineBindPoint;
    uint32_t firstSet;
    uint32_t descriptorSetCount;
    uint32_t dynamicOffsetCount;
};
VERIFY_4_BYTE_ALIGNMENT(BindDescriptorSetParams)

struct BindIndexBufferParams
{
    VkBuffer buffer;
    VkDeviceSize offset;
    VkIndexType indexType;
};
VERIFY_4_BYTE_ALIGNMENT(BindIndexBufferParams)

struct BindPipelineParams
{
    VkPipeline pipeline;
};
VERIFY_4_BYTE_ALIGNMENT(BindPipelineParams)

struct BindTransformFeedbackBuffersParams
{
    // ANGLE always has firstBinding of 0 so not storing that currently
    uint32_t bindingCount;
};
VERIFY_4_BYTE_ALIGNMENT(BindTransformFeedbackBuffersParams)

using BindVertexBuffersParams = BindTransformFeedbackBuffersParams;

struct BlitImageParams
{
    VkImage srcImage;
    VkImage dstImage;
    VkFilter filter;
    VkImageBlit region;
};
VERIFY_4_BYTE_ALIGNMENT(BlitImageParams)

struct BufferBarrierParams
{
    VkPipelineStageFlags srcStageMask;
    VkPipelineStageFlags dstStageMask;
    VkBufferMemoryBarrier bufferMemoryBarrier;
};
VERIFY_4_BYTE_ALIGNMENT(BufferBarrierParams)

struct ClearAttachmentsParams
{
    uint32_t attachmentCount;
    VkClearRect rect;
};
VERIFY_4_BYTE_ALIGNMENT(ClearAttachmentsParams)

struct ClearColorImageParams
{
    VkImage image;
    VkImageLayout imageLayout;
    VkClearColorValue color;
    VkImageSubresourceRange range;
};
VERIFY_4_BYTE_ALIGNMENT(ClearColorImageParams)

struct ClearDepthStencilImageParams
{
    VkImage image;
    VkImageLayout imageLayout;
    VkClearDepthStencilValue depthStencil;
    VkImageSubresourceRange range;
};
VERIFY_4_BYTE_ALIGNMENT(ClearDepthStencilImageParams)

struct CopyBufferParams
{
    VkBuffer srcBuffer;
    VkBuffer destBuffer;
    uint32_t regionCount;
};
VERIFY_4_BYTE_ALIGNMENT(CopyBufferParams)

struct CopyBufferToImageParams
{
    VkBuffer srcBuffer;
    VkImage dstImage;
    VkImageLayout dstImageLayout;
    VkBufferImageCopy region;
};
VERIFY_4_BYTE_ALIGNMENT(CopyBufferToImageParams)

struct CopyImageParams
{
    VkImage srcImage;
    VkImageLayout srcImageLayout;
    VkImage dstImage;
    VkImageLayout dstImageLayout;
    VkImageCopy region;
};
VERIFY_4_BYTE_ALIGNMENT(CopyImageParams)

struct CopyImageToBufferParams
{
    VkImage srcImage;
    VkImageLayout srcImageLayout;
    VkBuffer dstBuffer;
    VkBufferImageCopy region;
};
VERIFY_4_BYTE_ALIGNMENT(CopyImageToBufferParams)

// This is a common struct used by both begin & insert DebugUtilsLabelEXT() functions
struct DebugUtilsLabelParams
{
    float color[4];
};
VERIFY_4_BYTE_ALIGNMENT(DebugUtilsLabelParams)

struct DispatchParams
{
    uint32_t groupCountX;
    uint32_t groupCountY;
    uint32_t groupCountZ;
};
VERIFY_4_BYTE_ALIGNMENT(DispatchParams)

struct DispatchIndirectParams
{
    VkBuffer buffer;
    VkDeviceSize offset;
};
VERIFY_4_BYTE_ALIGNMENT(DispatchIndirectParams)

struct DrawParams
{
    uint32_t vertexCount;
    uint32_t firstVertex;
};
VERIFY_4_BYTE_ALIGNMENT(DrawParams)

struct DrawIndexedParams
{
    uint32_t indexCount;
};
VERIFY_4_BYTE_ALIGNMENT(DrawIndexedParams)

struct DrawIndexedBaseVertexParams
{
    uint32_t indexCount;
    uint32_t vertexOffset;
};
VERIFY_4_BYTE_ALIGNMENT(DrawIndexedBaseVertexParams)

struct DrawIndexedIndirectParams
{
    VkBuffer buffer;
    VkDeviceSize offset;
};
VERIFY_4_BYTE_ALIGNMENT(DrawIndexedIndirectParams)

struct DrawIndexedInstancedParams
{
    uint32_t indexCount;
    uint32_t instanceCount;
};
VERIFY_4_BYTE_ALIGNMENT(DrawIndexedInstancedParams)

struct DrawIndexedInstancedBaseVertexParams
{
    uint32_t indexCount;
    uint32_t instanceCount;
    uint32_t vertexOffset;
};
VERIFY_4_BYTE_ALIGNMENT(DrawIndexedInstancedBaseVertexParams)

struct DrawIndexedInstancedBaseVertexBaseInstanceParams
{
    uint32_t indexCount;
    uint32_t instanceCount;
    uint32_t firstIndex;
    int32_t vertexOffset;
    uint32_t firstInstance;
};
VERIFY_4_BYTE_ALIGNMENT(DrawIndexedInstancedBaseVertexBaseInstanceParams)

struct DrawIndirectParams
{
    VkBuffer buffer;
    VkDeviceSize offset;
};
VERIFY_4_BYTE_ALIGNMENT(DrawIndirectParams)

struct DrawInstancedParams
{
    uint32_t vertexCount;
    uint32_t instanceCount;
    uint32_t firstVertex;
};
VERIFY_4_BYTE_ALIGNMENT(DrawInstancedParams)

struct DrawInstancedBaseInstanceParams
{
    uint32_t vertexCount;
    uint32_t instanceCount;
    uint32_t firstVertex;
    uint32_t firstInstance;
};
VERIFY_4_BYTE_ALIGNMENT(DrawInstancedBaseInstanceParams)

// A special struct used with commands that don't have params
struct EmptyParams
{};

struct EndQueryParams
{
    VkQueryPool queryPool;
    uint32_t query;
};
VERIFY_4_BYTE_ALIGNMENT(EndQueryParams)

struct EndTransformFeedbackParams
{
    uint32_t bufferCount;
};
VERIFY_4_BYTE_ALIGNMENT(EndTransformFeedbackParams)

struct ExecutionBarrierParams
{
    VkPipelineStageFlags stageMask;
};
VERIFY_4_BYTE_ALIGNMENT(ExecutionBarrierParams)

struct FillBufferParams
{
    VkBuffer dstBuffer;
    VkDeviceSize dstOffset;
    VkDeviceSize size;
    uint32_t data;
};
VERIFY_4_BYTE_ALIGNMENT(FillBufferParams)

struct ImageBarrierParams
{
    VkPipelineStageFlags srcStageMask;
    VkPipelineStageFlags dstStageMask;
    VkImageMemoryBarrier imageMemoryBarrier;
};
VERIFY_4_BYTE_ALIGNMENT(ImageBarrierParams)

struct MemoryBarrierParams
{
    VkPipelineStageFlags srcStageMask;
    VkPipelineStageFlags dstStageMask;
    VkMemoryBarrier memoryBarrier;
};
VERIFY_4_BYTE_ALIGNMENT(MemoryBarrierParams)

struct PipelineBarrierParams
{
    VkPipelineStageFlags srcStageMask;
    VkPipelineStageFlags dstStageMask;
    VkDependencyFlags dependencyFlags;
    uint32_t memoryBarrierCount;
    uint32_t bufferMemoryBarrierCount;
    uint32_t imageMemoryBarrierCount;
};
VERIFY_4_BYTE_ALIGNMENT(PipelineBarrierParams)

struct PushConstantsParams
{
    VkPipelineLayout layout;
    VkShaderStageFlags flag;
    uint32_t offset;
    uint32_t size;
};
VERIFY_4_BYTE_ALIGNMENT(PushConstantsParams)

struct ResetEventParams
{
    VkEvent event;
    VkPipelineStageFlags stageMask;
};
VERIFY_4_BYTE_ALIGNMENT(ResetEventParams)

struct ResetQueryPoolParams
{
    VkQueryPool queryPool;
    uint32_t firstQuery;
    uint32_t queryCount;
};
VERIFY_4_BYTE_ALIGNMENT(ResetQueryPoolParams)

struct ResolveImageParams
{
    VkImage srcImage;
    VkImage dstImage;
    VkImageResolve region;
};
VERIFY_4_BYTE_ALIGNMENT(ResolveImageParams)

struct SetEventParams
{
    VkEvent event;
    VkPipelineStageFlags stageMask;
};
VERIFY_4_BYTE_ALIGNMENT(SetEventParams)

struct WaitEventsParams
{
    uint32_t eventCount;
    VkPipelineStageFlags srcStageMask;
    VkPipelineStageFlags dstStageMask;
    uint32_t memoryBarrierCount;
    uint32_t bufferMemoryBarrierCount;
    uint32_t imageMemoryBarrierCount;
};
VERIFY_4_BYTE_ALIGNMENT(WaitEventsParams)

struct WriteTimestampParams
{
    VkPipelineStageFlagBits pipelineStage;
    VkQueryPool queryPool;
    uint32_t query;
};
VERIFY_4_BYTE_ALIGNMENT(WriteTimestampParams)

// Header for every cmd in custom cmd buffer
struct CommandHeader
{
    CommandID id;
    uint16_t size;
};
static_assert(sizeof(CommandHeader) == 4, "Check CommandHeader size");

template <typename DestT, typename T>
ANGLE_INLINE DestT *Offset(T *ptr, size_t bytes)
{
    return reinterpret_cast<DestT *>((reinterpret_cast<uint8_t *>(ptr) + bytes));
}

template <typename DestT, typename T>
ANGLE_INLINE const DestT *Offset(const T *ptr, size_t bytes)
{
    return reinterpret_cast<const DestT *>((reinterpret_cast<const uint8_t *>(ptr) + bytes));
}

class SecondaryCommandBuffer final : angle::NonCopyable
{
  public:
    SecondaryCommandBuffer();
    ~SecondaryCommandBuffer();

    static bool SupportsQueries(const VkPhysicalDeviceFeatures &features) { return true; }

    // SecondaryCommandBuffer replays its commands inline when executed on the primary command
    // buffer.
    static constexpr bool ExecutesInline() { return true; }

    // Add commands
    void beginDebugUtilsLabelEXT(const VkDebugUtilsLabelEXT &label);

    void beginQuery(VkQueryPool queryPool, uint32_t query, VkQueryControlFlags flags);

    void beginTransformFeedback(uint32_t counterBufferCount, const VkBuffer *pCounterBuffers);

    void bindComputePipeline(const Pipeline &pipeline);

    void bindDescriptorSets(const PipelineLayout &layout,
                            VkPipelineBindPoint pipelineBindPoint,
                            uint32_t firstSet,
                            uint32_t descriptorSetCount,
                            const VkDescriptorSet *descriptorSets,
                            uint32_t dynamicOffsetCount,
                            const uint32_t *dynamicOffsets);

    void bindGraphicsPipeline(const Pipeline &pipeline);

    void bindIndexBuffer(const Buffer &buffer, VkDeviceSize offset, VkIndexType indexType);

    void bindTransformFeedbackBuffers(uint32_t bindingCount,
                                      const VkBuffer *buffers,
                                      const VkDeviceSize *offsets,
                                      const VkDeviceSize *sizes);

    void bindVertexBuffers(uint32_t firstBinding,
                           uint32_t bindingCount,
                           const VkBuffer *buffers,
                           const VkDeviceSize *offsets);

    void blitImage(const Image &srcImage,
                   VkImageLayout srcImageLayout,
                   const Image &dstImage,
                   VkImageLayout dstImageLayout,
                   uint32_t regionCount,
                   const VkImageBlit *regions,
                   VkFilter filter);

    void bufferBarrier(VkPipelineStageFlags srcStageMask,
                       VkPipelineStageFlags dstStageMask,
                       const VkBufferMemoryBarrier *bufferMemoryBarrier);

    void clearAttachments(uint32_t attachmentCount,
                          const VkClearAttachment *attachments,
                          uint32_t rectCount,
                          const VkClearRect *rects);

    void clearColorImage(const Image &image,
                         VkImageLayout imageLayout,
                         const VkClearColorValue &color,
                         uint32_t rangeCount,
                         const VkImageSubresourceRange *ranges);

    void clearDepthStencilImage(const Image &image,
                                VkImageLayout imageLayout,
                                const VkClearDepthStencilValue &depthStencil,
                                uint32_t rangeCount,
                                const VkImageSubresourceRange *ranges);

    void copyBuffer(const Buffer &srcBuffer,
                    const Buffer &destBuffer,
                    uint32_t regionCount,
                    const VkBufferCopy *regions);

    void copyBufferToImage(VkBuffer srcBuffer,
                           const Image &dstImage,
                           VkImageLayout dstImageLayout,
                           uint32_t regionCount,
                           const VkBufferImageCopy *regions);

    void copyImage(const Image &srcImage,
                   VkImageLayout srcImageLayout,
                   const Image &dstImage,
                   VkImageLayout dstImageLayout,
                   uint32_t regionCount,
                   const VkImageCopy *regions);

    void copyImageToBuffer(const Image &srcImage,
                           VkImageLayout srcImageLayout,
                           VkBuffer dstBuffer,
                           uint32_t regionCount,
                           const VkBufferImageCopy *regions);

    void dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ);

    void dispatchIndirect(const Buffer &buffer, VkDeviceSize offset);

    void draw(uint32_t vertexCount, uint32_t firstVertex);

    void drawIndexed(uint32_t indexCount);
    void drawIndexedBaseVertex(uint32_t indexCount, uint32_t vertexOffset);
    void drawIndexedIndirect(const Buffer &buffer,
                             VkDeviceSize offset,
                             uint32_t drawCount,
                             uint32_t stride);
    void drawIndexedInstanced(uint32_t indexCount, uint32_t instanceCount);
    void drawIndexedInstancedBaseVertex(uint32_t indexCount,
                                        uint32_t instanceCount,
                                        uint32_t vertexOffset);
    void drawIndexedInstancedBaseVertexBaseInstance(uint32_t indexCount,
                                                    uint32_t instanceCount,
                                                    uint32_t firstIndex,
                                                    int32_t vertexOffset,
                                                    uint32_t firstInstance);

    void drawIndirect(const Buffer &buffer,
                      VkDeviceSize offset,
                      uint32_t drawCount,
                      uint32_t stride);

    void drawInstanced(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex);
    void drawInstancedBaseInstance(uint32_t vertexCount,
                                   uint32_t instanceCount,
                                   uint32_t firstVertex,
                                   uint32_t firstInstance);

    void endDebugUtilsLabelEXT();

    void endQuery(VkQueryPool queryPool, uint32_t query);

    void endTransformFeedback(uint32_t counterBufferCount, const VkBuffer *pCounterBuffers);

    void executionBarrier(VkPipelineStageFlags stageMask);

    void fillBuffer(const Buffer &dstBuffer,
                    VkDeviceSize dstOffset,
                    VkDeviceSize size,
                    uint32_t data);

    void imageBarrier(VkPipelineStageFlags srcStageMask,
                      VkPipelineStageFlags dstStageMask,
                      const VkImageMemoryBarrier &imageMemoryBarrier);

    void insertDebugUtilsLabelEXT(const VkDebugUtilsLabelEXT &label);

    void memoryBarrier(VkPipelineStageFlags srcStageMask,
                       VkPipelineStageFlags dstStageMask,
                       const VkMemoryBarrier *memoryBarrier);

    void pipelineBarrier(VkPipelineStageFlags srcStageMask,
                         VkPipelineStageFlags dstStageMask,
                         VkDependencyFlags dependencyFlags,
                         uint32_t memoryBarrierCount,
                         const VkMemoryBarrier *memoryBarriers,
                         uint32_t bufferMemoryBarrierCount,
                         const VkBufferMemoryBarrier *bufferMemoryBarriers,
                         uint32_t imageMemoryBarrierCount,
                         const VkImageMemoryBarrier *imageMemoryBarriers);

    void pushConstants(const PipelineLayout &layout,
                       VkShaderStageFlags flag,
                       uint32_t offset,
                       uint32_t size,
                       const void *data);

    void resetEvent(VkEvent event, VkPipelineStageFlags stageMask);

    // Store up resetQueryPool command and prepend to commands when executing
    void queueResetQueryPool(VkQueryPool queryPool, uint32_t firstQuery, uint32_t queryCount);

    void resetQueryPool(VkQueryPool queryPool, uint32_t firstQuery, uint32_t queryCount);

    void resolveImage(const Image &srcImage,
                      VkImageLayout srcImageLayout,
                      const Image &dstImage,
                      VkImageLayout dstImageLayout,
                      uint32_t regionCount,
                      const VkImageResolve *regions);

    void setEvent(VkEvent event, VkPipelineStageFlags stageMask);

    void waitEvents(uint32_t eventCount,
                    const VkEvent *events,
                    VkPipelineStageFlags srcStageMask,
                    VkPipelineStageFlags dstStageMask,
                    uint32_t memoryBarrierCount,
                    const VkMemoryBarrier *memoryBarriers,
                    uint32_t bufferMemoryBarrierCount,
                    const VkBufferMemoryBarrier *bufferMemoryBarriers,
                    uint32_t imageMemoryBarrierCount,
                    const VkImageMemoryBarrier *imageMemoryBarriers);

    void writeTimestamp(VkPipelineStageFlagBits pipelineStage,
                        VkQueryPool queryPool,
                        uint32_t query);

    // No-op for compatibility
    VkResult end() { return VK_SUCCESS; }

    // Parse the cmds in this cmd buffer into given primary cmd buffer for execution
    void executeCommands(VkCommandBuffer cmdBuffer);
    // If resetQueryPoolCommands are queued, call this to execute them all
    //  This should only be called on a cmdBuffer without an active renderPass
    void executeQueuedResetQueryPoolCommands(VkCommandBuffer cmdBuffer);

    // Calculate memory usage of this command buffer for diagnostics.
    void getMemoryUsageStats(size_t *usedMemoryOut, size_t *allocatedMemoryOut) const;

    // Traverse the list of commands and build a summary for diagnostics.
    std::string dumpCommands(const char *separator) const;

    // Pool Alloc uses 16kB pages w/ 16byte header = 16368bytes. To minimize waste
    //  using a 16368/12 = 1364. Also better perf than 1024 due to fewer block allocations
    static constexpr size_t kBlockSize = 1364;
    // Make sure block size is 4-byte aligned to avoid Android errors
    static_assert((kBlockSize % 4) == 0, "Check kBlockSize alignment");

    // Initialize the SecondaryCommandBuffer by setting the allocator it will use
    void initialize(angle::PoolAllocator *allocator)
    {
        ASSERT(allocator);
        mAllocator = allocator;
        allocateNewBlock();
        // Set first command to Invalid to start
        reinterpret_cast<CommandHeader *>(mCurrentWritePointer)->id = CommandID::Invalid;
    }

    void reset()
    {
        mCommands.clear();
        initialize(mAllocator);
        mResetQueryQueue.clear();
    }

    // This will cause the SecondaryCommandBuffer to become invalid by clearing its allocator
    void releaseHandle() { mAllocator = nullptr; }
    // The SecondaryCommandBuffer is valid if it's been initialized
    bool valid() const { return mAllocator != nullptr; }

    static bool CanKnowIfEmpty() { return true; }
    bool empty() const { return mCommands.size() == 0 || mCommands[0]->id == CommandID::Invalid; }

  private:
    void commonDebugUtilsLabel(CommandID cmd, const VkDebugUtilsLabelEXT &label);
    template <class StructType>
    ANGLE_INLINE StructType *commonInit(CommandID cmdID, size_t allocationSize)
    {
        mCurrentBytesRemaining -= allocationSize;

        CommandHeader *header = reinterpret_cast<CommandHeader *>(mCurrentWritePointer);
        header->id            = cmdID;
        header->size          = static_cast<uint16_t>(allocationSize);
        ASSERT(allocationSize <= std::numeric_limits<uint16_t>::max());

        mCurrentWritePointer += allocationSize;
        // Set next cmd header to Invalid (0) so cmd sequence will be terminated
        reinterpret_cast<CommandHeader *>(mCurrentWritePointer)->id = CommandID::Invalid;
        return Offset<StructType>(header, sizeof(CommandHeader));
    }
    ANGLE_INLINE void allocateNewBlock(size_t blockSize = kBlockSize)
    {
        ASSERT(mAllocator);
        mCurrentWritePointer   = mAllocator->fastAllocate(blockSize);
        mCurrentBytesRemaining = blockSize;
        mCommands.push_back(reinterpret_cast<CommandHeader *>(mCurrentWritePointer));
    }

    // Allocate and initialize memory for given commandID & variable param size, setting
    // variableDataPtr to the byte following fixed cmd data where variable-sized ptr data will
    // be written and returning a pointer to the start of the command's parameter data
    template <class StructType>
    ANGLE_INLINE StructType *initCommand(CommandID cmdID,
                                         size_t variableSize,
                                         uint8_t **variableDataPtr)
    {
        constexpr size_t fixedAllocationSize = sizeof(StructType) + sizeof(CommandHeader);
        const size_t allocationSize          = fixedAllocationSize + variableSize;
        // Make sure we have enough room to mark follow-on header "Invalid"
        const size_t requiredSize = allocationSize + sizeof(CommandHeader);
        if (mCurrentBytesRemaining < requiredSize)
        {
            // variable size command can potentially exceed default cmd allocation blockSize
            if (requiredSize <= kBlockSize)
                allocateNewBlock();
            else
            {
                // Make sure allocation is 4-byte aligned
                const size_t alignedSize = roundUp<size_t>(requiredSize, 4);
                ASSERT((alignedSize % 4) == 0);
                allocateNewBlock(alignedSize);
            }
        }
        *variableDataPtr = Offset<uint8_t>(mCurrentWritePointer, fixedAllocationSize);
        return commonInit<StructType>(cmdID, allocationSize);
    }

    // Initialize a command that doesn't have variable-sized ptr data
    template <class StructType>
    ANGLE_INLINE StructType *initCommand(CommandID cmdID)
    {
        constexpr size_t paramSize =
            std::is_same<StructType, EmptyParams>::value ? 0 : sizeof(StructType);
        constexpr size_t allocationSize = paramSize + sizeof(CommandHeader);
        // Make sure we have enough room to mark follow-on header "Invalid"
        if (mCurrentBytesRemaining < (allocationSize + sizeof(CommandHeader)))
        {
            ASSERT((allocationSize + sizeof(CommandHeader)) < kBlockSize);
            allocateNewBlock();
        }
        return commonInit<StructType>(cmdID, allocationSize);
    }

    // Return a ptr to the parameter type
    template <class StructType>
    const StructType *getParamPtr(const CommandHeader *header) const
    {
        return reinterpret_cast<const StructType *>(reinterpret_cast<const uint8_t *>(header) +
                                                    sizeof(CommandHeader));
    }
    // Copy sizeInBytes data from paramData to writePointer & return writePointer plus sizeInBytes.
    template <class PtrType>
    ANGLE_INLINE uint8_t *storePointerParameter(uint8_t *writePointer,
                                                const PtrType *paramData,
                                                size_t sizeInBytes)
    {
        memcpy(writePointer, paramData, sizeInBytes);
        return writePointer + sizeInBytes;
    }

    std::vector<CommandHeader *> mCommands;

    // Allocator used by this class. If non-null then the class is valid.
    angle::PoolAllocator *mAllocator;

    uint8_t *mCurrentWritePointer;
    size_t mCurrentBytesRemaining;
    // resetQueryPool command must be executed outside RP so we queue them up for
    //  an inside RenderPass command buffer and pre-prend them to the commands
    std::vector<ResetQueryPoolParams> mResetQueryQueue;
};

ANGLE_INLINE SecondaryCommandBuffer::SecondaryCommandBuffer()
    : mAllocator(nullptr), mCurrentWritePointer(nullptr), mCurrentBytesRemaining(0)
{}
ANGLE_INLINE SecondaryCommandBuffer::~SecondaryCommandBuffer() {}

// begin and insert DebugUtilsLabelEXT funcs share this same function body
ANGLE_INLINE void SecondaryCommandBuffer::commonDebugUtilsLabel(CommandID cmd,
                                                                const VkDebugUtilsLabelEXT &label)
{
    uint8_t *writePtr;
    const size_t stringSize        = strlen(label.pLabelName) + 1;
    const size_t alignedStringSize = roundUp<size_t>(stringSize, 4);
    DebugUtilsLabelParams *paramStruct =
        initCommand<DebugUtilsLabelParams>(cmd, alignedStringSize, &writePtr);
    paramStruct->color[0] = label.color[0];
    paramStruct->color[1] = label.color[1];
    paramStruct->color[2] = label.color[2];
    paramStruct->color[3] = label.color[3];
    storePointerParameter(writePtr, label.pLabelName, alignedStringSize);
}

ANGLE_INLINE void SecondaryCommandBuffer::beginDebugUtilsLabelEXT(const VkDebugUtilsLabelEXT &label)
{
    commonDebugUtilsLabel(CommandID::BeginDebugUtilsLabel, label);
}

ANGLE_INLINE void SecondaryCommandBuffer::beginQuery(VkQueryPool queryPool,
                                                     uint32_t query,
                                                     VkQueryControlFlags flags)
{
    BeginQueryParams *paramStruct = initCommand<BeginQueryParams>(CommandID::BeginQuery);
    paramStruct->queryPool        = queryPool;
    paramStruct->query            = query;
    paramStruct->flags            = flags;
}

ANGLE_INLINE void SecondaryCommandBuffer::beginTransformFeedback(uint32_t bufferCount,
                                                                 const VkBuffer *counterBuffers)
{
    uint8_t *writePtr;
    size_t bufferSize                         = bufferCount * sizeof(VkBuffer);
    BeginTransformFeedbackParams *paramStruct = initCommand<BeginTransformFeedbackParams>(
        CommandID::BeginTransformFeedback, bufferSize, &writePtr);
    paramStruct->bufferCount = bufferCount;
    storePointerParameter(writePtr, counterBuffers, bufferSize);
}

ANGLE_INLINE void SecondaryCommandBuffer::bindComputePipeline(const Pipeline &pipeline)
{
    BindPipelineParams *paramStruct =
        initCommand<BindPipelineParams>(CommandID::BindComputePipeline);
    paramStruct->pipeline = pipeline.getHandle();
}

ANGLE_INLINE void SecondaryCommandBuffer::bindDescriptorSets(const PipelineLayout &layout,
                                                             VkPipelineBindPoint pipelineBindPoint,
                                                             uint32_t firstSet,
                                                             uint32_t descriptorSetCount,
                                                             const VkDescriptorSet *descriptorSets,
                                                             uint32_t dynamicOffsetCount,
                                                             const uint32_t *dynamicOffsets)
{
    size_t descSize   = descriptorSetCount * sizeof(VkDescriptorSet);
    size_t offsetSize = dynamicOffsetCount * sizeof(uint32_t);
    uint8_t *writePtr;
    BindDescriptorSetParams *paramStruct = initCommand<BindDescriptorSetParams>(
        CommandID::BindDescriptorSets, descSize + offsetSize, &writePtr);
    // Copy params into memory
    paramStruct->layout             = layout.getHandle();
    paramStruct->pipelineBindPoint  = pipelineBindPoint;
    paramStruct->firstSet           = firstSet;
    paramStruct->descriptorSetCount = descriptorSetCount;
    paramStruct->dynamicOffsetCount = dynamicOffsetCount;
    // Copy variable sized data
    writePtr = storePointerParameter(writePtr, descriptorSets, descSize);
    storePointerParameter(writePtr, dynamicOffsets, offsetSize);
}

ANGLE_INLINE void SecondaryCommandBuffer::bindGraphicsPipeline(const Pipeline &pipeline)
{
    BindPipelineParams *paramStruct =
        initCommand<BindPipelineParams>(CommandID::BindGraphicsPipeline);
    paramStruct->pipeline = pipeline.getHandle();
}

ANGLE_INLINE void SecondaryCommandBuffer::bindIndexBuffer(const Buffer &buffer,
                                                          VkDeviceSize offset,
                                                          VkIndexType indexType)
{
    BindIndexBufferParams *paramStruct =
        initCommand<BindIndexBufferParams>(CommandID::BindIndexBuffer);
    paramStruct->buffer    = buffer.getHandle();
    paramStruct->offset    = offset;
    paramStruct->indexType = indexType;
}

ANGLE_INLINE void SecondaryCommandBuffer::bindTransformFeedbackBuffers(uint32_t bindingCount,
                                                                       const VkBuffer *buffers,
                                                                       const VkDeviceSize *offsets,
                                                                       const VkDeviceSize *sizes)
{
    uint8_t *writePtr;
    size_t buffersSize = bindingCount * sizeof(VkBuffer);
    size_t offsetsSize = bindingCount * sizeof(VkDeviceSize);
    size_t sizesSize   = offsetsSize;
    BindTransformFeedbackBuffersParams *paramStruct =
        initCommand<BindTransformFeedbackBuffersParams>(CommandID::BindTransformFeedbackBuffers,
                                                        buffersSize + offsetsSize + sizesSize,
                                                        &writePtr);
    // Copy params
    paramStruct->bindingCount = bindingCount;
    writePtr                  = storePointerParameter(writePtr, buffers, buffersSize);
    writePtr                  = storePointerParameter(writePtr, offsets, offsetsSize);
    storePointerParameter(writePtr, sizes, sizesSize);
}

ANGLE_INLINE void SecondaryCommandBuffer::bindVertexBuffers(uint32_t firstBinding,
                                                            uint32_t bindingCount,
                                                            const VkBuffer *buffers,
                                                            const VkDeviceSize *offsets)
{
    ASSERT(firstBinding == 0);
    uint8_t *writePtr;
    size_t buffersSize                   = bindingCount * sizeof(VkBuffer);
    size_t offsetsSize                   = bindingCount * sizeof(VkDeviceSize);
    BindVertexBuffersParams *paramStruct = initCommand<BindVertexBuffersParams>(
        CommandID::BindVertexBuffers, buffersSize + offsetsSize, &writePtr);
    // Copy params
    paramStruct->bindingCount = bindingCount;
    writePtr                  = storePointerParameter(writePtr, buffers, buffersSize);
    storePointerParameter(writePtr, offsets, offsetsSize);
}

ANGLE_INLINE void SecondaryCommandBuffer::blitImage(const Image &srcImage,
                                                    VkImageLayout srcImageLayout,
                                                    const Image &dstImage,
                                                    VkImageLayout dstImageLayout,
                                                    uint32_t regionCount,
                                                    const VkImageBlit *regions,
                                                    VkFilter filter)
{
    // Currently ANGLE uses limited params so verify those assumptions and update if they change
    ASSERT(srcImageLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    ASSERT(dstImageLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    ASSERT(regionCount == 1);
    BlitImageParams *paramStruct = initCommand<BlitImageParams>(CommandID::BlitImage);
    paramStruct->srcImage        = srcImage.getHandle();
    paramStruct->dstImage        = dstImage.getHandle();
    paramStruct->filter          = filter;
    paramStruct->region          = regions[0];
}

ANGLE_INLINE void SecondaryCommandBuffer::bufferBarrier(
    VkPipelineStageFlags srcStageMask,
    VkPipelineStageFlags dstStageMask,
    const VkBufferMemoryBarrier *bufferMemoryBarrier)
{
    BufferBarrierParams *paramStruct = initCommand<BufferBarrierParams>(CommandID::BufferBarrier);
    paramStruct->srcStageMask        = srcStageMask;
    paramStruct->dstStageMask        = dstStageMask;
    paramStruct->bufferMemoryBarrier = *bufferMemoryBarrier;
}

ANGLE_INLINE void SecondaryCommandBuffer::clearAttachments(uint32_t attachmentCount,
                                                           const VkClearAttachment *attachments,
                                                           uint32_t rectCount,
                                                           const VkClearRect *rects)
{
    ASSERT(rectCount == 1);
    uint8_t *writePtr;
    size_t attachSize = attachmentCount * sizeof(VkClearAttachment);
    ClearAttachmentsParams *paramStruct =
        initCommand<ClearAttachmentsParams>(CommandID::ClearAttachments, attachSize, &writePtr);
    paramStruct->attachmentCount = attachmentCount;
    paramStruct->rect            = rects[0];
    // Copy variable sized data
    storePointerParameter(writePtr, attachments, attachSize);
}

ANGLE_INLINE void SecondaryCommandBuffer::clearColorImage(const Image &image,
                                                          VkImageLayout imageLayout,
                                                          const VkClearColorValue &color,
                                                          uint32_t rangeCount,
                                                          const VkImageSubresourceRange *ranges)
{
    ASSERT(rangeCount == 1);
    ClearColorImageParams *paramStruct =
        initCommand<ClearColorImageParams>(CommandID::ClearColorImage);
    paramStruct->image       = image.getHandle();
    paramStruct->imageLayout = imageLayout;
    paramStruct->color       = color;
    paramStruct->range       = ranges[0];
}

ANGLE_INLINE void SecondaryCommandBuffer::clearDepthStencilImage(
    const Image &image,
    VkImageLayout imageLayout,
    const VkClearDepthStencilValue &depthStencil,
    uint32_t rangeCount,
    const VkImageSubresourceRange *ranges)
{
    ASSERT(rangeCount == 1);
    ClearDepthStencilImageParams *paramStruct =
        initCommand<ClearDepthStencilImageParams>(CommandID::ClearDepthStencilImage);
    paramStruct->image        = image.getHandle();
    paramStruct->imageLayout  = imageLayout;
    paramStruct->depthStencil = depthStencil;
    paramStruct->range        = ranges[0];
}

ANGLE_INLINE void SecondaryCommandBuffer::copyBuffer(const Buffer &srcBuffer,
                                                     const Buffer &destBuffer,
                                                     uint32_t regionCount,
                                                     const VkBufferCopy *regions)
{
    uint8_t *writePtr;
    size_t regionSize = regionCount * sizeof(VkBufferCopy);
    CopyBufferParams *paramStruct =
        initCommand<CopyBufferParams>(CommandID::CopyBuffer, regionSize, &writePtr);
    paramStruct->srcBuffer   = srcBuffer.getHandle();
    paramStruct->destBuffer  = destBuffer.getHandle();
    paramStruct->regionCount = regionCount;
    // Copy variable sized data
    storePointerParameter(writePtr, regions, regionSize);
}

ANGLE_INLINE void SecondaryCommandBuffer::copyBufferToImage(VkBuffer srcBuffer,
                                                            const Image &dstImage,
                                                            VkImageLayout dstImageLayout,
                                                            uint32_t regionCount,
                                                            const VkBufferImageCopy *regions)
{
    ASSERT(regionCount == 1);
    CopyBufferToImageParams *paramStruct =
        initCommand<CopyBufferToImageParams>(CommandID::CopyBufferToImage);
    paramStruct->srcBuffer      = srcBuffer;
    paramStruct->dstImage       = dstImage.getHandle();
    paramStruct->dstImageLayout = dstImageLayout;
    paramStruct->region         = regions[0];
}

ANGLE_INLINE void SecondaryCommandBuffer::copyImage(const Image &srcImage,
                                                    VkImageLayout srcImageLayout,
                                                    const Image &dstImage,
                                                    VkImageLayout dstImageLayout,
                                                    uint32_t regionCount,
                                                    const VkImageCopy *regions)
{
    ASSERT(regionCount == 1);
    CopyImageParams *paramStruct = initCommand<CopyImageParams>(CommandID::CopyImage);
    paramStruct->srcImage        = srcImage.getHandle();
    paramStruct->srcImageLayout  = srcImageLayout;
    paramStruct->dstImage        = dstImage.getHandle();
    paramStruct->dstImageLayout  = dstImageLayout;
    paramStruct->region          = regions[0];
}

ANGLE_INLINE void SecondaryCommandBuffer::copyImageToBuffer(const Image &srcImage,
                                                            VkImageLayout srcImageLayout,
                                                            VkBuffer dstBuffer,
                                                            uint32_t regionCount,
                                                            const VkBufferImageCopy *regions)
{
    ASSERT(regionCount == 1);
    CopyImageToBufferParams *paramStruct =
        initCommand<CopyImageToBufferParams>(CommandID::CopyImageToBuffer);
    paramStruct->srcImage       = srcImage.getHandle();
    paramStruct->srcImageLayout = srcImageLayout;
    paramStruct->dstBuffer      = dstBuffer;
    paramStruct->region         = regions[0];
}

ANGLE_INLINE void SecondaryCommandBuffer::dispatch(uint32_t groupCountX,
                                                   uint32_t groupCountY,
                                                   uint32_t groupCountZ)
{
    DispatchParams *paramStruct = initCommand<DispatchParams>(CommandID::Dispatch);
    paramStruct->groupCountX    = groupCountX;
    paramStruct->groupCountY    = groupCountY;
    paramStruct->groupCountZ    = groupCountZ;
}

ANGLE_INLINE void SecondaryCommandBuffer::dispatchIndirect(const Buffer &buffer,
                                                           VkDeviceSize offset)
{
    DispatchIndirectParams *paramStruct =
        initCommand<DispatchIndirectParams>(CommandID::DispatchIndirect);
    paramStruct->buffer = buffer.getHandle();
    paramStruct->offset = offset;
}

ANGLE_INLINE void SecondaryCommandBuffer::draw(uint32_t vertexCount, uint32_t firstVertex)
{
    DrawParams *paramStruct  = initCommand<DrawParams>(CommandID::Draw);
    paramStruct->vertexCount = vertexCount;
    paramStruct->firstVertex = firstVertex;
}

ANGLE_INLINE void SecondaryCommandBuffer::drawIndexed(uint32_t indexCount)
{
    DrawIndexedParams *paramStruct = initCommand<DrawIndexedParams>(CommandID::DrawIndexed);
    paramStruct->indexCount        = indexCount;
}

ANGLE_INLINE void SecondaryCommandBuffer::drawIndexedBaseVertex(uint32_t indexCount,
                                                                uint32_t vertexOffset)
{
    DrawIndexedBaseVertexParams *paramStruct =
        initCommand<DrawIndexedBaseVertexParams>(CommandID::DrawIndexedBaseVertex);
    paramStruct->indexCount   = indexCount;
    paramStruct->vertexOffset = vertexOffset;
}

ANGLE_INLINE void SecondaryCommandBuffer::drawIndexedIndirect(const Buffer &buffer,
                                                              VkDeviceSize offset,
                                                              uint32_t drawCount,
                                                              uint32_t stride)
{
    DrawIndexedIndirectParams *paramStruct =
        initCommand<DrawIndexedIndirectParams>(CommandID::DrawIndexedIndirect);
    paramStruct->buffer = buffer.getHandle();
    paramStruct->offset = offset;
    ASSERT(drawCount == 1);
}

ANGLE_INLINE void SecondaryCommandBuffer::drawIndexedInstanced(uint32_t indexCount,
                                                               uint32_t instanceCount)
{
    DrawIndexedInstancedParams *paramStruct =
        initCommand<DrawIndexedInstancedParams>(CommandID::DrawIndexedInstanced);
    paramStruct->indexCount    = indexCount;
    paramStruct->instanceCount = instanceCount;
}

ANGLE_INLINE void SecondaryCommandBuffer::drawIndexedInstancedBaseVertex(uint32_t indexCount,
                                                                         uint32_t instanceCount,
                                                                         uint32_t vertexOffset)
{
    DrawIndexedInstancedBaseVertexParams *paramStruct =
        initCommand<DrawIndexedInstancedBaseVertexParams>(
            CommandID::DrawIndexedInstancedBaseVertex);
    paramStruct->indexCount    = indexCount;
    paramStruct->instanceCount = instanceCount;
    paramStruct->vertexOffset  = vertexOffset;
}

ANGLE_INLINE void SecondaryCommandBuffer::drawIndexedInstancedBaseVertexBaseInstance(
    uint32_t indexCount,
    uint32_t instanceCount,
    uint32_t firstIndex,
    int32_t vertexOffset,
    uint32_t firstInstance)
{
    DrawIndexedInstancedBaseVertexBaseInstanceParams *paramStruct =
        initCommand<DrawIndexedInstancedBaseVertexBaseInstanceParams>(
            CommandID::DrawIndexedInstancedBaseVertexBaseInstance);
    paramStruct->indexCount    = indexCount;
    paramStruct->instanceCount = instanceCount;
    paramStruct->firstIndex    = firstIndex;
    paramStruct->vertexOffset  = vertexOffset;
    paramStruct->firstInstance = firstInstance;
}

ANGLE_INLINE void SecondaryCommandBuffer::drawIndirect(const Buffer &buffer,
                                                       VkDeviceSize offset,
                                                       uint32_t drawCount,
                                                       uint32_t stride)
{
    DrawIndirectParams *paramStruct = initCommand<DrawIndirectParams>(CommandID::DrawIndirect);
    paramStruct->buffer             = buffer.getHandle();
    paramStruct->offset             = offset;

    // OpenGL ES doesn't have a way to specify a drawCount or stride, throw assert if something
    // changes.
    ASSERT(drawCount == 1);
}

ANGLE_INLINE void SecondaryCommandBuffer::drawInstanced(uint32_t vertexCount,
                                                        uint32_t instanceCount,
                                                        uint32_t firstVertex)
{
    DrawInstancedParams *paramStruct = initCommand<DrawInstancedParams>(CommandID::DrawInstanced);
    paramStruct->vertexCount         = vertexCount;
    paramStruct->instanceCount       = instanceCount;
    paramStruct->firstVertex         = firstVertex;
}

ANGLE_INLINE void SecondaryCommandBuffer::drawInstancedBaseInstance(uint32_t vertexCount,
                                                                    uint32_t instanceCount,
                                                                    uint32_t firstVertex,
                                                                    uint32_t firstInstance)
{
    DrawInstancedBaseInstanceParams *paramStruct =
        initCommand<DrawInstancedBaseInstanceParams>(CommandID::DrawInstancedBaseInstance);
    paramStruct->vertexCount   = vertexCount;
    paramStruct->instanceCount = instanceCount;
    paramStruct->firstVertex   = firstVertex;
    paramStruct->firstInstance = firstInstance;
}

ANGLE_INLINE void SecondaryCommandBuffer::endDebugUtilsLabelEXT()
{
    initCommand<EmptyParams>(CommandID::EndDebugUtilsLabel);
}

ANGLE_INLINE void SecondaryCommandBuffer::endQuery(VkQueryPool queryPool, uint32_t query)
{
    EndQueryParams *paramStruct = initCommand<EndQueryParams>(CommandID::EndQuery);
    paramStruct->queryPool      = queryPool;
    paramStruct->query          = query;
}

ANGLE_INLINE void SecondaryCommandBuffer::endTransformFeedback(uint32_t bufferCount,
                                                               const VkBuffer *counterBuffers)
{
    uint8_t *writePtr;
    size_t bufferSize                       = bufferCount * sizeof(VkBuffer);
    EndTransformFeedbackParams *paramStruct = initCommand<EndTransformFeedbackParams>(
        CommandID::EndTransformFeedback, bufferSize, &writePtr);
    paramStruct->bufferCount = bufferCount;
    storePointerParameter(writePtr, counterBuffers, bufferSize);
}

ANGLE_INLINE void SecondaryCommandBuffer::executionBarrier(VkPipelineStageFlags stageMask)
{
    ExecutionBarrierParams *paramStruct =
        initCommand<ExecutionBarrierParams>(CommandID::ExecutionBarrier);
    paramStruct->stageMask = stageMask;
}

ANGLE_INLINE void SecondaryCommandBuffer::fillBuffer(const Buffer &dstBuffer,
                                                     VkDeviceSize dstOffset,
                                                     VkDeviceSize size,
                                                     uint32_t data)
{
    FillBufferParams *paramStruct = initCommand<FillBufferParams>(CommandID::FillBuffer);
    paramStruct->dstBuffer        = dstBuffer.getHandle();
    paramStruct->dstOffset        = dstOffset;
    paramStruct->size             = size;
    paramStruct->data             = data;
}

ANGLE_INLINE void SecondaryCommandBuffer::imageBarrier(
    VkPipelineStageFlags srcStageMask,
    VkPipelineStageFlags dstStageMask,
    const VkImageMemoryBarrier &imageMemoryBarrier)
{
    ImageBarrierParams *paramStruct = initCommand<ImageBarrierParams>(CommandID::ImageBarrier);
    ASSERT(imageMemoryBarrier.pNext == nullptr);
    paramStruct->srcStageMask       = srcStageMask;
    paramStruct->dstStageMask       = dstStageMask;
    paramStruct->imageMemoryBarrier = imageMemoryBarrier;
}

ANGLE_INLINE void SecondaryCommandBuffer::insertDebugUtilsLabelEXT(
    const VkDebugUtilsLabelEXT &label)
{
    commonDebugUtilsLabel(CommandID::InsertDebugUtilsLabel, label);
}

ANGLE_INLINE void SecondaryCommandBuffer::memoryBarrier(VkPipelineStageFlags srcStageMask,
                                                        VkPipelineStageFlags dstStageMask,
                                                        const VkMemoryBarrier *memoryBarrier)
{
    MemoryBarrierParams *paramStruct = initCommand<MemoryBarrierParams>(CommandID::MemoryBarrier);
    paramStruct->srcStageMask        = srcStageMask;
    paramStruct->dstStageMask        = dstStageMask;
    paramStruct->memoryBarrier       = *memoryBarrier;
}

ANGLE_INLINE void SecondaryCommandBuffer::pipelineBarrier(
    VkPipelineStageFlags srcStageMask,
    VkPipelineStageFlags dstStageMask,
    VkDependencyFlags dependencyFlags,
    uint32_t memoryBarrierCount,
    const VkMemoryBarrier *memoryBarriers,
    uint32_t bufferMemoryBarrierCount,
    const VkBufferMemoryBarrier *bufferMemoryBarriers,
    uint32_t imageMemoryBarrierCount,
    const VkImageMemoryBarrier *imageMemoryBarriers)
{
    uint8_t *writePtr;
    size_t memBarrierSize              = memoryBarrierCount * sizeof(VkMemoryBarrier);
    size_t buffBarrierSize             = bufferMemoryBarrierCount * sizeof(VkBufferMemoryBarrier);
    size_t imgBarrierSize              = imageMemoryBarrierCount * sizeof(VkImageMemoryBarrier);
    PipelineBarrierParams *paramStruct = initCommand<PipelineBarrierParams>(
        CommandID::PipelineBarrier, memBarrierSize + buffBarrierSize + imgBarrierSize, &writePtr);
    paramStruct->srcStageMask             = srcStageMask;
    paramStruct->dstStageMask             = dstStageMask;
    paramStruct->dependencyFlags          = dependencyFlags;
    paramStruct->memoryBarrierCount       = memoryBarrierCount;
    paramStruct->bufferMemoryBarrierCount = bufferMemoryBarrierCount;
    paramStruct->imageMemoryBarrierCount  = imageMemoryBarrierCount;
    // Copy variable sized data
    writePtr = storePointerParameter(writePtr, memoryBarriers, memBarrierSize);
    writePtr = storePointerParameter(writePtr, bufferMemoryBarriers, buffBarrierSize);
    storePointerParameter(writePtr, imageMemoryBarriers, imgBarrierSize);
}

ANGLE_INLINE void SecondaryCommandBuffer::pushConstants(const PipelineLayout &layout,
                                                        VkShaderStageFlags flag,
                                                        uint32_t offset,
                                                        uint32_t size,
                                                        const void *data)
{
    ASSERT(size == static_cast<size_t>(size));
    uint8_t *writePtr;
    PushConstantsParams *paramStruct = initCommand<PushConstantsParams>(
        CommandID::PushConstants, static_cast<size_t>(size), &writePtr);
    paramStruct->layout = layout.getHandle();
    paramStruct->flag   = flag;
    paramStruct->offset = offset;
    paramStruct->size   = size;
    // Copy variable sized data
    storePointerParameter(writePtr, data, static_cast<size_t>(size));
}

ANGLE_INLINE void SecondaryCommandBuffer::resetEvent(VkEvent event, VkPipelineStageFlags stageMask)
{
    ResetEventParams *paramStruct = initCommand<ResetEventParams>(CommandID::ResetEvent);
    paramStruct->event            = event;
    paramStruct->stageMask        = stageMask;
}

ANGLE_INLINE void SecondaryCommandBuffer::queueResetQueryPool(VkQueryPool queryPool,
                                                              uint32_t firstQuery,
                                                              uint32_t queryCount)
{
    mResetQueryQueue.push_back({queryPool, firstQuery, queryCount});
}

ANGLE_INLINE void SecondaryCommandBuffer::resetQueryPool(VkQueryPool queryPool,
                                                         uint32_t firstQuery,
                                                         uint32_t queryCount)
{
    ResetQueryPoolParams *paramStruct =
        initCommand<ResetQueryPoolParams>(CommandID::ResetQueryPool);
    paramStruct->queryPool  = queryPool;
    paramStruct->firstQuery = firstQuery;
    paramStruct->queryCount = queryCount;
}

ANGLE_INLINE void SecondaryCommandBuffer::resolveImage(const Image &srcImage,
                                                       VkImageLayout srcImageLayout,
                                                       const Image &dstImage,
                                                       VkImageLayout dstImageLayout,
                                                       uint32_t regionCount,
                                                       const VkImageResolve *regions)
{
    // Currently ANGLE uses limited params so verify those assumptions and update if they change.
    ASSERT(srcImageLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    ASSERT(dstImageLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    ASSERT(regionCount == 1);
    ResolveImageParams *paramStruct = initCommand<ResolveImageParams>(CommandID::ResolveImage);
    paramStruct->srcImage           = srcImage.getHandle();
    paramStruct->dstImage           = dstImage.getHandle();
    paramStruct->region             = regions[0];
}

ANGLE_INLINE void SecondaryCommandBuffer::setEvent(VkEvent event, VkPipelineStageFlags stageMask)
{
    SetEventParams *paramStruct = initCommand<SetEventParams>(CommandID::SetEvent);
    paramStruct->event          = event;
    paramStruct->stageMask      = stageMask;
}

ANGLE_INLINE void SecondaryCommandBuffer::waitEvents(
    uint32_t eventCount,
    const VkEvent *events,
    VkPipelineStageFlags srcStageMask,
    VkPipelineStageFlags dstStageMask,
    uint32_t memoryBarrierCount,
    const VkMemoryBarrier *memoryBarriers,
    uint32_t bufferMemoryBarrierCount,
    const VkBufferMemoryBarrier *bufferMemoryBarriers,
    uint32_t imageMemoryBarrierCount,
    const VkImageMemoryBarrier *imageMemoryBarriers)
{
    uint8_t *writePtr;
    size_t eventSize              = eventCount * sizeof(VkEvent);
    size_t memBarrierSize         = memoryBarrierCount * sizeof(VkMemoryBarrier);
    size_t buffBarrierSize        = bufferMemoryBarrierCount * sizeof(VkBufferMemoryBarrier);
    size_t imgBarrierSize         = imageMemoryBarrierCount * sizeof(VkImageMemoryBarrier);
    WaitEventsParams *paramStruct = initCommand<WaitEventsParams>(
        CommandID::WaitEvents, eventSize + memBarrierSize + buffBarrierSize + imgBarrierSize,
        &writePtr);
    paramStruct->eventCount               = eventCount;
    paramStruct->srcStageMask             = srcStageMask;
    paramStruct->dstStageMask             = dstStageMask;
    paramStruct->memoryBarrierCount       = memoryBarrierCount;
    paramStruct->bufferMemoryBarrierCount = bufferMemoryBarrierCount;
    paramStruct->imageMemoryBarrierCount  = imageMemoryBarrierCount;
    // Copy variable sized data
    writePtr = storePointerParameter(writePtr, events, eventSize);
    writePtr = storePointerParameter(writePtr, memoryBarriers, memBarrierSize);
    writePtr = storePointerParameter(writePtr, bufferMemoryBarriers, buffBarrierSize);
    storePointerParameter(writePtr, imageMemoryBarriers, imgBarrierSize);
}

ANGLE_INLINE void SecondaryCommandBuffer::writeTimestamp(VkPipelineStageFlagBits pipelineStage,
                                                         VkQueryPool queryPool,
                                                         uint32_t query)
{
    WriteTimestampParams *paramStruct =
        initCommand<WriteTimestampParams>(CommandID::WriteTimestamp);
    paramStruct->pipelineStage = pipelineStage;
    paramStruct->queryPool     = queryPool;
    paramStruct->query         = query;
}
}  // namespace priv
}  // namespace vk
}  // namespace rx

#endif  // LIBANGLE_RENDERER_VULKAN_SECONDARYCOMMANDBUFFERVK_H_
