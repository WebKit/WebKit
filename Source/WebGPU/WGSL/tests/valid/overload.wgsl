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
  {
    let x = 0 * 0;
    let x1 = 0i * 0i;
    let x2 = 0u * 0u;
    let x3 = 0.0 * 0.0;
    let x4 = 0.0f * 0.0f;
  }

  let v2 = vec2<f32>(0, 0);
  let v4 = vec4<f32>(0, 0, 0, 0);
  let m = mat2x4<f32>(0, 0, 0, 0, 0, 0, 0, 0);
  let r1 = m * v2;
  let r2 = v4 * m;
  let r3 = vec2(1, 1) * 1;
  let r4 = 1 * vec2(1, 1);
  let r5 = vec2(1, 1) * vec2(1, 1);

  let x0 = mat2x2(0, 0, 0, 0) * mat2x2(0, 0, 0, 0);
  let x1 = mat2x2(0, 0, 0, 0) * mat3x2(0, 0, 0, 0, 0, 0);
  let x2 = mat2x2(0, 0, 0, 0) * mat4x2(0, 0, 0, 0, 0, 0, 0, 0);
}

fn testDivision() {
   let x = 0 / 1;
   let x1 = 0i / 1i;
   let x2 = 0u / 1u;
   let x3 = 0.0 / 1.0;
   let x4 = 0.0f / 1.0f;
   let x5 = vec2(0.0) / 1.0;
   let x6 = 0.0 / vec2(1.0, 1.0);
   let x7 = vec2(0.0, 0.0) / vec2(1.0, 1.0);
}

fn testModulo() {
   let x = 0 % 1;
   let x1 = 0i % 1i;
   let x2 = 0u % 1u;
   let x3 = 0.0 % 1.0;
   let x4 = 0.0f % 1.0f;
   let x5 = vec2(0.0) % 1.0;
   let x6 = 0.0 % vec2(1.0, 1.0);
   let x7 = vec2(0.0, 0.0) % vec2(1.0, 1.0);
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
  let v = vec2<f32>(0);
  let v0 = vec2<f32>(0, 0);
  let v1 = vec2<i32>(vec2(0, 0));
  let v2 = vec2(vec2(0, 0));
  let v3 = vec2(0, 0);
}

fn testVec3() {
  let v = vec3<f32>(0);
  let v0 = vec3<f32>(0, 0, 0);
  let v1 = vec3<i32>(vec3(0, 0, 0));
  let v2 = vec3(vec3(0, 0, 0));
  let v3 = vec3(0, 0, 0);
  let v4 = vec3(vec2(0, 0), 0);
  let v5 = vec3(0, vec2(0, 0));
}

