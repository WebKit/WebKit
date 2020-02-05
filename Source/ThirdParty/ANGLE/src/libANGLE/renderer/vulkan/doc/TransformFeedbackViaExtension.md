# Transform Feedback via extension

## Outline

ANGLE emulates transform feedback using the vertexPipelineStoresAndAtomics features in Vulkan.
But some GPU vendors do not support these atomics. Also the emulation becomes more difficult in
GLES 3.2. Therefore ANGLE must support using the VK_EXT_transform_feedback extension .

But some GPU vendor does not support this feature, So we need another implementation using
VK_EXT_transform_feedback.

We also expect a performance gain when we use this extension.

## Implementation of Pause/Resume using CounterBuffer

The Vulkan extension does not provide separate APIs for `glPauseTransformFeedback` /
`glEndTransformFeedback`.

Instead, Vulkan introduced Counter buffers in `vkCmdBeginTransformFeedbackEXT` /
`vkCmdEndTransformFeedbackEXT` as API parameters.

To pause, we call `vkCmdEndTransformFeedbackEXT` and provide valid buffer handles in the
`pCounterBuffers` array and valid offsets in the `pCounterBufferOffsets` array for the
implementation to save the resume points.

Then to resume, we call `vkCmdBeginTransformFeedbackEXT` with the previous `pCounterBuffers`
and `pCounterBufferOffsets` values.

Between the pause and resume there needs to be a memory barrier for the counter buffers with a
source access of `VK_ACCESS_TRANSFORM_FEEDBACK_COUNTER_WRITE_BIT_EXT` at pipeline stage
`VK_PIPELINE_STAGE_TRANSFORM_FEEDBACK_BIT_EXT` to a destination access of
`VK_ACCESS_TRANSFORM_FEEDBACK_COUNTER_READ_BIT_EXT` at pipeline stage
`VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT`.

## Implementation of glTransformFeedbackVaryings

There is no equivalent function for glTransformFeedbackVaryings in Vulkan. The Vulkan specification
states that the last vertex processing stage shader must be declared with the XFB execution mode.

So we need to modify gl shader code to have transform feedback qualifiers. The glsl code will be
converted proper SPIR-V code.

we add the below layout qualifier for built-in XFB varyings.

```
out gl_PerVertex

{

layout(xfb_buffer = buffer_num, xfb_offset = offset, xfb_stride = stride) varying_type varying_name;

}
```

 And for user xfb varyings.

```
layout(xfb_buffer = buffer_num, xfb_offset = offset, xfb_stride = stride, location = num )
out varying_type varying_name;

```

There are some corner cases we should handle.

If more than 2 built-in varyings are used in the shader, and only one varying is declared as a
transformFeedback varying, we can generate a layout qualifier like this.

```
out gl_PerVertex

{

layout(xfb_buffer = buffer_num, xfb_offset = offset, xfb_stride = stride) varying_type varying_name1;

varying_type varying_name2;

...

}
```

ANGLE modifies gl_position.z in vertex shader for the Vulkan coordinate system. So, if we capture
the value of 'gl_position' in the XFB buffer, the captured values will be incorrect.

To resolve this, we declare user declare an internal position varying and copy the value from
'gl_position'. We capture the internal position varying during transform feedback operation.

```
layout(xfb_buffer = buffer_num, xfb_offset = offset, xfb_stride = stride, location = num )
out vec4 xfbANGLEPosition;

....

void main(){

...

xfbANGLEPosition = gl_Position;
(gl_Position.z = ((gl_Position.z + gl_Position.w) * 0.5));
}
```

