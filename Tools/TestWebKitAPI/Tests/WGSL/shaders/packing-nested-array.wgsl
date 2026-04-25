struct S {
    x: array<vec3<f32>, 1>,
    y: vec3f,
}


@group(0) @binding(0) var<storage, read> s : S;


struct S0 {
  @align(4)
  x: u32,
}

struct S1 {
  x: array<array<S0, 1>, 1>,
}

@group(0) @binding(1) var<storage, read_write> s1: S1;

struct S2 {
  f: array<array<vec3f, 1>, 1>,
}

@group(0) @binding(2) var<storage, read_write> s2: S2;

@compute @workgroup_size(1)
fn main()
{
  { var x = s.x; }
  { var x = s1.x[0][0].x; }
  { var x = s2.f; }
}
