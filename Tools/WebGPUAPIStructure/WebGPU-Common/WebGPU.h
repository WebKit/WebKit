/*
* Copyright (C) 2017 Apple Inc. All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions
* are met:
* 1. Redistributions of source code must retain the above copyright
*    notice, this list of conditions and the following disclaimer.
* 2. Redistributions in binary form must reproduce the above copyright
*    notice, this list of conditions and the following disclaimer in the
*    documentation and/or other materials provided with the distribution.
*
* THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
* EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
* PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
* CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
* EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
* PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
* PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
* OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
* OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#pragma once

#include <memory>
#include <vector>
#include <string>
#include <boost/variant.hpp>
#include <boost/thread.hpp>
#include <windows.h>

namespace WebGPU {
    class Texture;
    class Resource;
    class Device;
    class RenderPass;
    class ComputePass;
    class BlitPass;
    class HostAccessPass;

    class Queue {
    public:
        virtual ~Queue() {}

        virtual std::unique_ptr<RenderPass> createRenderPass(const std::vector<std::reference_wrapper<Texture>>* textures) = 0;
        virtual void commitRenderPass(std::unique_ptr<RenderPass>&&) = 0;
        virtual std::unique_ptr<ComputePass> createComputePass() = 0;
        virtual void commitComputePass(std::unique_ptr<ComputePass>&&) = 0;
        virtual std::unique_ptr<BlitPass> createBlitPass() = 0;
        virtual void commitBlitPass(std::unique_ptr<BlitPass>&&) = 0;
        virtual std::unique_ptr<HostAccessPass> createHostAccessPass() = 0;
        virtual void commitHostAccessPass(std::unique_ptr<HostAccessPass>&&) = 0;
        virtual void present() = 0;

    protected:
        Queue() = default;
        Queue(const Queue&) = delete;
        Queue(Queue&&) = delete;
        virtual Queue& operator=(const Queue&) = default;
        virtual Queue& operator=(Queue&&) = default;
    };

    class RenderState {
    public:
        virtual ~RenderState() {}

        enum class VertexFormat {
            Float,
            Float2,
            Float3,
            Float4
        };

        struct VertexAttribute {
            VertexFormat format;
            unsigned int offsetWithinStride;
            unsigned int vertexAttributeBufferIndex;
        };

    protected:
        RenderState() = default;
        RenderState(const RenderState&) = delete;
        RenderState(RenderState&&) = default;
        virtual RenderState& operator=(const RenderState&) = delete;
        virtual RenderState& operator=(RenderState&&) = default;
    };

    class ComputeState {
    public:
        virtual ~ComputeState() {}

    protected:
        ComputeState() = default;
        ComputeState(const ComputeState&) = delete;
        ComputeState(ComputeState&&) = default;
        virtual ComputeState& operator=(const ComputeState&) = delete;
        virtual ComputeState& operator=(ComputeState&&) = default;
    };

    enum class ResourceType {
        Texture,
        Sampler,
        UniformBufferObject,
        ShaderStorageBufferObject,
    };

    class Buffer {
    public:
        virtual ~Buffer() {}

    protected:
        Buffer() = default;
        Buffer(const Buffer&) = delete;
        Buffer(Buffer&&) = default;
        virtual Buffer& operator=(const Buffer&) = delete;
        virtual Buffer& operator=(Buffer&&) = default;
    };

    enum class PixelFormat {
        RGBA8sRGB,
        RGBA8
    };

    class Texture {
    public:
        virtual ~Texture() {}

    protected:
        Texture() = default;
        Texture(const Texture&) = delete;
        Texture(Texture&&) = default;
        virtual Texture& operator=(const Texture&) = delete;
        virtual Texture& operator=(Texture&&) = default;
    };

    class Sampler {
    public:
        virtual ~Sampler() {}

    protected:
        Sampler() = default;
        Sampler(const Sampler&) = delete;
        Sampler(Sampler&&) = default;
        virtual Sampler& operator=(const Sampler&) = delete;
        virtual Sampler& operator=(Sampler&&) = default;
    };

    class TextureReference {
    public:
        TextureReference() = default;
        TextureReference(Texture& texture) : texture(texture) {}
        TextureReference(const TextureReference&) = default;
        TextureReference(TextureReference&&) = default;
        TextureReference& operator=(const TextureReference&) = default;
        TextureReference& operator=(TextureReference&&) = default;

        Texture& get() const { return texture; }

    private:
        Texture& texture;
    };

    class SamplerReference {
    public:
        SamplerReference() = default;
        SamplerReference(Sampler& sampler) : sampler(sampler) {}
        SamplerReference(const SamplerReference&) = default;
        SamplerReference(SamplerReference&&) = default;
        SamplerReference& operator=(const SamplerReference&) = default;
        SamplerReference& operator=(SamplerReference&&) = default;

        Sampler& get() const { return sampler; }

    private:
        Sampler& sampler;
    };

    class UniformBufferObjectReference {
    public:
        UniformBufferObjectReference() = default;
        UniformBufferObjectReference(Buffer& buffer) : buffer(buffer) {}
        UniformBufferObjectReference(const UniformBufferObjectReference&) = default;
        UniformBufferObjectReference(UniformBufferObjectReference&&) = default;
        UniformBufferObjectReference& operator=(const UniformBufferObjectReference&) = default;
        UniformBufferObjectReference& operator=(UniformBufferObjectReference&&) = default;

        Buffer& get() const { return buffer; }

    private:
        Buffer& buffer;
    };

    class ShaderStorageBufferObjectReference {
    public:
        ShaderStorageBufferObjectReference() = default;
        ShaderStorageBufferObjectReference(Buffer& buffer) : buffer(buffer) {}
        ShaderStorageBufferObjectReference(const ShaderStorageBufferObjectReference&) = default;
        ShaderStorageBufferObjectReference(ShaderStorageBufferObjectReference&&) = default;
        ShaderStorageBufferObjectReference& operator=(const ShaderStorageBufferObjectReference&) = default;
        ShaderStorageBufferObjectReference& operator=(ShaderStorageBufferObjectReference&&) = default;

        Buffer& get() const { return buffer; }

    private:
        Buffer& buffer;
    };

    typedef boost::variant<TextureReference, SamplerReference, UniformBufferObjectReference, ShaderStorageBufferObjectReference> ResourceReference;

    class RenderPass {
    public:
        virtual ~RenderPass() {}

        virtual void setRenderState(const RenderState&) = 0;
        virtual void setVertexAttributeBuffers(const std::vector<std::reference_wrapper<Buffer>>& buffers) = 0;
        virtual void setResources(unsigned int index, const std::vector<ResourceReference>&) = 0;
        virtual void setViewport(unsigned int x, unsigned int y, unsigned int width, unsigned int height) = 0;
        virtual void setScissorRect(unsigned int x, unsigned int y, unsigned int width, unsigned int height) = 0;
        virtual void draw(unsigned int vertexCount) = 0;

    protected:
        RenderPass() = default;
        RenderPass(const RenderPass&) = delete;
        RenderPass(RenderPass&&) = delete;
        virtual RenderPass& operator=(const RenderPass&) = default;
        virtual RenderPass& operator=(RenderPass&&) = default;
    };

    class ComputePass {
    public:
        virtual ~ComputePass() {}

        virtual void setComputeState(const ComputeState&) = 0;
        virtual void setResources(unsigned int index, const std::vector<ResourceReference>&) = 0;
        virtual void dispatch(unsigned int width, unsigned int height, unsigned int depth) = 0;

    protected:
        ComputePass() = default;
        ComputePass(const ComputePass&) = delete;
        ComputePass(ComputePass&&) = delete;
        virtual ComputePass& operator=(const ComputePass&) = default;
        virtual ComputePass& operator=(ComputePass&&) = default;
    };

    class BlitPass {
    public:
        virtual ~BlitPass() {}

        virtual void copyTexture(Texture& source, Texture& destination, unsigned int sourceX, unsigned int sourceY, unsigned int destinationX, unsigned int destinationY, unsigned int width, unsigned int height) = 0;

    protected:
        BlitPass() = default;
        BlitPass(const BlitPass&) = delete;
        BlitPass(BlitPass&&) = delete;
        virtual BlitPass& operator=(const BlitPass&) = default;
        virtual BlitPass& operator=(BlitPass&&) = default;
    };

    class HostAccessPass {
    public:
        virtual ~HostAccessPass() {}

        virtual void overwriteBuffer(Buffer& buffer, const std::vector<uint8_t>& newData) = 0;
        virtual boost::unique_future<std::vector<uint8_t>> getBufferContents(Buffer& buffer) = 0;

    protected:
        HostAccessPass() = default;
        HostAccessPass(const HostAccessPass&) = delete;
        HostAccessPass(HostAccessPass&&) = delete;
        virtual HostAccessPass& operator=(const HostAccessPass&) = default;
        virtual HostAccessPass& operator=(HostAccessPass&&) = default;
    };

    class BufferHolder {
    public:
        BufferHolder() = delete;
        BufferHolder(Device& device, Buffer& buffer);
        ~BufferHolder();
        BufferHolder(const BufferHolder&) = delete;
        BufferHolder(BufferHolder&&);
        BufferHolder& operator=(const BufferHolder&) = delete;
        BufferHolder& operator=(BufferHolder&&) = delete;

        Buffer& get() { return *buffer; }

    private:
        friend class Device;
        Device& device;
        Buffer* buffer;
    };

    class TextureHolder {
    public:
        TextureHolder() = delete;
        TextureHolder(Device& device, Texture& texture);
        ~TextureHolder();
        TextureHolder(const TextureHolder&) = delete;
        TextureHolder(TextureHolder&&);
        TextureHolder& operator=(const TextureHolder&) = delete;
        TextureHolder& operator=(TextureHolder&&) = default;

        Texture& get() { return *texture; }

    private:
        friend class Device;
        Device& device;
        Texture* texture;
    };

    class SamplerHolder {
    public:
        SamplerHolder() = delete;
        SamplerHolder(Device& device, Sampler& texture);
        ~SamplerHolder();
        SamplerHolder(const TextureHolder&) = delete;
        SamplerHolder(SamplerHolder&&);
        SamplerHolder& operator=(const SamplerHolder&) = delete;
        SamplerHolder& operator=(SamplerHolder&&) = default;

        Sampler& get() { return *sampler; }

    private:
        friend class Device;
        Device& device;
        Sampler* sampler;
    };

    enum class AddressMode {
        ClampToEdge,
        MirrorClampToEdge,
        Repeat,
        MirrorRepeat
    };

    enum class Filter {
        Nearest,
        Linear
    };

    class Device {
    public:
        static std::unique_ptr<Device> create(HINSTANCE, HWND); // FIXME: This should probably return a shared_ptr.

        virtual ~Device() {}

        virtual Queue& getCommandQueue() = 0;
        virtual RenderState& getRenderState(const std::vector<uint8_t>& vertexShader, const std::string& vertexShaderName, const std::vector<uint8_t>& fragmentShader, const std::string& fragmentShaderName, const std::vector<unsigned int>& vertexAttributeBufferStrides, const std::vector<RenderState::VertexAttribute>& vertexAttributes, const std::vector<std::vector<ResourceType>>& resources, const std::vector<PixelFormat>* colorPixelFormats) = 0;
        virtual ComputeState& getComputeState(const std::vector<uint8_t>& shader, const std::string& shaderName, const std::vector<std::vector<ResourceType>>& resources) = 0;
        virtual BufferHolder getBuffer(unsigned int length) = 0;
        virtual TextureHolder getTexture(unsigned int width, unsigned int height, PixelFormat format) = 0;
        virtual SamplerHolder getSampler(AddressMode addressMode, Filter filter) = 0;

    protected:
        Device() = default;
        Device(const Device&) = delete;
        Device(Device&&) = delete;
        virtual Device& operator=(const Device&) = default;
        virtual Device& operator=(Device&&) = default;

    private:
        friend class BufferHolder;
        friend class TextureHolder;
        friend class SamplerHolder;
        virtual void returnBuffer(Buffer&) = 0;
        virtual void returnTexture(Texture&) = 0;
        virtual void returnSampler(Sampler&) = 0;
    };
}
