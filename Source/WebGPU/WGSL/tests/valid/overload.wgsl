fn testAdd() {
  {
    let x1 = 1 + 2;
    let x2 = 1i + 1;
    let x3 = 1 + 1i;
    let x4 = 1u + 1;
    let x5 = 1 + 1u;
    let x6 = 1.0 + 2;
    let x7 = 1 + 2.0;
    let x8 = 1.0 + 2.0;
  }


  {
    let v1 = vec2<f32>(0, 0) + 1;
    let v2 = vec3<f32>(0, 0, 0) + 1;
    let v3 = vec4<f32>(0, 0, 0, 0) + 1;
  }

  {
    let v1 = 1 + vec2<f32>(0, 0);
    let v2 = 1 + vec3<f32>(0, 0, 0);
    let v3 = 1 + vec4<f32>(0, 0, 0, 0);
  }

  {
    let v1 = vec2<f32>(0, 0) + vec2<f32>(1, 1);
    let v2 = vec3<f32>(0, 0, 0) + vec3<f32>(1, 1, 1);
    let v3 = vec4<f32>(0, 0, 0, 0) + vec4<f32>(1, 1, 1, 1);
  }

  {
    let m1 = mat2x2<f32>(0, 0, 0, 0) + mat2x2<f32>(1, 1, 1, 1);
    let m2 = mat2x3<f32>(0, 0, 0, 0, 0, 0) + mat2x3<f32>(1, 1, 1, 1, 1, 1);
    let m3 = mat2x4<f32>(0, 0, 0, 0, 0, 0, 0, 0) + mat2x4<f32>(1, 1, 1, 1, 1, 1, 1, 1);
  }
}

fn testMultiply() {
  let v2 = vec2<f32>(0, 0);
  let v4 = vec4<f32>(0, 0, 0, 0);
  let m = mat2x4<f32>(0, 0, 0, 0, 0, 0, 0, 0);
  let r1 = m * v2;
  let r2 = v4 * m;
}

fn testTextureSample() {
  {
    let t: texture_1d<f32>;
    let s: sampler;
    let r = textureSample(t, s, 1);
  }

  {
    let t: texture_2d<f32>;
    let s: sampler;
    let r = textureSample(t, s, vec2<f32>(0, 0));
  }

  {
    let t: texture_2d<f32>;
    let s: sampler;
    let r = textureSample(t, s, vec2<f32>(0, 0), vec2<i32>(1, 1));
  }

  {
    let t: texture_2d_array<f32>;
    let s: sampler;
    let r = textureSample(t, s, vec2<f32>(0, 0), 0);
  }

  {
    let t: texture_2d_array<f32>;
    let s: sampler;
    let r = textureSample(t, s, vec2<f32>(0, 0), 0, vec2<i32>(1, 1));
  }

  {
    let t: texture_3d<f32>;
    let s: sampler;
    let r = textureSample(t, s, vec3<f32>(0, 0, 0));
  }

  {
    let t: texture_cube<f32>;
    let s: sampler;
    let r = textureSample(t, s, vec3<f32>(0, 0, 0));
  }

  {
    let t: texture_3d<f32>;
    let s: sampler;
    let r = textureSample(t, s, vec3<f32>(0, 0, 0), vec3<i32>(0, 0, 0));
  }

  {
    let t: texture_cube_array<f32>;
    let s: sampler;
    let r = textureSample(t, s, vec3<f32>(0, 0, 0), 0);
  }
}

fn testVec2() {
  let v0 = vec2<f32>(0, 0);
  let v1 = vec2<i32>(vec2(0, 0));
  let v2 = vec2(vec2(0, 0));
  let v3 = vec2(0, 0);
}

fn testVec3() {
  let v0 = vec3<f32>(0, 0, 0);
  let v1 = vec3<i32>(vec3(0, 0, 0));
  let v2 = vec3(vec3(0, 0, 0));
  let v3 = vec3(0, 0, 0);
  let v4 = vec3(vec2(0, 0), 0);
  let v5 = vec3(0, vec2(0, 0));
}

fn testVec4() {
  let v0 = vec4<f32>(0, 0, 0, 0);
  let v1 = vec4<i32>(vec4(0, 0, 0, 0));
  let v2 = vec4(vec4(0, 0, 0, 0));
  let v3 = vec4(0, 0, 0, 0);
  let v4 = vec4(0, vec2(0, 0), 0);
  let v5 = vec4(0, 0, vec2(0, 0));
  let v6 = vec4(vec2(0, 0), 0, 0);
  let v7 = vec4(vec2(0, 0), vec2(0, 0));
  let v8 = vec4(vec3(0, 0, 0), 0);
  let v9 = vec4(0, vec3(0, 0, 0));
}

