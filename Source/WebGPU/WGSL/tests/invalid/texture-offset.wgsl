// RUN: %not %wgslc | %check

@group(0) @binding(0) var s: sampler;
@group(0) @binding(23) var td2d: texture_depth_2d;

@fragment
fn main() {
    var offset = vec2i(0);

    // CHECK-L: the offset argument must be a const-expression
    _ = textureGather(td2d, s, vec2f(0), offset);

    // CHECK-L: each component of the offset argument must be at least -8 and at most 7. offset component 0 is -9
    _ = textureGather(td2d, s, vec2f(0), vec2i(-9));

    // CHECK-L: each component of the offset argument must be at least -8 and at most 7. offset component 0 is 8
    _ = textureGather(td2d, s, vec2f(0), vec2i(8));
}
