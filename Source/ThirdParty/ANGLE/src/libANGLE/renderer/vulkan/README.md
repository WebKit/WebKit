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

The back-end records commands into command buffers via the the following `ContextVk` APIs:

 * `endRenderPassAndGetCommandBuffer`: returns a secondary command buffer *outside* a RenderPass instance.
 * `flushAndBeginRenderPass`: returns a secondary command buffer *inside* a RenderPass instance.
 * `flushAndGetPrimaryCommandBuffer`: returns the primary command buffer. You should rarely need this API.

*Note*: All of these commands may write out (aka flush) prior pending commands into a primary
command buffer. When a RenderPass is open `endRenderPassAndGetCommandBuffer` will flush the
pending RenderPass commands. `flushAndBeginRenderPass` will flush out pending commands outside a
RenderPass to a primary buffer. On submit ANGLE submits the primary command buffer to a `VkQueue`.

If you need to record inside a RenderPass, use `flushAndBeginRenderPass`. Otherwise, use
`endRenderPassAndGetCommandBuffer`. You should rarely need to call `flushAndGetPrimaryCommandBuffer`.
It's there for commands like debug labels, barriers and queries that need to be recorded serially on
the primary command buffer.

The back-end usually records Image and Buffer barriers through additional `ContextVk` APIs:

 * `onBufferTransferRead/onBufferComputeShaderRead` and `onBufferTransferWrite/onBufferComputeShaderWrite` accumulate `VkBuffer` barriers.
 * `onImageRead` and `onImageWrite` accumulate `VkImage` barriers.
 * `onRenderPassImageWrite` is a special API for write barriers inside a RenderPass instance.

After the back-end records commands to the primary buffer we flush (e.g. on swap) or when we call
`ContextVk::finishToSerial`.

See the [code][CommandAPIs] for more details.

### Simple command recording example

In this example we'll be recording a buffer copy command:

```
    # Ensure that ANGLE sets proper read and write barriers for the Buffers.
    ANGLE_TRY(contextVk->onBufferTransferWrite(destBuffer));
    ANGLE_TRY(contextVk->onBufferTransferRead(srcBuffer));

    # Get a pointer to a secondary command buffer for command recording. May "flush" the RP.
    vk::CommandBuffer *commandBuffer;
    ANGLE_TRY(contextVk->endRenderPassAndGetCommandBuffer(&commandBuffer));

    # Record the copy command into the secondary buffer. We're done!
    commandBuffer->copyBuffer(srcBuffer->getBuffer(), destBuffer->getBuffer(), copyCount, copies);
```

## Additional Reading

More implementation details can be found in the `doc` directory:

- [Fast OpenGL State Transitions](doc/FastOpenGLStateTransitions.md)
- [Shader Module Compilation](doc/ShaderModuleCompilation.md)
- [OpenGL Line Segment Rasterization](doc/OpenGLLineSegmentRasterization.md)
- [Format Tables and Emulation](doc/FormatTablesAndEmulation.md)

[VkDevice]: https://www.khronos.org/registry/vulkan/specs/1.1-extensions/man/html/VkDevice.html
[VkQueue]: https://www.khronos.org/registry/vulkan/specs/1.1-extensions/man/html/VkQueue.html
[CommandAPIs]: https://chromium.googlesource.com/angle/angle/+/df31624eaf3df986a0bdf3f58a87b79b0cc8db5c/src/libANGLE/renderer/vulkan/ContextVk.h#620

