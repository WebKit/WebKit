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

Implementation details can be found in the `doc` directory.

- [Fast OpenGL State Transitions](doc/FastOpenGLStateTransitions.md)
- [Shader Module Compilation](doc/ShaderModuleCompilation.md)
- [OpenGL Line Segment Rasterization](doc/OpenGLLineSegmentRasterization.md)
- [Format Tables and Emulation](doc/FormatTablesAndEmulation.md)

[VkDevice]: https://www.khronos.org/registry/vulkan/specs/1.1-extensions/man/html/VkDevice.html
[VkQueue]: https://www.khronos.org/registry/vulkan/specs/1.1-extensions/man/html/VkQueue.html
