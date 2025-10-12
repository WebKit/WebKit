@compute @workgroup_size(1)
fn main()
{
    _ = unpack4x8snorm(pack4x8snorm(vec4f(1.0, 0.5, -0.5, -1)));
    _ = unpack4x8unorm(pack4x8unorm(vec4f(1.0, 0.5, 0.25, 0)));
    _ = unpack4xI8(pack4xI8(vec4i(128, 127, -128, -129)));
    _ = unpack4xU8(pack4xU8(vec4u(256, 255, 128, 64)));
    _ = unpack4xI8(pack4xI8Clamp(vec4i(128, 127, -128, -129)));
    _ = unpack4xU8(pack4xU8Clamp(vec4u(256, 255, 128, 64)));
    _ = unpack2x16snorm(pack2x16snorm(vec2f(0.5, -0.5)));
    _ = unpack2x16unorm(pack2x16unorm(vec2f(0.5, -0.5)));
    _ = unpack2x16float(pack2x16float(vec2f(0.5, -0.5)));
}
