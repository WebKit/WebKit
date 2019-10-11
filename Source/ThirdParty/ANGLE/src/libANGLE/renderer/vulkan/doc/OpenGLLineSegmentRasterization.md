# OpenGL Line Segment Rasterization

OpenGL and Vulkan both render line segments as a series of pixels between two points. They differ in
which pixels cover the line.

For single sample rendering Vulkan uses an algorithm based on quad coverage. A small shape is
extruded around the line segment. Samples covered by the shape then represent the line segment. See
[the Vulkan spec][VulkanLineRaster] for more details.

OpenGL's algorithm is based on [Bresenham's line algorithm][Bresenham]. Bresenham's algorithm
selects pixels on the line between the two segment points. Note Bresenham's does not support
multisampling. When compared visually you can see the Vulkan line segment rasterization algorithm
always selects a superset of the line segment pixels rasterized in OpenGL. See this example:

![Vulkan vs OpenGL Line Rasterization][VulkanVsGLLineRaster]

The OpenGL spec defines a "diamond-exit" rule to select fragments on a line. Please refer to the 2.0
spec section 3.4.1 "Basic Line Segment Rasterization" spec for more details. To implement this rule
we inject a small computation to test if a pixel falls within the diamond in the start of the pixel
shader. If the pixel fails the diamond test we discard the fragment. Note that we only perform this
test when drawing lines. See the section on [Shader Compilation](ShaderModuleCompilation.md) for
more info. See the below diagram for an illustration of the diamond rule:

![OpenGL Diamond Rule Example][DiamondRule]

We can implement the OpenGL test by checking the intersection of the line and the medial axes of the
pixel `p`. If the length of the line segment between intersections `p` and the point center is
greater than a half-pixel for all possible `p` then the pixel is not on the segment. To solve for
`p` we use the pixel center `a` given by `gl_FragCoord` and the projection of `a` onto the line
segment `b` given by the interpolated `gl_Position`. Since `gl_Position` is not available in the
fragment shader we must add an internal position varying when drawing lines.

The full code derivation is omitted for brevity. It reduces to the following shader snippet:

```vec2 position = PositionVarying.xy / PositionVarying.w;
vec2 b = ((position * 0.5) + 0.5) * gl_Viewport.zw + gl_Viewport.xy;
vec2 ba = abs(b - gl_FragCoord.xy);
vec2 ba2 = 2.0 * (ba * ba);
vec2 bp = ba2 + ba2.yx - ba;
if (bp.x > epsilon && bp.y > epsilon)
    discard;
```

Note that we must also pass the viewport size as an internal uniform. We use a small epsilon value
to correct for cases when the line segment is perfectly parallel or perpendicular to the window. For
code please see [TranslatorVulkan.cpp][TranslatorVulkan.cpp] under
`AddLineSegmentRasterizationEmulation`.

[Bresenham]: https://en.wikipedia.org/wiki/Bresenham%27s_line_algorithm
[DiamondRule]: img/LineRasterPixelExample.png
[TranslatorVulkan.cpp]: https://chromium.googlesource.com/angle/angle/+/refs/heads/master/src/compiler/translator/TranslatorVulkan.cpp
[VulkanLineRaster]: https://www.khronos.org/registry/vulkan/specs/1.1/html/chap24.html#primsrast-lines-basic
[VulkanVsGLLineRaster]: img/LineRasterComparison.gif
