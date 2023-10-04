// RUN: %metal-compile main
struct S {
  @location(0) x : vec3<f32>,
  @location(1) y : vec4<f32>,
}

@group(0) @binding(0) var<storage> s: S;

@fragment
fn main() -> S {
  var output : S;
  output.x = vec3f(0);
  output.y = vec4f(1);
  return output;
}
