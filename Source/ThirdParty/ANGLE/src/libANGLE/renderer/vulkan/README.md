# ANGLE: Vulkan Back-end

ANGLE's Vulkan back-end implementation lives in this folder.

[Vulkan](https://www.khronos.org/vulkan/) is an explicit graphics API. It has a lot in common with
other explicit APIs such as Microsoft's [D3D12][D3D12 Guide] and Apple's
[Metal](https://developer.apple.com/metal/). Compared to APIs like OpenGL or D3D11 explicit APIs can
offer a number of significant benefits:

 * Lower API call CPU overhead.
 * A smaller API surface with more direct hardware control.
 * Better support for multi-core programming.
 * Vulkan in particular has open-source tooling and tests.

[D3D12 Guide]: https://docs.microsoft.com/en-us/windows/desktop/direct3d12/directx-12-programming-guide

## Back-end Design

The [`RendererVk`](RendererVk.cpp) class represents an `EGLDisplay`. `RendererVk` owns shared global
resources like the [VkDevice][VkDevice], [VkQueue][VkQueue], the [Vulkan format tables](vk_format_utils.h)
and [internal Vulkan shaders](shaders). The [ContextVk](ContextVk.cpp) class implements the back-end
of a front-end OpenGL Context. ContextVk processes state changes and handles action commands like
`glDrawArrays` and `glDrawElements`.

## Command recording

The back-end records commands into command buffers via the following `ContextVk` APIs:

 * `beginNewRenderPass`: Writes out (aka flushes) prior pending commands into a primary command
   buffer, then starts a new render pass. Returns a secondary command buffer *inside* a render pass
   instance.
 * `getOutsideRenderPassCommandBuffer`: May flush prior command buffers and close the render pass if
   necessary, in addition to issuing the appropriate barriers. Returns a secondary command buffer
   *outside* a render pass instance.
 * `getStartedRenderPassCommands`: Returns a reference to the currently open render pass' commands
   buffer.

The back-end (mostly) records Image and Buffer barriers through additional `CommandBufferAccess`
APIs, the result of which is passed to `getOutsideRenderPassCommandBuffer`.  Note that the
barriers are not actually recorded until `getOutsideRenderPassCommandBuffer` is called:

 * `onBufferTransferRead` and `onBufferComputeShaderRead` accumulate `VkBuffer` read barriers.
 * `onBufferTransferWrite` and `onBufferComputeShaderWrite` accumulate `VkBuffer` write barriers.
 * `onBuffferSelfCopy` is a special case for `VkBuffer` self copies. It behaves the same as write.
 * `onImageTransferRead` and `onImageComputerShadeRead` accumulate `VkImage` read barriers.
 * `onImageTransferWrite` and `onImageComputerShadeWrite` accumulate `VkImage` write barriers.
 * `onImageRenderPassRead` and `onImageRenderPassWrite` accumulate `VkImage` barriers inside a
   started RenderPass.

After the back-end records commands to the primary buffer and we flush (e.g. on swap) or when we call
`ContextVk::finishToSerial`, ANGLE submits the primary command buffer to a `VkQueue`.

See the [code][CommandAPIs] for more details.

### Simple command recording example

In this example we'll be recording a buffer copy command:

```
    // Ensure that ANGLE sets proper read and write barriers for the Buffers.
    vk::CommandBufferAccess access;
    access.onBufferTransferWrite(dstBuffer);
    access.onBufferTransferRead(srcBuffer);

    // Get a pointer to a secondary command buffer for command recording.
    vk::OutsideRenderPassCommandBuffer *commandBuffer;
    ANGLE_TRY(contextVk->getOutsideRenderPassCommandBuffer(access, &commandBuffer));

    // Record the copy command into the secondary buffer. We're done!
    commandBuffer->copyBuffer(srcBuffer->getBuffer(), dstBuffer->getBuffer(), copyCount, copies);
```

## Additional Reading

More implementation details can be found in the `doc` directory:

- [Fast OpenGL State Transitions](doc/FastOpenGLStateTransitions.md)
- [Shader Module Compilation](doc/ShaderModuleCompilation.md)
- [OpenGL Line Segment Rasterization](doc/OpenGLLineSegmentRasterization.md)
- [Format Tables and Emulation](doc/FormatTablesAndEmulation.md)
- [Deferred Clears](doc/DeferredClears.md)
- [Queries](doc/Queries.md)

[VkDevice]: https://www.khronos.org/registry/vulkan/specs/1.1-extensions/man/html/VkDevice.html
[VkQueue]: https://www.khronos.org/registry/vulkan/specs/1.1-extensions/man/html/VkQueue.html
[CommandAPIs]: https://chromium.googlesource.com/angle/angle/+/df31624eaf3df986a0bdf3f58a87b79b0cc8db5c/src/libANGLE/renderer/vulkan/ContextVk.h#620