fn testMatrixConstructor() {
  {
    // FIXME: overload resolution should support explicitly instantiating variables
    // let m1 = mat2x2<f32>(mat2x2(0.0, 0.0, 0.0, 0.0));
    let m2 = mat2x2(mat2x2(0.0, 0.0, 0.0, 0.0));
    let m3 = mat2x2(0.0, 0.0, 0.0, 0.0);
    let m4 = mat2x2(vec2(0.0, 0.0), vec2(0.0, 0.0));
  }

  {
    // FIXME: overload resolution should support explicitly instantiating variables
    // let m1 = mat2x3<f32>(mat2x3(0.0, 0.0, 0.0, 0.0, 0.0, 0.0));
    let m2 = mat2x3(mat2x3(0.0, 0.0, 0.0, 0.0, 0.0, 0.0));
    let m3 = mat2x3(0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
    let m4 = mat2x3(vec3(0.0, 0.0, 0.0), vec3(0.0, 0.0, 0.0));
  }

  {
    // FIXME: overload resolution should support explicitly instantiating variables
    // let m1 = mat2x4<f32>(mat2x4(0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0));
    let m2 = mat2x4(mat2x4(0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0));
    let m3 = mat2x4(0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
    let m4 = mat2x4(vec4(0.0, 0.0, 0.0, 0.0), vec4(0.0, 0.0, 0.0, 0.0));
  }

  {
    // FIXME: overload resolution should support explicitly instantiating variables
    // let m1 = mat3x2<f32>(mat3x2(0.0, 0.0, 0.0, 0.0, 0.0, 0.0));
    let m2 = mat3x2(mat3x2(0.0, 0.0, 0.0, 0.0, 0.0, 0.0));
    let m3 = mat3x2(0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
    let m4 = mat3x2(vec2(0.0, 0.0), vec2(0.0, 0.0), vec2(0.0, 0.0));
  }

  {
    // FIXME: overload resolution should support explicitly instantiating variables
    // let m1 = mat3x3<f32>(mat3x3(0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0));
    let m2 = mat3x3(mat3x3(0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0));
    let m3 = mat3x3(0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
    let m4 = mat3x3(vec3(0.0, 0.0, 0.0), vec3(0.0, 0.0, 0.0), vec3(0.0, 0.0, 0.0));
  }

  {
    // FIXME: overload resolution should support explicitly instantiating variables
    // let m1 = mat3x4<f32>(mat3x4(0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0));
    let m2 = mat3x4(mat3x4(0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0));
    let m3 = mat3x4(0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
    let m4 = mat3x4(vec4(0.0, 0.0, 0.0, 0.0), vec4(0.0, 0.0, 0.0, 0.0), vec4(0.0, 0.0, 0.0, 0.0));
  }

  {
    // FIXME: overload resolution should support explicitly instantiating variables
    // let m1 = mat4x2<f32>(mat4x2(0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0));
    let m2 = mat4x2(mat4x2(0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0));
    let m3 = mat4x2(0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
    let m4 = mat4x2(vec2(0.0, 0.0), vec2(0.0, 0.0), vec2(0.0, 0.0), vec2(0.0, 0.0));
  }

  {
    // FIXME: overload resolution should support explicitly instantiating variables
    // let m1 = mat4x3<f32>(mat4x3(0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0));
    let m2 = mat4x3(mat4x3(0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0));
    let m3 = mat4x3(0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
    let m4 = mat4x3(vec3(0.0, 0.0, 0.0), vec3(0.0, 0.0, 0.0), vec3(0.0, 0.0, 0.0), vec3(0.0, 0.0, 0.0));
  }

  {
    // FIXME: overload resolution should support explicitly instantiating variables
    // let m1 = mat4x4<f32>(mat4x4(0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0));
    let m2 = mat4x4(mat4x4(0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0));
    let m3 = mat4x4(mat4x4(0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0));
    let m4 = mat4x4(vec4(0.0, 0.0, 0.0, 0.0), vec4(0.0, 0.0, 0.0, 0.0), vec4(0.0, 0.0, 0.0, 0.0), vec4(0.0, 0.0, 0.0, 0.0));
  }
}

fn testUnaryMinus() {
  let x = 1;
  let x1 = -x;
  let x2 = -vec2(1, 1);
}

fn testClamp() {
   let x = clamp(0, 0, 0);
   let x1 = clamp(0u, 0u, 0u);
   let x2 = clamp(0i, 0i, 0i);
   let x3 = clamp(0.0, 0.0, 0.0);
   let x4 = clamp(0.0f, 0.0f, 0.0f);
   let x5 = clamp(vec2(0.0, 0.0), vec2(0.0, 0.0), vec2(0.0, 0.0));
   let x6 = clamp(vec3(0.0, 0.0, 0.0), vec3(0.0, 0.0, 0.0), vec3(0.0, 0.0, 0.0));
   let x7 = clamp(vec4(0.0, 0.0, 0.0, 0.0), vec4(0.0, 0.0, 0.0, 0.0), vec4(0.0, 0.0, 0.0, 0.0));
}

fn testMin() {
   let x = min(0, 0u);
   let x1 = min(0u, 0u);
   let x2 = min(0i, 0i);
   let x3 = min(0.0, 0.0);
   let x4 = min(0.0f, 0.0f);
   let x5 = min(vec2(0.0, 0.0), vec2(0.0, 0.0));
   let x6 = min(vec3(0.0, 0.0, 0.0), vec3(0.0, 0.0, 0.0));
   let x7 = min(vec4(0.0, 0.0, 0.0, 0.0), vec4(0.0, 0.0, 0.0, 0.0));
}

fn testMax() {
   let x = max(0, 0u);
   let x1 = max(0u, 0u);
   let x2 = max(0i, 0i);
   let x3 = max(0.0, 0.0);
   let x4 = max(0.0f, 0.0f);
   let x5 = max(vec2(0.0, 0.0), vec2(0.0, 0.0));
   let x6 = max(vec3(0.0, 0.0, 0.0), vec3(0.0, 0.0, 0.0));
   let x7 = max(vec4(0.0, 0.0, 0.0, 0.0), vec4(0.0, 0.0, 0.0, 0.0));
}