fn testVec4() {
  let v = vec4<f32>(0);
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

fn testTrigonometric() {
  {
    let x = acos(0.0);
    let x1 = acos(vec2(0.0, 0.0));
    let x2 = acos(vec3(0.0, 0.0, 0.0));
    let x3 = acos(vec4(0.0, 0.0, 0.0, 0.0));
  }

  {
    let x = asin(0.0);
    let x1 = asin(vec2(0.0, 0.0));
    let x2 = asin(vec3(0.0, 0.0, 0.0));
    let x3 = asin(vec4(0.0, 0.0, 0.0, 0.0));
  }

  {
    let x = atan(0.0);
    let x1 = atan(vec2(0.0, 0.0));
    let x2 = atan(vec3(0.0, 0.0, 0.0));
    let x3 = atan(vec4(0.0, 0.0, 0.0, 0.0));
  }

  {
    let x = cos(0.0);
    let x1 = cos(vec2(0.0, 0.0));
    let x2 = cos(vec3(0.0, 0.0, 0.0));
    let x3 = cos(vec4(0.0, 0.0, 0.0, 0.0));
  }

  {
    let x = sin(0.0);
    let x1 = sin(vec2(0.0, 0.0));
    let x2 = sin(vec3(0.0, 0.0, 0.0));
    let x3 = sin(vec4(0.0, 0.0, 0.0, 0.0));
  }

  {
    let x = tan(0.0);
    let x1 = tan(vec2(0.0, 0.0));
    let x2 = tan(vec3(0.0, 0.0, 0.0));
    let x3 = tan(vec4(0.0, 0.0, 0.0, 0.0));
  }
}

fn testTrigonometricHyperbolic() {
  {
    let x = acosh(0.0);
    let x1 = acosh(vec2(0.0, 0.0));
    let x2 = acosh(vec3(0.0, 0.0, 0.0));
    let x3 = acosh(vec4(0.0, 0.0, 0.0, 0.0));
  }

  {
    let x = asinh(0.0);
    let x1 = asinh(vec2(0.0, 0.0));
    let x2 = asinh(vec3(0.0, 0.0, 0.0));
    let x3 = asinh(vec4(0.0, 0.0, 0.0, 0.0));
  }

  {
    let x = atanh(0.0);
    let x1 = atanh(vec2(0.0, 0.0));
    let x2 = atanh(vec3(0.0, 0.0, 0.0));
    let x3 = atanh(vec4(0.0, 0.0, 0.0, 0.0));
  }

  {
    let x = cosh(0.0);
    let x1 = cosh(vec2(0.0, 0.0));
    let x2 = cosh(vec3(0.0, 0.0, 0.0));
    let x3 = cosh(vec4(0.0, 0.0, 0.0, 0.0));
  }

  {
    let x = sinh(0.0);
    let x1 = sinh(vec2(0.0, 0.0));
    let x2 = sinh(vec3(0.0, 0.0, 0.0));
    let x3 = sinh(vec4(0.0, 0.0, 0.0, 0.0));
  }

  {
    let x = tanh(0.0);
    let x1 = tanh(vec2(0.0, 0.0));
    let x2 = tanh(vec3(0.0, 0.0, 0.0));
    let x3 = tanh(vec4(0.0, 0.0, 0.0, 0.0));
  }
}

fn testBinaryMinus() {
  let x1 = vec2(1, 1) - 1;
  let x2 = 1 - vec2(1, 1);
  let x3 = vec2(1, 1) - vec2(1, 1);
}

fn testComparison() {
  {
    let x = false == true;
    let x1 = 0 == 1;
    let x2 = 0i == 1i;
    let x3 = 0u == 1u;
    let x4 = 0.0 == 1.0;
    let x5 = 0.0f == 1.0f;
    let x6 = vec2(false) == vec2(true);
    let x7 = vec2(0) == vec2(1);
    let x8 = vec2(0i) == vec2(1i);
    let x9 = vec2(0u) == vec2(1u);
    let x10 = vec2(0.0) == vec2(1.0);
    let x11 = vec2(0.0f) == vec2(1.0f);
  }

  {
    let x = false != true;
    let x1 = 0 != 1;
    let x2 = 0i != 1i;
    let x3 = 0u != 1u;
    let x4 = 0.0 != 1.0;
    let x5 = 0.0f != 1.0f;
    let x6 = vec2(false) != vec2(true);
    let x7 = vec2(0) != vec2(1);
    let x8 = vec2(0i) != vec2(1i);
    let x9 = vec2(0u) != vec2(1u);
    let x10 = vec2(0.0) != vec2(1.0);
    let x11 = vec2(0.0f) != vec2(1.0f);
  }

  {
    let x = 0 > 1;
    let x1 = 0i > 1i;
    let x2 = 0u > 1u;
    let x3 = 0.0 > 1.0;
    let x4 = 0.0f > 1.0f;
    let x5 = vec2(0) > vec2(1);
    let x6 = vec2(0i) > vec2(1i);
    let x7 = vec2(0u) > vec2(1u);
    let x8 = vec2(0.0) > vec2(1.0);
    let x9 = vec2(0.0f) > vec2(1.0f);
  }

  {
    let x = 0 >= 1;
    let x1 = 0i >= 1i;
    let x2 = 0u >= 1u;
    let x3 = 0.0 >= 1.0;
    let x4 = 0.0f >= 1.0f;
    let x5 = vec2(0) >= vec2(1);
    let x6 = vec2(0i) >= vec2(1i);
    let x7 = vec2(0u) >= vec2(1u);
    let x8 = vec2(0.0) >= vec2(1.0);
    let x9 = vec2(0.0f) >= vec2(1.0f);
  }

  {
    let x = 0 < 1;
    let x1 = 0i < 1i;
    let x2 = 0u < 1u;
    let x3 = 0.0 < 1.0;
    let x4 = 0.0f < 1.0f;
    let x5 = vec2(0) < vec2(1);
    let x6 = vec2(0i) < vec2(1i);
    let x7 = vec2(0u) < vec2(1u);
    let x8 = vec2(0.0) < vec2(1.0);
    let x9 = vec2(0.0f) < vec2(1.0f);
  }

  {
    let x = 0 <= 1;
    let x1 = 0i <= 1i;
    let x2 = 0u <= 1u;
    let x3 = 0.0 <= 1.0;
    let x4 = 0.0f <= 1.0f;
    let x5 = vec2(0) <= vec2(1);
    let x6 = vec2(0i) <= vec2(1i);
    let x7 = vec2(0u) <= vec2(1u);
    let x8 = vec2(0.0) <= vec2(1.0);
    let x9 = vec2(0.0f) <= vec2(1.0f);
  }
}

fn testBitwise()
{
  {
    let x = ~0;
    let x1 = ~0i;
    let x2 = ~0u;
  }

  {
    let x = 0 & 1;
    let x1 = 0i & 1i;
    let x2 = 0u & 1u;
  }

  {
    let x = 0 | 1;
    let x1 = 0i | 1i;
    let x2 = 0u | 1u;
  }

  {
    let x = 0 ^ 1;
    let x1 = 0i ^ 1i;
    let x2 = 0u ^ 1u;
  }
}