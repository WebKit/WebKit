// AUTO-GENERATED - DO NOT EDIT. See src/common/tools/gen_listings.ts.

export const listing = [
  {
    "file": [],
    "readme": "WebGPU conformance test suite."
  },
  {
    "file": [
      "api"
    ],
    "readme": "Tests for full coverage of the Javascript API surface of WebGPU."
  },
  {
    "file": [
      "api",
      "operation"
    ],
    "readme": "Tests that check the result of performing valid WebGPU operations, taking advantage of\nparameterization to exercise interactions between features."
  },
  {
    "file": [
      "api",
      "operation",
      "adapter",
      "requestDevice"
    ]
  },
  {
    "file": [
      "api",
      "operation",
      "adapter",
      "requestDevice_limits"
    ]
  },
  {
    "file": [
      "api",
      "operation",
      "async_ordering"
    ],
    "readme": "Test ordering of async resolutions between promises returned by the following calls (and possibly\nbetween multiple of the same call), where there are constraints on the ordering.\nSpec issue: https://github.com/gpuweb/gpuweb/issues/962\n\nTODO: plan and implement\n- createReadyPipeline() (not sure if this actually has any ordering constraints)\n- cmdbuf.executionTime\n- device.popErrorScope()\n- device.lost\n- queue.onSubmittedWorkDone()\n- buffer.mapAsync()\n- shadermodule.compilationInfo()"
  },
  {
    "file": [
      "api",
      "operation",
      "buffers"
    ],
    "readme": "GPUBuffer tests."
  },
  {
    "file": [
      "api",
      "operation",
      "buffers",
      "map"
    ]
  },
  {
    "file": [
      "api",
      "operation",
      "buffers",
      "map_ArrayBuffer"
    ]
  },
  {
    "file": [
      "api",
      "operation",
      "buffers",
      "map_detach"
    ]
  },
  {
    "file": [
      "api",
      "operation",
      "buffers",
      "map_oom"
    ]
  },
  {
    "file": [
      "api",
      "operation",
      "buffers",
      "threading"
    ]
  },
  {
    "file": [
      "api",
      "operation",
      "command_buffer",
      "basic"
    ]
  },
  {
    "file": [
      "api",
      "operation",
      "command_buffer",
      "clearBuffer"
    ]
  },
  {
    "file": [
      "api",
      "operation",
      "command_buffer",
      "copyBufferToBuffer"
    ]
  },
  {
    "file": [
      "api",
      "operation",
      "command_buffer",
      "copyTextureToTexture"
    ]
  },
  {
    "file": [
      "api",
      "operation",
      "command_buffer",
      "image_copy"
    ]
  },
  {
    "file": [
      "api",
      "operation",
      "command_buffer",
      "programmable",
      "state_tracking"
    ]
  },
  {
    "file": [
      "api",
      "operation",
      "command_buffer",
      "queries"
    ],
    "readme": "TODO: test the behavior of creating/using/resolving queries.\n- occlusion\n- pipeline statistics\n  TODO: pipeline statistics queries are removed from core; consider moving tests to another suite.\n- timestamp\n- nested (e.g. timestamp or PS query inside occlusion query), if any such cases are valid. Try\n  writing to the same query set (at same or different indices), if valid. Check results make sense.\n- start a query (all types) with no draw calls"
  },
  {
    "file": [
      "api",
      "operation",
      "command_buffer",
      "render",
      "dynamic_state"
    ]
  },
  {
    "file": [
      "api",
      "operation",
      "command_buffer",
      "render",
      "state_tracking"
    ]
  },
  {
    "file": [
      "api",
      "operation",
      "compute",
      "basic"
    ]
  },
  {
    "file": [
      "api",
      "operation",
      "compute_pipeline",
      "entry_point_name"
    ]
  },
  {
    "file": [
      "api",
      "operation",
      "device",
      "lost"
    ]
  },
  {
    "file": [
      "api",
      "operation",
      "error_scope"
    ],
    "readme": "TODO: plan and implement\n- test very deeply nested error scopes, make sure errors go to the right place, e.g.\n    - validation, ..., validation, out-of-memory\n    - out-of-memory, validation, ..., validation\n    - out-of-memory, ..., out-of-memory, validation\n    - validation, out-of-memory, ..., out-of-memory\n- use error scopes on two different threads and make sure errors go to the right place\n- unhandled errors always go to the \"original\" device object\n    - test they go nowhere if the original was dropped (attemptGarbageCollection to make sure)"
  },
  {
    "file": [
      "api",
      "operation",
      "labels"
    ]
  },
  {
    "file": [
      "api",
      "operation",
      "memory_allocation"
    ],
    "readme": "Try to stress memory allocators in the implementation and driver.\n\nTODO: plan and implement\n- Tests which (pseudo-randomly?) allocate a bunch of memory and then assert things about the memory\n  (it's not aliased, it's valid to read and write in various ways, accesses read/write the correct data)\n    - Possibly also with OOB accesses/robust buffer access?\n- Tests which are targeted against particular known implementation details"
  },
  {
    "file": [
      "api",
      "operation",
      "memory_sync",
      "buffer",
      "multiple_buffers"
    ]
  },
  {
    "file": [
      "api",
      "operation",
      "memory_sync",
      "buffer",
      "single_buffer"
    ]
  },
  {
    "file": [
      "api",
      "operation",
      "memory_sync",
      "texture",
      "same_subresource"
    ]
  },
  {
    "file": [
      "api",
      "operation",
      "onSubmittedWorkDone"
    ]
  },
  {
    "file": [
      "api",
      "operation",
      "pipeline",
      "default_layout"
    ]
  },
  {
    "file": [
      "api",
      "operation",
      "queue",
      "writeBuffer"
    ]
  },
  {
    "file": [
      "api",
      "operation",
      "render_pass"
    ],
    "readme": "Render pass stuff other than commands (which are in command_buffer/)."
  },
  {
    "file": [
      "api",
      "operation",
      "render_pass",
      "clear_value"
    ]
  },
  {
    "file": [
      "api",
      "operation",
      "render_pass",
      "resolve"
    ]
  },
  {
    "file": [
      "api",
      "operation",
      "render_pass",
      "storeOp"
    ]
  },
  {
    "file": [
      "api",
      "operation",
      "render_pass",
      "storeop2"
    ]
  },
  {
    "file": [
      "api",
      "operation",
      "render_pipeline",
      "alpha_to_coverage"
    ]
  },
  {
    "file": [
      "api",
      "operation",
      "render_pipeline",
      "culling_tests"
    ]
  },
  {
    "file": [
      "api",
      "operation",
      "render_pipeline",
      "entry_point_name"
    ]
  },
  {
    "file": [
      "api",
      "operation",
      "render_pipeline",
      "pipeline_output_targets"
    ]
  },
  {
    "file": [
      "api",
      "operation",
      "render_pipeline",
      "primitive_topology"
    ]
  },
  {
    "file": [
      "api",
      "operation",
      "render_pipeline",
      "sample_mask"
    ]
  },
  {
    "file": [
      "api",
      "operation",
      "render_pipeline",
      "vertex_only_render_pipeline"
    ]
  },
  {
    "file": [
      "api",
      "operation",
      "rendering",
      "basic"
    ]
  },
  {
    "file": [
      "api",
      "operation",
      "rendering",
      "blending"
    ]
  },
  {
    "file": [
      "api",
      "operation",
      "rendering",
      "depth"
    ]
  },
  {
    "file": [
      "api",
      "operation",
      "rendering",
      "depth_clip_clamp"
    ]
  },
  {
    "file": [
      "api",
      "operation",
      "rendering",
      "draw"
    ]
  },
  {
    "file": [
      "api",
      "operation",
      "rendering",
      "indirect_draw"
    ]
  },
  {
    "file": [
      "api",
      "operation",
      "rendering",
      "robust_access_index"
    ]
  },
  {
    "file": [
      "api",
      "operation",
      "resource_init",
      "buffer"
    ]
  },
  {
    "file": [
      "api",
      "operation",
      "resource_init",
      "texture_zero"
    ]
  },
  {
    "file": [
      "api",
      "operation",
      "sampling",
      "anisotropy"
    ]
  },
  {
    "file": [
      "api",
      "operation",
      "sampling",
      "filter_mode"
    ]
  },
  {
    "file": [
      "api",
      "operation",
      "sampling",
      "lod_clamp"
    ]
  },
  {
    "file": [
      "api",
      "operation",
      "shader_module",
      "compilation_info"
    ]
  },
  {
    "file": [
      "api",
      "operation",
      "texture_view",
      "format_reinterpretation"
    ]
  },
  {
    "file": [
      "api",
      "operation",
      "texture_view",
      "read"
    ]
  },
  {
    "file": [
      "api",
      "operation",
      "texture_view",
      "write"
    ]
  },
  {
    "file": [
      "api",
      "operation",
      "threading"
    ],
    "readme": "Tests for behavior with multiple threads (main thread + workers).\n\nTODO: plan and implement\n- 'postMessage'\n  Try postMessage'ing an object of every type (to same or different thread)\n    - {main -> main, main -> worker, worker -> main, worker1 -> worker1, worker1 -> worker2}\n    - through {global postMessage, MessageChannel}\n    - {in, not in} transferrable object list, when valid\n- 'concurrency'\n  Short tight loop doing many of an action from two threads at the same time\n    - e.g. {create {buffer, texture, shader, pipeline}}"
  },
  {
    "file": [
      "api",
      "operation",
      "uncapturederror"
    ]
  },
  {
    "file": [
      "api",
      "operation",
      "vertex_state",
      "correctness"
    ]
  },
  {
    "file": [
      "api",
      "operation",
      "vertex_state",
      "index_format"
    ]
  },
  {
    "file": [
      "api",
      "regression"
    ],
    "readme": "One-off tests that reproduce API bugs found in implementations to prevent the bugs from\nappearing again."
  },
  {
    "file": [
      "api",
      "validation"
    ],
    "readme": "Positive and negative tests for all the validation rules of the API."
  },
  {
    "file": [
      "api",
      "validation",
      "attachment_compatibility"
    ]
  },
  {
    "file": [
      "api",
      "validation",
      "buffer",
      "create"
    ]
  },
  {
    "file": [
      "api",
      "validation",
      "buffer",
      "destroy"
    ]
  },
  {
    "file": [
      "api",
      "validation",
      "buffer",
      "mapping"
    ]
  },
  {
    "file": [
      "api",
      "validation",
      "buffer",
      "threading"
    ]
  },
  {
    "file": [
      "api",
      "validation",
      "capability_checks",
      "features"
    ],
    "readme": "Test every method or option that shouldn't be allowed without a feature enabled.\nIf the feature is not enabled, any use of an enum value added by a feature must be an\n*exception*, per <https://github.com/gpuweb/gpuweb/blob/main/design/ErrorConventions.md>.\n\n- x= that feature {enabled, disabled}\n\nGenerally one file for each feature name, but some may be grouped (e.g. one file for all optional\nquery types, one file for all optional texture formats).\n\nTODO: implement"
  },
  {
    "file": [
      "api",
      "validation",
      "capability_checks",
      "features",
      "depth_clip_control"
    ]
  },
  {
    "file": [
      "api",
      "validation",
      "capability_checks",
      "features",
      "query_types"
    ]
  },
  {
    "file": [
      "api",
      "validation",
      "capability_checks",
      "features",
      "texture_formats"
    ]
  },
  {
    "file": [
      "api",
      "validation",
      "capability_checks",
      "limits"
    ],
    "readme": "Test everything that shouldn't be valid without a higher-than-specified limit.\n\n- x= that limit {default, max supported (if different), lower than default (TODO: if allowed)}\n\nOne file for each limit name.\n\nTODO: implement\nTODO: Also test that \"alignment\" limits require a power of 2."
  },
  {
    "file": [
      "api",
      "validation",
      "createBindGroup"
    ]
  },
  {
    "file": [
      "api",
      "validation",
      "createBindGroupLayout"
    ]
  },
  {
    "file": [
      "api",
      "validation",
      "createComputePipeline"
    ]
  },
  {
    "file": [
      "api",
      "validation",
      "createPipelineLayout"
    ]
  },
  {
    "file": [
      "api",
      "validation",
      "createRenderPipeline"
    ]
  },
  {
    "file": [
      "api",
      "validation",
      "createSampler"
    ]
  },
  {
    "file": [
      "api",
      "validation",
      "createTexture"
    ]
  },
  {
    "file": [
      "api",
      "validation",
      "createView"
    ]
  },
  {
    "file": [
      "api",
      "validation",
      "create_pipeline"
    ]
  },
  {
    "file": [
      "api",
      "validation",
      "encoding",
      "beginRenderPass"
    ]
  },
  {
    "file": [
      "api",
      "validation",
      "encoding",
      "cmds",
      "buffer_texture_copies"
    ]
  },
  {
    "file": [
      "api",
      "validation",
      "encoding",
      "cmds",
      "clearBuffer"
    ]
  },
  {
    "file": [
      "api",
      "validation",
      "encoding",
      "cmds",
      "compute_pass"
    ]
  },
  {
    "file": [
      "api",
      "validation",
      "encoding",
      "cmds",
      "copyBufferToBuffer"
    ]
  },
  {
    "file": [
      "api",
      "validation",
      "encoding",
      "cmds",
      "copyTextureToTexture"
    ]
  },
  {
    "file": [
      "api",
      "validation",
      "encoding",
      "cmds",
      "debug"
    ]
  },
  {
    "file": [
      "api",
      "validation",
      "encoding",
      "cmds",
      "index_access"
    ]
  },
  {
    "file": [
      "api",
      "validation",
      "encoding",
      "cmds",
      "render",
      "draw"
    ]
  },
  {
    "file": [
      "api",
      "validation",
      "encoding",
      "cmds",
      "render",
      "dynamic_state"
    ]
  },
  {
    "file": [
      "api",
      "validation",
      "encoding",
      "cmds",
      "render",
      "indirect_draw"
    ]
  },
  {
    "file": [
      "api",
      "validation",
      "encoding",
      "cmds",
      "render",
      "setIndexBuffer"
    ]
  },
  {
    "file": [
      "api",
      "validation",
      "encoding",
      "cmds",
      "render",
      "setPipeline"
    ]
  },
  {
    "file": [
      "api",
      "validation",
      "encoding",
      "cmds",
      "render",
      "setVertexBuffer"
    ]
  },
  {
    "file": [
      "api",
      "validation",
      "encoding",
      "cmds",
      "render",
      "state_tracking"
    ]
  },
  {
    "file": [
      "api",
      "validation",
      "encoding",
      "cmds",
      "render_pass"
    ]
  },
  {
    "file": [
      "api",
      "validation",
      "encoding",
      "cmds",
      "setBindGroup"
    ]
  },
  {
    "file": [
      "api",
      "validation",
      "encoding",
      "createRenderBundleEncoder"
    ]
  },
  {
    "file": [
      "api",
      "validation",
      "encoding",
      "encoder_state"
    ]
  },
  {
    "file": [
      "api",
      "validation",
      "encoding",
      "programmable",
      "pipeline_bind_group_compat"
    ]
  },
  {
    "file": [
      "api",
      "validation",
      "encoding",
      "queries",
      "begin_end"
    ]
  },
  {
    "file": [
      "api",
      "validation",
      "encoding",
      "queries",
      "general"
    ]
  },
  {
    "file": [
      "api",
      "validation",
      "encoding",
      "queries",
      "pipeline_statistics"
    ]
  },
  {
    "file": [
      "api",
      "validation",
      "encoding",
      "queries",
      "resolveQuerySet"
    ]
  },
  {
    "file": [
      "api",
      "validation",
      "encoding",
      "render_bundle"
    ]
  },
  {
    "file": [
      "api",
      "validation",
      "error_scope"
    ]
  },
  {
    "file": [
      "api",
      "validation",
      "image_copy"
    ],
    "readme": "writeTexture + copyBufferToTexture + copyTextureToBuffer validation tests.\n\nTest coverage:\n* resource usages:\n\t- texture_usage_must_be_valid: for GPUTextureUsage::COPY_SRC, GPUTextureUsage::COPY_DST flags.\n\t- buffer_usage_must_be_valid: for GPUBufferUsage::COPY_SRC, GPUBufferUsage::COPY_DST flags.\n\n* textureCopyView:\n\t- texture_must_be_valid: for valid, destroyed, error textures.\n\t- sample_count_must_be_1: for sample count 1 and 4.\n\t- mip_level_must_be_in_range: for various combinations of mipLevel and mipLevelCount.\n\t- format: for all formats with full and non-full copies on width, height, and depth.\n\t- texel_block_alignment_on_origin: for all formats and coordinates.\n\n* bufferCopyView:\n\t- buffer_must_be_valid: for valid, destroyed, error buffers.\n\t- bytes_per_row_alignment: for bytesPerRow to be 256-byte aligned or not, and bytesPerRow is required or not.\n\n* linear texture data:\n\t- bound_on_rows_per_image: for various combinations of copyDepth (1, >1), copyHeight, rowsPerImage.\n\t- offset_plus_required_bytes_in_copy_overflow\n\t- required_bytes_in_copy: testing minimal data size and data size too small for various combinations of bytesPerRow, rowsPerImage, copyExtent and offset. for the copy method, bytesPerRow is computed as bytesInACompleteRow aligned to be a multiple of 256 + bytesPerRowPadding * 256.\n\t- texel_block_alignment_on_rows_per_image: for all formats.\n\t- offset_alignment: for all formats.\n\t- bound_on_offset: for various combinations of offset and dataSize.\n\n* texture copy range:\n\t- 1d_texture: copyExtent.height isn't 1, copyExtent.depthOrArrayLayers isn't 1.\n\t- texel_block_alignment_on_size: for all formats and coordinates.\n\t- texture_range_conditons: for all coordinate and various combinations of origin, copyExtent, textureSize and mipLevel.\n\nTODO: more test coverage for 1D and 3D textures."
  },
  {
    "file": [
      "api",
      "validation",
      "image_copy",
      "buffer_related"
    ]
  },
  {
    "file": [
      "api",
      "validation",
      "image_copy",
      "layout_related"
    ]
  },
  {
    "file": [
      "api",
      "validation",
      "image_copy",
      "texture_related"
    ]
  },
  {
    "file": [
      "api",
      "validation",
      "initialization",
      "requestDevice"
    ]
  },
  {
    "file": [
      "api",
      "validation",
      "layout_shader_compat"
    ]
  },
  {
    "file": [
      "api",
      "validation",
      "query_set",
      "create"
    ]
  },
  {
    "file": [
      "api",
      "validation",
      "query_set",
      "destroy"
    ]
  },
  {
    "file": [
      "api",
      "validation",
      "queue"
    ],
    "readme": "Tests for validation that occurs inside queued operations\n(submit, writeBuffer, writeTexture, copyExternalImageToTexture).\n\nBufferMapStatesToTest = {\n  mapped -> unmapped,\n  mapped at creation -> unmapped,\n  mapping pending -> unmapped,\n  pending -> mapped (await map),\n  unmapped -> pending (noawait map),\n  created mapped-at-creation,\n}\n\nNote writeTexture is tested in image_copy."
  },
  {
    "file": [
      "api",
      "validation",
      "queue",
      "buffer_mapped"
    ]
  },
  {
    "file": [
      "api",
      "validation",
      "queue",
      "copyToTexture",
      "CopyExternalImageToTexture"
    ]
  },
  {
    "file": [
      "api",
      "validation",
      "queue",
      "destroyed",
      "query_set"
    ]
  },
  {
    "file": [
      "api",
      "validation",
      "queue",
      "submit"
    ]
  },
  {
    "file": [
      "api",
      "validation",
      "queue",
      "writeBuffer"
    ]
  },
  {
    "file": [
      "api",
      "validation",
      "render_pass"
    ],
    "readme": "Render pass stuff other than commands (which are in encoding/cmds/)."
  },
  {
    "file": [
      "api",
      "validation",
      "render_pass",
      "resolve"
    ]
  },
  {
    "file": [
      "api",
      "validation",
      "render_pass",
      "storeOp"
    ]
  },
  {
    "file": [
      "api",
      "validation",
      "render_pass_descriptor"
    ]
  },
  {
    "file": [
      "api",
      "validation",
      "resource_usages",
      "buffer"
    ],
    "readme": "TODO: look at texture,*"
  },
  {
    "file": [
      "api",
      "validation",
      "resource_usages",
      "texture",
      "in_pass_encoder"
    ]
  },
  {
    "file": [
      "api",
      "validation",
      "resource_usages",
      "texture",
      "in_render_common"
    ]
  },
  {
    "file": [
      "api",
      "validation",
      "resource_usages",
      "texture",
      "in_render_misc"
    ]
  },
  {
    "file": [
      "api",
      "validation",
      "state",
      "device_lost"
    ],
    "readme": "Tests of behavior while the device is lost.\n\n- x= every method in the API.\n\nTODO: implement"
  },
  {
    "file": [
      "api",
      "validation",
      "state",
      "device_lost",
      "destroy"
    ]
  },
  {
    "file": [
      "api",
      "validation",
      "texture",
      "destroy"
    ]
  },
  {
    "file": [
      "api",
      "validation",
      "vertex_state"
    ]
  },
  {
    "file": [
      "examples"
    ]
  },
  {
    "file": [
      "idl"
    ],
    "readme": "Tests to check that the WebGPU IDL is correctly implemented, for examples that objects exposed\nexactly the correct members, and that methods throw when passed incomplete dictionaries.\n\nSee https://github.com/gpuweb/cts/issues/332\n\nTODO: exposed.html.ts: Test all WebGPU interfaces instead of just some of them.\nTODO: Check prototype chains. (Add a helper in IDLTest for this.)"
  },
  {
    "file": [
      "idl",
      "constants",
      "flags"
    ]
  },
  {
    "file": [
      "shader"
    ],
    "readme": "Tests for full coverage of the shaders that can be passed to WebGPU."
  },
  {
    "file": [
      "shader",
      "execution"
    ],
    "readme": "Tests that check the result of valid shader execution."
  },
  {
    "file": [
      "shader",
      "execution",
      "evaluation_order"
    ]
  },
  {
    "file": [
      "shader",
      "execution",
      "expression",
      "binary",
      "bitwise"
    ]
  },
  {
    "file": [
      "shader",
      "execution",
      "expression",
      "binary",
      "f32_arithmetic"
    ]
  },
  {
    "file": [
      "shader",
      "execution",
      "expression",
      "binary",
      "f32_logical"
    ]
  },
  {
    "file": [
      "shader",
      "execution",
      "expression",
      "call",
      "builtin",
      "abs"
    ]
  },
  {
    "file": [
      "shader",
      "execution",
      "expression",
      "call",
      "builtin",
      "all"
    ]
  },
  {
    "file": [
      "shader",
      "execution",
      "expression",
      "call",
      "builtin",
      "any"
    ]
  },
  {
    "file": [
      "shader",
      "execution",
      "expression",
      "call",
      "builtin",
      "atan"
    ]
  },
  {
    "file": [
      "shader",
      "execution",
      "expression",
      "call",
      "builtin",
      "atan2"
    ]
  },
  {
    "file": [
      "shader",
      "execution",
      "expression",
      "call",
      "builtin",
      "ceil"
    ]
  },
  {
    "file": [
      "shader",
      "execution",
      "expression",
      "call",
      "builtin",
      "clamp"
    ]
  },
  {
    "file": [
      "shader",
      "execution",
      "expression",
      "call",
      "builtin",
      "cos"
    ]
  },
  {
    "file": [
      "shader",
      "execution",
      "expression",
      "call",
      "builtin",
      "countLeadingZeros"
    ]
  },
  {
    "file": [
      "shader",
      "execution",
      "expression",
      "call",
      "builtin",
      "countOneBits"
    ]
  },
  {
    "file": [
      "shader",
      "execution",
      "expression",
      "call",
      "builtin",
      "countTrailingZeros"
    ]
  },
  {
    "file": [
      "shader",
      "execution",
      "expression",
      "call",
      "builtin",
      "extractBits"
    ]
  },
  {
    "file": [
      "shader",
      "execution",
      "expression",
      "call",
      "builtin",
      "firstLeadingBit"
    ]
  },
  {
    "file": [
      "shader",
      "execution",
      "expression",
      "call",
      "builtin",
      "firstTrailingBit"
    ]
  },
  {
    "file": [
      "shader",
      "execution",
      "expression",
      "call",
      "builtin",
      "float_built_functions"
    ]
  },
  {
    "file": [
      "shader",
      "execution",
      "expression",
      "call",
      "builtin",
      "floor"
    ]
  },
  {
    "file": [
      "shader",
      "execution",
      "expression",
      "call",
      "builtin",
      "fract"
    ]
  },
  {
    "file": [
      "shader",
      "execution",
      "expression",
      "call",
      "builtin",
      "insertBits"
    ]
  },
  {
    "file": [
      "shader",
      "execution",
      "expression",
      "call",
      "builtin",
      "inversesqrt"
    ]
  },
  {
    "file": [
      "shader",
      "execution",
      "expression",
      "call",
      "builtin",
      "ldexp"
    ]
  },
  {
    "file": [
      "shader",
      "execution",
      "expression",
      "call",
      "builtin",
      "log"
    ]
  },
  {
    "file": [
      "shader",
      "execution",
      "expression",
      "call",
      "builtin",
      "log2"
    ]
  },
  {
    "file": [
      "shader",
      "execution",
      "expression",
      "call",
      "builtin",
      "logical_built_in_functions"
    ]
  },
  {
    "file": [
      "shader",
      "execution",
      "expression",
      "call",
      "builtin",
      "max"
    ]
  },
  {
    "file": [
      "shader",
      "execution",
      "expression",
      "call",
      "builtin",
      "min"
    ]
  },
  {
    "file": [
      "shader",
      "execution",
      "expression",
      "call",
      "builtin",
      "reverseBits"
    ]
  },
  {
    "file": [
      "shader",
      "execution",
      "expression",
      "call",
      "builtin",
      "select"
    ]
  },
  {
    "file": [
      "shader",
      "execution",
      "expression",
      "call",
      "builtin",
      "sin"
    ]
  },
  {
    "file": [
      "shader",
      "execution",
      "expression",
      "call",
      "builtin",
      "value_testing_built_in_functions"
    ]
  },
  {
    "file": [
      "shader",
      "execution",
      "expression",
      "unary",
      "f32_arithmetic"
    ]
  },
  {
    "file": [
      "shader",
      "execution",
      "memory_model",
      "atomicity"
    ]
  },
  {
    "file": [
      "shader",
      "execution",
      "memory_model",
      "barrier"
    ]
  },
  {
    "file": [
      "shader",
      "execution",
      "memory_model",
      "coherence"
    ]
  },
  {
    "file": [
      "shader",
      "execution",
      "memory_model",
      "weak"
    ]
  },
  {
    "file": [
      "shader",
      "execution",
      "robust_access"
    ]
  },
  {
    "file": [
      "shader",
      "execution",
      "robust_access_vertex"
    ]
  },
  {
    "file": [
      "shader",
      "execution",
      "sampling",
      "gradients_in_varying_loop"
    ]
  },
  {
    "file": [
      "shader",
      "execution",
      "shader_io",
      "compute_builtins"
    ]
  },
  {
    "file": [
      "shader",
      "execution",
      "shader_io",
      "shared_structs"
    ]
  },
  {
    "file": [
      "shader",
      "execution",
      "zero_init"
    ]
  },
  {
    "file": [
      "shader",
      "regression"
    ],
    "readme": "One-off tests that reproduce shader bugs found in implementations to prevent the bugs from\nappearing again."
  },
  {
    "file": [
      "shader",
      "validation"
    ],
    "readme": "Positive and negative tests for all the validation rules of the shading language."
  },
  {
    "file": [
      "shader",
      "validation",
      "shader_io",
      "builtins"
    ]
  },
  {
    "file": [
      "shader",
      "validation",
      "shader_io",
      "generic"
    ]
  },
  {
    "file": [
      "shader",
      "validation",
      "shader_io",
      "interpolate"
    ]
  },
  {
    "file": [
      "shader",
      "validation",
      "shader_io",
      "invariant"
    ]
  },
  {
    "file": [
      "shader",
      "validation",
      "shader_io",
      "locations"
    ]
  },
  {
    "file": [
      "shader",
      "validation",
      "tokens"
    ]
  },
  {
    "file": [
      "shader",
      "validation",
      "variable_and_const"
    ]
  },
  {
    "file": [
      "shader",
      "validation",
      "wgsl",
      "basic"
    ]
  },
  {
    "file": [
      "util",
      "texture",
      "texel_data"
    ]
  },
  {
    "file": [
      "util",
      "texture",
      "texture_ok"
    ]
  },
  {
    "file": [
      "web_platform"
    ],
    "readme": "Tests for Web platform-specific interactions like GPUCanvasContext and canvas, WebXR,\nImageBitmaps, and video APIs.\n\nTODO(#922): Also hopefully tests for user-initiated readbacks from WebGPU canvases\n(printing, save image as, etc.)"
  },
  {
    "file": [
      "web_platform",
      "canvas"
    ],
    "readme": "Tests for WebGPU <canvas> and OffscreenCanvas presentation."
  },
  {
    "file": [
      "web_platform",
      "canvas",
      "configure"
    ]
  },
  {
    "file": [
      "web_platform",
      "canvas",
      "context_creation"
    ]
  },
  {
    "file": [
      "web_platform",
      "canvas",
      "getCurrentTexture"
    ]
  },
  {
    "file": [
      "web_platform",
      "canvas",
      "getPreferredFormat"
    ]
  },
  {
    "file": [
      "web_platform",
      "canvas",
      "readbackFromWebGPUCanvas"
    ]
  },
  {
    "file": [
      "web_platform",
      "copyToTexture",
      "ImageBitmap"
    ]
  },
  {
    "file": [
      "web_platform",
      "copyToTexture"
    ],
    "readme": "Tests for copyToTexture from all possible sources (video, canvas, ImageBitmap, ...)"
  },
  {
    "file": [
      "web_platform",
      "copyToTexture",
      "canvas"
    ]
  },
  {
    "file": [
      "web_platform",
      "copyToTexture",
      "video"
    ]
  },
  {
    "file": [
      "web_platform",
      "external_texture"
    ],
    "readme": "Tests for external textures."
  },
  {
    "file": [
      "web_platform",
      "external_texture",
      "video"
    ]
  },
  {
    "file": [
      "web_platform",
      "reftests"
    ],
    "readme": "Reference tests (reftests) for WebGPU canvas presentation.\n\nThese render some contents to a canvas using WebGPU, and WPT compares the rendering result with\nthe \"reference\" versions (in `ref/`) which render with 2D canvas.\n\nThis tests things like:\n- The canvas has the correct orientation.\n- The canvas renders with the correct transfer function.\n- The canvas blends and interpolates in the correct color encoding.\n\nTODO(#918): Test all possible color spaces (once we have more than 1)\nTODO(#921): Why is there sometimes a difference of 1 (e.g. 3f vs 40) in canvas_size_different_with_back_buffer_size?\nAnd why does chromium's image_diff show diffs on other pixels that don't seem to have diffs?\nTODO(#1093): Test rgba16float values which are out of gamut of the canvas but under SDR luminance.\nTODO(#1093): Test rgba16float values which are above SDR luminance.\nTODO(#1116): Test canvas scaling."
  },
  {
    "file": [
      "web_platform",
      "worker",
      "worker"
    ]
  }
];
