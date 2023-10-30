// RUN: %metal-compile main
struct S {
  @builtin(frag_depth) depth: f32,
  @location(0) x : vec3<f32>,
  @location(1) y : vec4<f32>,
}

@group(0) @binding(0) var<storage> s: S;

@fragment
fn main() -> S {
  var output : S;
  output.x = vec3f(0);
  output.y = vec4f(1);
  output.depth = 1;
  return output;
}

// RUN: %metal-compile main2
@group(0) @binding(1) var inputTexture: texture_2d<f32>;
@fragment fn main2(@builtin(position) fragcoord : vec4<f32>) -> @builtin(frag_depth) f32
{
    var depthValue : vec4<f32> = textureLoad(inputTexture, vec2<i32>(fragcoord.xy), 0);
    return depthValue.x;
}
