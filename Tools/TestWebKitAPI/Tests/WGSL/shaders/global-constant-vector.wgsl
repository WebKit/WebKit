const a = vec2u(0);
const b = vec2f(a);
@compute @workgroup_size(1)
fn main() {
  let x = vec4f(b, 0, 0);
  _ = x[0];
}
