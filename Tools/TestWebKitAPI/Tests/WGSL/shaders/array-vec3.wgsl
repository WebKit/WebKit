@group(0) @binding(0) var<storage, read_write> un: array<vec3f, 1>;

@vertex
fn main() -> @builtin(position) vec4f {
  {
    let x = un[0].x;
    let xy = un[0].xy;
    let y = un[0][1];
    un[0].x = x;
    un[0][1] = y;
  }

  {
    var v = un[0];
    let x = v.x;
    let xy = v.xy;
    let y = v[1];
    v.x = x;
    v[1] = y;
  }

  {
    let v = &un[0];
    let x = v.x;
    let xy = v.xy;
    let y = v[1];
    v.x = x;
    v[1] = y;
  }
  return vec4();
}
