//
// Copyright 2024 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#ifdef UNSAFE_BUFFERS_BUILD
#    pragma allow_unsafe_buffers
#endif

#include "libANGLE/renderer/wgpu/wgpu_command_buffer.h"

namespace rx
{
namespace webgpu
{
namespace
{
template <typename T>
T::ObjectType GetReferencedObject(std::unordered_set<T> &referenceList, const T &item)
{
    auto iter = referenceList.insert(item).first;
    return (*iter).get();
}

// Get the packed command ID from the current command data
CommandID CurrentCommandID(const uint8_t *commandData)
{
    return *reinterpret_cast<const CommandID *>(commandData);
}

// Get the command struct from the current command data and increment the command data to the next
// command
template <CommandID Command, typename CommandType = CommandTypeHelper<Command>::CommandType>
const CommandType &GetCommandAndIterate(const uint8_t **commandData)
{
    constexpr size_t commandAndIdSize = sizeof(CommandID) + sizeof(CommandType);
    const CommandType *command =
        reinterpret_cast<const CommandType *>(*commandData + sizeof(CommandID));
    *commandData += commandAndIdSize;
    return *command;
}
}  // namespace

CommandBuffer::CommandBuffer() {}

void CommandBuffer::draw(uint32_t vertexCount,
                         uint32_t instanceCount,
                         uint32_t firstVertex,
                         uint32_t firstInstance)
{
    DrawCommand *drawCommand   = initCommand<CommandID::Draw>();
    drawCommand->vertexCount   = vertexCount;
    drawCommand->instanceCount = instanceCount;
    drawCommand->firstVertex   = firstVertex;
    drawCommand->firstInstance = firstInstance;
}

void CommandBuffer::drawIndexed(uint32_t indexCount,
                                uint32_t instanceCount,
                                uint32_t firstIndex,
                                int32_t baseVertex,
                                uint32_t firstInstance)
{
    DrawIndexedCommand *drawIndexedCommand = initCommand<CommandID::DrawIndexed>();
    drawIndexedCommand->indexCount         = indexCount;
    drawIndexedCommand->instanceCount      = instanceCount;
    drawIndexedCommand->firstIndex         = firstIndex;
    drawIndexedCommand->baseVertex         = baseVertex;
    drawIndexedCommand->firstInstance      = firstInstance;
}

void CommandBuffer::setBindGroup(uint32_t groupIndex, BindGroupHandle bindGroup)
{
    SetBindGroupCommand *setBindGroupCommand = initCommand<CommandID::SetBindGroup>();
    setBindGroupCommand->groupIndex          = groupIndex;
    setBindGroupCommand->bindGroup = GetReferencedObject(mState.referencedBindGroups, bindGroup);
}

void CommandBuffer::setBlendConstant(float r, float g, float b, float a)
{
    SetBlendConstantCommand *setBlendConstantCommand = initCommand<CommandID::SetBlendConstant>();
    setBlendConstantCommand->r                       = r;
    setBlendConstantCommand->g                       = g;
    setBlendConstantCommand->b                       = b;
    setBlendConstantCommand->a                       = a;

    mState.hasSetBlendConstantCommand = true;
}

void CommandBuffer::setPipeline(RenderPipelineHandle pipeline)
{
    SetPipelineCommand *setPiplelineCommand = initCommand<CommandID::SetPipeline>();
    setPiplelineCommand->pipeline = GetReferencedObject(mState.referencedRenderPipelines, pipeline);
}

void CommandBuffer::setScissorRect(uint32_t x, uint32_t y, uint32_t width, uint32_t height)
{
    SetScissorRectCommand *setScissorRectCommand = initCommand<CommandID::SetScissorRect>();
    setScissorRectCommand->x                     = x;
    setScissorRectCommand->y                     = y;
    setScissorRectCommand->width                 = width;
    setScissorRectCommand->height                = height;

    mState.hasSetScissorCommand = true;
}

void CommandBuffer::setViewport(float x,
                                float y,
                                float width,
                                float height,
                                float minDepth,
                                float maxDepth)
{
    SetViewportCommand *setViewportCommand = initCommand<CommandID::SetViewport>();
    setViewportCommand->x                  = x;
    setViewportCommand->y                  = y;
    setViewportCommand->width              = width;
    setViewportCommand->height             = height;
    setViewportCommand->minDepth           = minDepth;
    setViewportCommand->maxDepth           = maxDepth;

    mState.hasSetViewportCommand = true;
}

void CommandBuffer::setIndexBuffer(BufferHandle buffer,
                                   WGPUIndexFormat format,
                                   uint64_t offset,
                                   uint64_t size)
{
    SetIndexBufferCommand *setIndexBufferCommand = initCommand<CommandID::SetIndexBuffer>();
    setIndexBufferCommand->buffer = GetReferencedObject(mState.referencedBuffers, buffer);
    setIndexBufferCommand->format                = format;
    setIndexBufferCommand->offset                = offset;
    setIndexBufferCommand->size                  = size;
}

void CommandBuffer::setVertexBuffer(uint32_t slot,
                                    BufferHandle buffer,
                                    uint64_t offset,
                                    uint64_t size)
{
    SetVertexBufferCommand *setVertexBufferCommand = initCommand<CommandID::SetVertexBuffer>();
    setVertexBufferCommand->slot                   = slot;
    setVertexBufferCommand->buffer = GetReferencedObject(mState.referencedBuffers, buffer);
    setVertexBufferCommand->offset                 = offset;
    setVertexBufferCommand->size                   = size;
}

void CommandBuffer::clear()
{
    if (!mCommandBlocks.empty())
    {
        // Only clear the command blocks that have been used
        for (size_t cmdBlockIdx = 0; cmdBlockIdx <= mState.currentCommandBlock; cmdBlockIdx++)
        {
            mCommandBlocks[cmdBlockIdx]->clear();
        }
    }
    mState = PerSubmissionData();
}

void CommandBuffer::recordCommands(const DawnProcTable *wgpu, RenderPassEncoderHandle encoder)
{
    ASSERT(hasCommands());
    ASSERT(!mCommandBlocks.empty());

    // Make sure the last block is finalized
    mCommandBlocks[mState.currentCommandBlock]->finalize();

    for (size_t cmdBlockIdx = 0; cmdBlockIdx <= mState.currentCommandBlock; cmdBlockIdx++)
    {
        const CommandBlock *commandBlock = mCommandBlocks[cmdBlockIdx].get();

        const uint8_t *currentCommand = commandBlock->mData;
        while (CurrentCommandID(currentCommand) != CommandID::Invalid)
        {
            switch (CurrentCommandID(currentCommand))
            {
                case CommandID::Invalid:
                    UNREACHABLE();
                    return;

                case CommandID::Draw:
                {
                    const DrawCommand &drawCommand =
                        GetCommandAndIterate<CommandID::Draw>(&currentCommand);
                    wgpu->renderPassEncoderDraw(encoder.get(), drawCommand.vertexCount,
                                                drawCommand.instanceCount, drawCommand.firstVertex,
                                                drawCommand.firstInstance);
                    break;
                }

                case CommandID::DrawIndexed:
                {
                    const DrawIndexedCommand &drawIndexedCommand =
                        GetCommandAndIterate<CommandID::DrawIndexed>(&currentCommand);
                    wgpu->renderPassEncoderDrawIndexed(
                        encoder.get(), drawIndexedCommand.indexCount,
                        drawIndexedCommand.instanceCount, drawIndexedCommand.firstIndex,
                        drawIndexedCommand.baseVertex, drawIndexedCommand.firstInstance);
                    break;
                }

                case CommandID::SetBindGroup:
                {
                    const SetBindGroupCommand &setBindGroupCommand =
                        GetCommandAndIterate<CommandID::SetBindGroup>(&currentCommand);
                    wgpu->renderPassEncoderSetBindGroup(encoder.get(),
                                                        setBindGroupCommand.groupIndex,
                                                        setBindGroupCommand.bindGroup, 0, nullptr);
                    break;
                }

                case CommandID::SetBlendConstant:
                {
                    const SetBlendConstantCommand &setBlendConstantCommand =
                        GetCommandAndIterate<CommandID::SetBlendConstant>(&currentCommand);
                    WGPUColor color{setBlendConstantCommand.r, setBlendConstantCommand.g,
                                    setBlendConstantCommand.b, setBlendConstantCommand.a};
                    wgpu->renderPassEncoderSetBlendConstant(encoder.get(), &color);
                    break;
                }

                case CommandID::SetIndexBuffer:
                {
                    const SetIndexBufferCommand &setIndexBufferCommand =
                        GetCommandAndIterate<CommandID::SetIndexBuffer>(&currentCommand);
                    wgpu->renderPassEncoderSetIndexBuffer(
                        encoder.get(), setIndexBufferCommand.buffer, setIndexBufferCommand.format,
                        setIndexBufferCommand.offset, setIndexBufferCommand.size);
                    break;
                }

                case CommandID::SetPipeline:
                {
                    const SetPipelineCommand &setPiplelineCommand =
                        GetCommandAndIterate<CommandID::SetPipeline>(&currentCommand);
                    wgpu->renderPassEncoderSetPipeline(encoder.get(), setPiplelineCommand.pipeline);
                    break;
                }

                case CommandID::SetScissorRect:
                {
                    const SetScissorRectCommand &setScissorRectCommand =
                        GetCommandAndIterate<CommandID::SetScissorRect>(&currentCommand);
                    wgpu->renderPassEncoderSetScissorRect(
                        encoder.get(), setScissorRectCommand.x, setScissorRectCommand.y,
                        setScissorRectCommand.width, setScissorRectCommand.height);
                    break;
                }

                case CommandID::SetViewport:
                {
                    const SetViewportCommand &setViewportCommand =
                        GetCommandAndIterate<CommandID::SetViewport>(&currentCommand);
                    wgpu->renderPassEncoderSetViewport(
                        encoder.get(), setViewportCommand.x, setViewportCommand.y,
                        setViewportCommand.width, setViewportCommand.height,
                        setViewportCommand.minDepth, setViewportCommand.maxDepth);
                    break;
                }

                case CommandID::SetVertexBuffer:
                {
                    const SetVertexBufferCommand &setVertexBufferCommand =
                        GetCommandAndIterate<CommandID::SetVertexBuffer>(&currentCommand);
                    wgpu->renderPassEncoderSetVertexBuffer(
                        encoder.get(), setVertexBufferCommand.slot, setVertexBufferCommand.buffer,
                        setVertexBufferCommand.offset, setVertexBufferCommand.size);
                    break;
                }

                default:
                    UNREACHABLE();
                    return;
            }
        }
    }
}

void CommandBuffer::nextCommandBlock()
{
    if (!mCommandBlocks.empty())
    {
        ASSERT(mState.currentCommandBlock < mCommandBlocks.size());

        // Finish the current command block before moving to a new one
        mCommandBlocks[mState.currentCommandBlock]->finalize();
    }

    if (mState.currentCommandBlock + 1 < mCommandBlocks.size())
    {
        // There is already a command block allocated. Make sure it's been cleared and use it.
        mState.currentCommandBlock++;
        ASSERT(mCommandBlocks[mState.currentCommandBlock]->mCurrentPosition == 0);
        ASSERT(mCommandBlocks[mState.currentCommandBlock]->mRemainingSize > 0);
    }
    else
    {
        std::unique_ptr<CommandBlock> newBlock = std::make_unique<CommandBlock>();
        mCommandBlocks.push_back(std::move(newBlock));
        mState.currentCommandBlock = mCommandBlocks.size() - 1;
    }
}

void CommandBuffer::CommandBlock::clear()
{
    mCurrentPosition = 0;
    mRemainingSize   = kCommandBlockInitialRemainingSize;
}

void CommandBuffer::CommandBlock::finalize()
{
    // Don't move the current position to allow finalize to be called multiple times if needed
    CommandID *nextCommandID = getDataAtCurrentPositionAndReserveSpace<CommandID>(0);
    *nextCommandID           = CommandID::Invalid;
    mRemainingSize           = 0;
}

}  // namespace webgpu
}  // namespace rx
