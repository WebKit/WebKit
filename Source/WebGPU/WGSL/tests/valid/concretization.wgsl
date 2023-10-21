// RUN: %wgslc

fn testVariableInialization() {
  let x1: u32 = 0;
  let x2: vec2<u32> = vec2(0, 0);
  let x3: f32 = 0;
  let x4: f32 = 0.0;

  var v1 = vec2(0.0);
  v1 = vec2f(0);
}

@vertex
fn testConcretizationOfArguments() {
  // CHECK-L: vec<unsigned, 2>(0, 0);
  let x1 = 0u + vec2(0, 0);

  // CHECK-L: vec<int, 2>(0, 0);
  let x2 = 0i + vec2(0, 0);

  // CHECK-L: vec<float, 2>(0, 0);
  let x3 = 0f + vec2(0, 0);
}

@vertex
fn testArrayConcretization() {
  // CHECK-L: vec<int, 2>(0, 0),
  let x1 = array<vec2<i32>, 1>(vec2(0, 0));

  // CHECK-L: vec<float, 2>(0, 0),
  let x2 = array<vec2<f32>, 1>(vec2(0, 0));

  // CHECK-L: vec<float, 2>(0, 0),
  // CHECK-L: vec<float, 2>(0, 0),
  let x3 = array(vec2(0, 0), vec2(0.0, 0.0));

  // CHECK-L: vec<unsigned, 2>(0, 0),
  // CHECK-L: vec<unsigned, 2>(0, 0),
  let x4 = array(vec2(0, 0), vec2(0u, 0u));
}

@vertex
fn testInitializerConcretization() {
  // CHECK-L: vec<int, 2>(0, 0)
  let x1 = vec2(0, 0);

  // CHECK-L: vec<unsigned, 2>(0, 0)
  let x2 : vec2<u32> = vec2(0, 0);
}

@group(0) @binding(0) var<storage, read_write> a: array<atomic<i32>>;
@compute @workgroup_size(1)
fn testArrayAccessMaterialization()
{
    let i = 0;
    let x = atomicLoad(&a[i]);
}
