// RUN: %not %wgslc | %check

@group(0) @binding(0) var t: texture_2d<f32>;
@group(0) @binding(1) var s: sampler;

@fragment
fn main() {
  // CHECK-L: the component argument must be at least 0 and at most 3. component is 4
  _ = textureGather(4, t, s, vec2());

  var c = 4;
  // CHECK-L: the component argument must be a const-expression
  _ = textureGather(c, t, s, vec2());
}
