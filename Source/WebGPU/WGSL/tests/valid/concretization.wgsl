fn testVariableInialization() {
  let x1: u32 = 0;
  let x2: vec2<u32> = vec2(0, 0);
  let x3: f32 = 0;
  let x4: f32 = 0.0;
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
