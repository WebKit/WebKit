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

// 17.3. Logical Built-in Functions (https://www.w3.org/TR/WGSL/#logical-builtin-functions)

// 17.3.1
fn testAll()
{
    // [N].(Vector[Bool, N]) => Bool,
    let x1 = all(vec2(false, true));
    let x2 = all(vec3(true, true, true));
    let x3 = all(vec4(false, false, false, false));

    // [N].(Bool) => Bool,
    let x4 = all(true);
    let x5 = all(false);
}

// 17.3.2
fn testAny()
{
    // [N].(Vector[Bool, N]) => Bool,
    let x1 = any(vec2(false, true));
    let x2 = any(vec3(true, true, true));
    let x3 = any(vec4(false, false, false, false));

    // [N].(Bool) => Bool,
    let x4 = any(true);
    let x5 = any(false);
}

// 17.3.3
fn testSelect()
{
    // [T < Scalar].(T, T, Bool) => T,
    {
        let x1 = select(13, 42,   false);
        let x2 = select(13, 42i,  false);
        let x3 = select(13, 42u,  true);
        let x4 = select(13, 42f,  true);
        let x5 = select(13, 42.0, true);
    }

    // [T < Scalar, N].(Vector[T, N], Vector[T, N], Bool) => Vector[T, N],
    {
        let x1 = select(vec2(13), vec2(42),   false);
        let x2 = select(vec2(13), vec2(42i),  false);
        let x3 = select(vec2(13), vec2(42u),  true);
        let x4 = select(vec2(13), vec2(42f),  true);
        let x5 = select(vec2(13), vec2(42.0), true);
    }
    {
        let x1 = select(vec3(13), vec3(42),   false);
        let x2 = select(vec3(13), vec3(42i),  false);
        let x3 = select(vec3(13), vec3(42u),  true);
        let x4 = select(vec3(13), vec3(42f),  true);
        let x5 = select(vec3(13), vec3(42.0), true);
    }
    {
        let x1 = select(vec4(13), vec4(42),   false);
        let x2 = select(vec4(13), vec4(42i),  false);
        let x3 = select(vec4(13), vec4(42u),  true);
        let x4 = select(vec4(13), vec4(42f),  true);
        let x5 = select(vec4(13), vec4(42.0), true);
    }

    // [T < Scalar, N].(Vector[T, N], Vector[T, N], Vector[Bool, N]) => Vector[T, N],
    {
        let x1 = select(vec2(13), vec2(42),   vec2(false));
        let x2 = select(vec2(13), vec2(42i),  vec2(false));
        let x3 = select(vec2(13), vec2(42u),  vec2(true));
        let x4 = select(vec2(13), vec2(42f),  vec2(true));
        let x5 = select(vec2(13), vec2(42.0), vec2(true));
    }
    {
        let x1 = select(vec3(13), vec3(42),   vec3(false));
        let x2 = select(vec3(13), vec3(42i),  vec3(false));
        let x3 = select(vec3(13), vec3(42u),  vec3(true));
        let x4 = select(vec3(13), vec3(42f),  vec3(true));
        let x5 = select(vec3(13), vec3(42.0), vec3(true));
    }
    {
        let x1 = select(vec4(13), vec4(42),   vec4(false));
        let x2 = select(vec4(13), vec4(42i),  vec4(false));
        let x3 = select(vec4(13), vec4(42u),  vec4(true));
        let x4 = select(vec4(13), vec4(42f),  vec4(true));
        let x5 = select(vec4(13), vec4(42.0), vec4(true));
    }
}

// 17.5. Numeric Built-in Functions (https://www.w3.org/TR/WGSL/#numeric-builtin-functions)

// Trigonometric
fn testTrigonometric()
{
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

fn testTrigonometricHyperbolic()
{
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


// 17.5.1
fn testAbs()
{
    // [T < Float].(T) => T,
    {
        let x1 = abs(0);
        let x2 = abs(1i);
        let x3 = abs(1u);
        let x4 = abs(0.0);
        let x5 = abs(1f);
    }

    // [T < Float, N].(Vector[T, N]) => Vector[T, N],
    {
        let x1 = abs(vec2(0, 1));
        let x2 = abs(vec2(1i, 2i));
        let x3 = abs(vec2(1u, 2u));
        let x4 = abs(vec2(0.0, 1.0));
        let x5 = abs(vec2(1f, 2f));
    }
    {
        let x1 = abs(vec3(-1, 0, 1));
        let x2 = abs(vec3(-1i, 1i, 2i));
        let x3 = abs(vec3(0u, 1u, 2u));
        let x4 = abs(vec3(-1.0, 0.0, 1.0));
        let x5 = abs(vec3(-1f, 1f, 2f));
    }
}

// 17.5.2. acos
// 17.5.3. acosh
// 17.5.4. asin
// 17.5.5. asinh
// 17.5.6. atan
// 17.5.7. atanh
// Tested in testTrigonometric and testTrigonometricHyperbolic

// 17.5.8
fn testAtan2() {
    // [T < Float].(T, T) => T,
    {
        let x1 = atan2(0, 1);
        let x2 = atan2(0, 1.0);
        let x3 = atan2(1, 2f);
    }
    // [T < Float, N].(Vector[T, N], Vector[T, N], Vector[T, N]) => Vector[T, N],
    {
        let x1 = atan2(vec2(0), vec2(0));
        let x2 = atan2(vec2(0), vec2(0.0));
        let x3 = atan2(vec2(0), vec2(0f));
    }
    {
        let x1 = atan2(vec3(0), vec3(0));
        let x2 = atan2(vec3(0), vec3(0.0));
        let x3 = atan2(vec3(0), vec3(0f));
    }
    {
        let x1 = atan2(vec4(0), vec4(0));
        let x2 = atan2(vec4(0), vec4(0.0));
        let x3 = atan2(vec4(0), vec4(0f));
    }
}

// 17.5.9
fn testCeil()
{
    // [T < Float].(T) => T,
    {
        let x1 = ceil(0);
        let x2 = ceil(0.0);
        let x3 = ceil(1f);
    }

    // [T < Float, N].(Vector[T, N]) => Vector[T, N],
    {
        let x1 = ceil(vec2(0, 1));
        let x2 = ceil(vec2(0.0, 1.0));
        let x3 = ceil(vec2(1f, 2f));
    }
    {
        let x1 = ceil(vec3(-1, 0, 1));
        let x2 = ceil(vec3(-1.0, 0.0, 1.0));
        let x3 = ceil(vec3(-1f, 1f, 2f));
    }
    {
        let x1 = ceil(vec4(-1, 0, 1, 2));
        let x2 = ceil(vec4(-1.0, 0.0, 1.0, 2.0));
        let x3 = ceil(vec4(-1f, 1f, 2f, 3f));
    }
}

// 17.5.10
fn testClamp()
{
    // [T < Number].(T, T, T) => T,
    {
        let x1 = clamp(-1, 0, 1);
        let x2 = clamp(-1, 1, 2i);
        let x3 = clamp(0, 1, 2u);
        let x4 = clamp(-1, 0, 1.0);
        let x5 = clamp(-1, 1, 2f);
    }
    // [T < Number, N].(Vector[T, N], Vector[T, N], Vector[T, N]) => Vector[T, N],
    {
        let x1 = clamp(vec2(0), vec2(0), vec2(0));
        let x2 = clamp(vec2(0), vec2(0), vec2(0i));
        let x3 = clamp(vec2(0), vec2(0), vec2(0u));
        let x4 = clamp(vec2(0), vec2(0), vec2(0.0));
        let x5 = clamp(vec2(0), vec2(0), vec2(0f));
    }
    {
        let x1 = clamp(vec3(0), vec3(0), vec3(0));
        let x2 = clamp(vec3(0), vec3(0), vec3(0i));
        let x3 = clamp(vec3(0), vec3(0), vec3(0u));
        let x4 = clamp(vec3(0), vec3(0), vec3(0.0));
        let x5 = clamp(vec3(0), vec3(0), vec3(0f));
    }
    {
        let x1 = clamp(vec4(0), vec4(0), vec4(0));
        let x2 = clamp(vec4(0), vec4(0), vec4(0i));
        let x3 = clamp(vec4(0), vec4(0), vec4(0u));
        let x4 = clamp(vec4(0), vec4(0), vec4(0.0));
        let x5 = clamp(vec4(0), vec4(0), vec4(0f));
    }
}

// 17.5.11. cos
// 17.5.12. cosh
// Tested in testTrigonometric and testTrigonometricHyperbolic

// 17.5.13-15 (Bit counting)
fn testBitCounting()
{
    // [T < ConcreteInteger].(T) => T,
    {
        let x1 = countLeadingZeros(1);
        let x2 = countLeadingZeros(1i);
        let x3 = countLeadingZeros(1u);
    }
    {
        let x1 = countOneBits(1);
        let x2 = countOneBits(1i);
        let x3 = countOneBits(1u);
    }
    {
        let x1 = countTrailingZeros(1);
        let x2 = countTrailingZeros(1i);
        let x3 = countTrailingZeros(1u);
    }
    // [T < ConcreteInteger, N].(Vector[T, N]) => Vector[T, N],
    {
        let x1 = countLeadingZeros(vec2(1, 1));
        let x2 = countLeadingZeros(vec2(1i, 1i));
        let x3 = countLeadingZeros(vec2(1u, 1u));
    }
    {
        let x1 = countOneBits(vec3(1, 1, 1));
        let x2 = countOneBits(vec3(1i, 1i, 1i));
        let x3 = countOneBits(vec3(1u, 1u, 1u));
    }
    {
        let x1 = countTrailingZeros(vec4(1, 1, 1, 1));
        let x2 = countTrailingZeros(vec4(1i, 1i, 1i, 1i));
        let x3 = countTrailingZeros(vec4(1u, 1u, 1u, 1u));
    }
}

// 17.5.16
fn testCross()
{
    // [T < Float].(Vector[T, 3], Vector[T, 3]) => Vector[T, 3],
    let x1 = cross(vec3(1, 1, 1), vec3(1f, 2f, 3f));
    let x2 = cross(vec3(1.0, 1.0, 1.0), vec3(1f, 2f, 3f));
    let x3 = cross(vec3(1f, 1f, 1f), vec3(1f, 2f, 3f));
}

// 17.5.17
fn testDegress()
{
    // [T < Float].(T) => T,
    {
        let x1 = degrees(0);
        let x2 = degrees(0.0);
        let x3 = degrees(1f);
    }
    // [T < Float, N].(Vector[T, N]) => Vector[T, N],
    {
        let x1 = degrees(vec2(0, 1));
        let x2 = degrees(vec2(0.0, 1.0));
        let x3 = degrees(vec2(1f, 2f));
    }
    {
        let x1 = degrees(vec3(-1, 0, 1));
        let x2 = degrees(vec3(-1.0, 0.0, 1.0));
        let x3 = degrees(vec3(-1f, 1f, 2f));
    }
    {
        let x1 = degrees(vec4(-1, 0, 1, 2));
        let x2 = degrees(vec4(-1.0, 0.0, 1.0, 2.0));
        let x3 = degrees(vec4(-1f, 1f, 2f, 3f));
    }
}

// 17.5.18
fn testDeterminant()
{
    // [T < Float, C].(Matrix[T, C, C]) => T,
    let x1 = determinant(mat2x2(1, 1, 1, 1));
    let x2 = determinant(mat3x3(1, 1, 1, 1, 1, 1, 1, 1, 1));
    let x3 = determinant(mat4x4(1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1));
}

// 17.5.19
fn testDistance()
{
    // [T < Float].(T, T) => T,
    {
        let x1 = distance(0, 1);
        let x2 = distance(0, 1.0);
        let x3 = distance(0, 1f);
        let x4 = distance(0.0, 1.0);
        let x5 = distance(1.0, 2f);
        let x6 = distance(1f, 2f);
    }
    // [T < Float, N].(Vector[T, N], Vector[T, N]) => T,
    {
        let x1 = distance(vec2(0),   vec2(1)  );
        let x2 = distance(vec2(0),   vec2(0.0));
        let x3 = distance(vec2(0),   vec2(1f) );
        let x4 = distance(vec2(0.0), vec2(0.0));
        let x5 = distance(vec2(0.0), vec2(0f) );
        let x6 = distance(vec2(1f),  vec2(1f) );
    }
    {
        let x1 = distance(vec3(0),   vec3(1)  );
        let x2 = distance(vec3(0),   vec3(0.0));
        let x3 = distance(vec3(0),   vec3(1f) );
        let x4 = distance(vec3(0.0), vec3(0.0));
        let x5 = distance(vec3(0.0), vec3(0f) );
        let x6 = distance(vec3(1f),  vec3(1f) );
    }
    {
        let x1 = distance(vec4(0),   vec4(1)  );
        let x2 = distance(vec4(0),   vec4(0.0));
        let x3 = distance(vec4(0),   vec4(1f) );
        let x4 = distance(vec4(0.0), vec4(0.0));
        let x5 = distance(vec4(0.0), vec4(0f) );
        let x6 = distance(vec4(1f),  vec4(1f) );
    }
}

// 17.5.20
fn testDot()
{
    // [T < Number, N].(Vector[T, N], Vector[T, N]) => T,
    {
        let x1 = dot(vec2(0),   vec2(1)  );
        let x2 = dot(vec2(0),   vec2(1f) );
        let x3 = dot(vec2(0),   vec2(1i) );
        let x4 = dot(vec2(0),   vec2(1u) );
        let x5 = dot(vec2(1i),  vec2(1i) );
        let x6 = dot(vec2(1u),  vec2(1u) );
        let x7 = dot(vec2(0),   vec2(0.0));
        let x8 = dot(vec2(0.0), vec2(0.0));
        let x9 = dot(vec2(0.0), vec2(0f) );
        let x0 = dot(vec2(1f),  vec2(1f) );
    }
    {
        let x1 = dot(vec3(0),   vec3(1)  );
        let x2 = dot(vec3(0),   vec3(1f) );
        let x3 = dot(vec3(0),   vec3(1i) );
        let x4 = dot(vec3(0),   vec3(1u) );
        let x5 = dot(vec3(1i),  vec3(1i) );
        let x6 = dot(vec3(1u),  vec3(1u) );
        let x7 = dot(vec3(0),   vec3(0.0));
        let x8 = dot(vec3(0.0), vec3(0.0));
        let x9 = dot(vec3(0.0), vec3(0f) );
        let x0 = dot(vec3(1f),  vec3(1f) );
    }
    {
        let x1 = dot(vec4(0),   vec4(1)  );
        let x2 = dot(vec4(0),   vec4(1f) );
        let x3 = dot(vec4(0),   vec4(1i) );
        let x4 = dot(vec4(0),   vec4(1u) );
        let x5 = dot(vec4(1i),  vec4(1i) );
        let x6 = dot(vec4(1u),  vec4(1u) );
        let x7 = dot(vec4(0),   vec4(0.0));
        let x8 = dot(vec4(0.0), vec4(0.0));
        let x9 = dot(vec4(0.0), vec4(0f) );
        let x0 = dot(vec4(1f),  vec4(1f) );
    }
}

// 17.5.21 & 17.5.22
fn testExpAndExp2() {
    // [T < Float].(T) => T,
    {
        let x1 = exp(0);
        let x2 = exp(0.0);
        let x3 = exp(1f);
    }
    {
        let x1 = exp2(0);
        let x2 = exp2(0.0);
        let x3 = exp2(1f);
    }

    // [T < Float, N].(Vector[T, N]) => Vector[T, N],
    {
        let x1 = exp(vec2(0, 1));
        let x2 = exp(vec2(0.0, 1.0));
        let x3 = exp(vec2(1f, 2f));
    }
    {
        let x1 = exp(vec3(-1, 0, 1));
        let x2 = exp(vec3(-1.0, 0.0, 1.0));
        let x3 = exp(vec3(-1f, 1f, 2f));
    }
    {
        let x1 = exp(vec4(-1, 0, 1, 2));
        let x2 = exp(vec4(-1.0, 0.0, 1.0, 2.0));
        let x3 = exp(vec4(-1f, 1f, 2f, 3f));
    }

    {
        let x1 = exp2(vec2(0, 1));
        let x2 = exp2(vec2(0.0, 1.0));
        let x3 = exp2(vec2(1f, 2f));
    }
    {
        let x1 = exp2(vec3(-1, 0, 1));
        let x2 = exp2(vec3(-1.0, 0.0, 1.0));
        let x3 = exp2(vec3(-1f, 1f, 2f));
    }
    {
        let x1 = exp2(vec4(-1, 0, 1, 2));
        let x2 = exp2(vec4(-1.0, 0.0, 1.0, 2.0));
        let x3 = exp2(vec4(-1f, 1f, 2f, 3f));
    }
}

// 17.5.23 & 17.5.24
fn testExtractBits()
{
    // signed
    // [].(I32, U32, U32) => I32,
    {
        let x1 = extractBits(0, 1, 1);
        let x2 = extractBits(0i, 1, 1);
        let x3 = extractBits(0i, 1u, 1u);
    }
    // [N].(Vector[I32, N], U32, U32) => Vector[I32, N],
    {
        let x1 = extractBits(vec2(0), 1, 1);
        let x2 = extractBits(vec2(0i), 1, 1);
        let x3 = extractBits(vec2(0i), 1u, 1u);
    }
    {
        let x1 = extractBits(vec3(0), 1, 1);
        let x2 = extractBits(vec3(0i), 1, 1);
        let x3 = extractBits(vec3(0i), 1u, 1u);
    }
    {
        let x1 = extractBits(vec4(0), 1, 1);
        let x2 = extractBits(vec4(0i), 1, 1);
        let x3 = extractBits(vec4(0i), 1u, 1u);
    }

    // unsigned
    // [].(U32, U32, U32) => U32,
    {
        let x1 = extractBits(0, 1, 1);
        let x2 = extractBits(0u, 1, 1);
        let x3 = extractBits(0u, 1u, 1u);
    }

    // [N].(Vector[U32, N], U32, U32) => Vector[U32, N],
    {
        let x1 = extractBits(vec2(0), 1, 1);
        let x2 = extractBits(vec2(0u), 1, 1);
        let x3 = extractBits(vec2(0u), 1u, 1u);
    }
    {
        let x1 = extractBits(vec3(0), 1, 1);
        let x2 = extractBits(vec3(0u), 1, 1);
        let x3 = extractBits(vec3(0u), 1u, 1u);
    }
    {
        let x1 = extractBits(vec4(0), 1, 1);
        let x2 = extractBits(vec4(0u), 1, 1);
        let x3 = extractBits(vec4(0u), 1u, 1u);
    }
}

// 17.5.25
fn testFaceForward()
{
    // [T < Float, N].(Vector[T, N], Vector[T, N], Vector[T, N]) => Vector[T, N],
    {
        let x1 = faceForward(vec2(0), vec2(1),   vec2(2));
        let x2 = faceForward(vec2(0), vec2(1),   vec2(2.0));
        let x3 = faceForward(vec2(0), vec2(1.0), vec2(2f));
    }
    {
        let x1 = faceForward(vec3(0), vec3(1),   vec3(2));
        let x2 = faceForward(vec3(0), vec3(1),   vec3(2.0));
        let x3 = faceForward(vec3(0), vec3(1.0), vec3(2f));
    }
    {
        let x1 = faceForward(vec4(0), vec4(1),   vec4(2));
        let x2 = faceForward(vec4(0), vec4(1),   vec4(2.0));
        let x3 = faceForward(vec4(0), vec4(1.0), vec4(2f));
    }
}

// 17.5.26 & 17.5.27
fn testFirstLeadingBit()
{
    // signed
    // [].(I32) => I32,
    {
        let x1 = firstLeadingBit(0);
        let x2 = firstLeadingBit(0i);
    }
    // [N].(Vector[I32, N]) => Vector[I32, N],
    {
        let x1 = firstLeadingBit(vec2(0));
        let x2 = firstLeadingBit(vec2(0i));
    }
    {
        let x1 = firstLeadingBit(vec3(0));
        let x2 = firstLeadingBit(vec3(0i));
    }
    {
        let x1 = firstLeadingBit(vec4(0));
        let x2 = firstLeadingBit(vec4(0i));
    }

    // unsigned
    // [].(U32) => U32,
    {
        let x1 = firstLeadingBit(0);
        let x2 = firstLeadingBit(0u);
    }

    // [N].(Vector[U32, N]) => Vector[U32, N],
    {
        let x1 = firstLeadingBit(vec2(0));
        let x2 = firstLeadingBit(vec2(0u));
    }
    {
        let x1 = firstLeadingBit(vec3(0));
        let x2 = firstLeadingBit(vec3(0u));
    }
    {
        let x1 = firstLeadingBit(vec4(0));
        let x2 = firstLeadingBit(vec4(0u));
    }
}

// 17.5.28
fn testFirstTrailingBit()
{
    // [T < ConcreteInteger].(T) => T,
    {
        let x1 = firstTrailingBit(0);
        let x2 = firstTrailingBit(0i);
        let x3 = firstTrailingBit(0u);
    }

    // [T < ConcreteInteger, N].(Vector[T, N]) => Vector[T, N],
    {
        let x1 = firstTrailingBit(vec2(0));
        let x2 = firstTrailingBit(vec2(0i));
        let x3 = firstTrailingBit(vec2(0u));
    }
    {
        let x1 = firstTrailingBit(vec3(0));
        let x2 = firstTrailingBit(vec3(0i));
        let x3 = firstTrailingBit(vec3(0u));
    }
    {
        let x1 = firstTrailingBit(vec4(0));
        let x2 = firstTrailingBit(vec4(0i));
        let x3 = firstTrailingBit(vec4(0u));
    }
}

// 17.5.29
fn testFloor()
{
    // [T < Float].(T) => T,
    {
        let x1 = floor(0);
        let x2 = floor(0.0);
        let x3 = floor(1f);
    }

    // [T < Float, N].(Vector[T, N]) => Vector[T, N],
    {
        let x1 = floor(vec2(0, 1));
        let x2 = floor(vec2(0.0, 1.0));
        let x3 = floor(vec2(1f, 2f));
    }
    {
        let x1 = floor(vec3(-1, 0, 1));
        let x2 = floor(vec3(-1.0, 0.0, 1.0));
        let x3 = floor(vec3(-1f, 1f, 2f));
    }
    {
        let x1 = floor(vec4(-1, 0, 1, 2));
        let x2 = floor(vec4(-1.0, 0.0, 1.0, 2.0));
        let x3 = floor(vec4(-1f, 1f, 2f, 3f));
    }
}

// 17.5.30
fn testFma()
{
    // [T < Float].(T, T, T) => T,
    {
        let x1 = fma(-1, 0, 1);
        let x4 = fma(-1, 0, 1.0);
        let x5 = fma(-1, 1, 2f);
    }
    // [T < Float, N].(Vector[T, N], Vector[T, N], Vector[T, N]) => Vector[T, N],
    {
        let x1 = fma(vec2(0), vec2(0), vec2(0));
        let x2 = fma(vec2(0), vec2(0), vec2(0.0));
        let x3 = fma(vec2(0), vec2(0), vec2(0f));
    }
    {
        let x1 = fma(vec3(0), vec3(0), vec3(0));
        let x2 = fma(vec3(0), vec3(0), vec3(0.0));
        let x3 = fma(vec3(0), vec3(0), vec3(0f));
    }
    {
        let x1 = fma(vec4(0), vec4(0), vec4(0));
        let x2 = fma(vec4(0), vec4(0), vec4(0.0));
        let x3 = fma(vec4(0), vec4(0), vec4(0f));
    }
}

// 17.5.31
fn testFract()
{
    // [T < Float].(T) => T,
    {
        let x1 = fract(0);
        let x2 = fract(0.0);
        let x3 = fract(1f);
    }

    // [T < Float, N].(Vector[T, N]) => Vector[T, N],
    {
        let x1 = fract(vec2(0));
        let x2 = fract(vec2(0.0));
        let x3 = fract(vec2(1f));
    }
    {
        let x1 = fract(vec3(-1));
        let x2 = fract(vec3(-1.0));
        let x3 = fract(vec3(-1f));
    }
    {
        let x1 = fract(vec4(-1));
        let x2 = fract(vec4(-1.0));
        let x3 = fract(vec4(-1f));
    }
}

// 17.5.32
fn testFrexp()
{
    // FIXME: this needs the special return types __frexp_result_*
}

// 17.5.33
fn testInsertBits()
{
    // [T < ConcreteInteger].(T, T, U32, U32) => T,
    {
        let x1 = insertBits(0, 0, 0, 0);
        let x2 = insertBits(0, 0i, 0, 0);
        // FIXME: fails to resolve overload resolution
        // let x3 = insertBits(0, 0u, 0, 0);
    }

    // [T < ConcreteInteger, N].(Vector[T, N], Vector[T, N], U32, U32) => Vector[T, N],
    {
        let x1 = insertBits(vec2(0), vec2(0), 0, 0);
        let x2 = insertBits(vec2(0), vec2(0i), 0, 0);
        // FIXME: fails to resolve overload resolution
        // let x3 = insertBits(vec2(0), vec2(0u), 0, 0);
    }
    {
        let x1 = insertBits(vec3(0), vec3(0), 0, 0);
        let x2 = insertBits(vec3(0), vec3(0i), 0, 0);
        // FIXME: fails to resolve overload resolution
        // let x3 = insertBits(vec3(0), vec3(0u), 0, 0);
    }
    {
        let x1 = insertBits(vec4(0), vec4(0), 0, 0);
        let x2 = insertBits(vec4(0), vec4(0i), 0, 0);
        // FIXME: fails to resolve overload resolution
        // let x3 = insertBits(vec4(0), vec4(0u), 0, 0);
    }
}

// 17.5.34
fn testInverseSqrt()
{
    // [T < Float].(T) => T,
    {
        let x1 = inverseSqrt(0);
        let x2 = inverseSqrt(0.0);
        let x3 = inverseSqrt(1f);
    }

    // [T < Float, N].(Vector[T, N]) => Vector[T, N],
    {
        let x1 = inverseSqrt(vec2(0));
        let x2 = inverseSqrt(vec2(0.0));
        let x3 = inverseSqrt(vec2(1f));
    }
    {
        let x1 = inverseSqrt(vec3(-1));
        let x2 = inverseSqrt(vec3(-1.0));
        let x3 = inverseSqrt(vec3(-1f));
    }
    {
        let x1 = inverseSqrt(vec4(-1));
        let x2 = inverseSqrt(vec4(-1.0));
        let x3 = inverseSqrt(vec4(-1f));
    }
}

// 17.5.35
fn testLdexp()
{
    // [T < ConcreteFloat].(T, I32) => T,
    {
        let x1 = ldexp(0f, 1);
    }
    // [].(AbstractFloat, AbstractInt) => AbstractFloat,
    {
        let x1 = ldexp(0, 1);
        let x2 = ldexp(0.0, 1);
    }
    // [T < ConcreteFloat, N].(Vector[T, N], Vector[I32, N]) => Vector[T, N],
    {
        let x1 = ldexp(vec2(0f), vec2(1));
        let x2 = ldexp(vec3(0f), vec3(1));
        let x3 = ldexp(vec4(0f), vec4(1));
    }
    // [N].(Vector[AbstractFloat, N], Vector[AbstractInt, N]) => Vector[AbstractFloat, N],
    {
        let x1 = ldexp(vec2(0), vec2(1));
        let x2 = ldexp(vec3(0), vec3(1));
        let x3 = ldexp(vec4(0), vec4(1));

        let x4 = ldexp(vec2(0.0), vec2(1));
        let x5 = ldexp(vec3(0.0), vec3(1));
        let x6 = ldexp(vec4(0.0), vec4(1));
    }
}

// 17.5.36
fn testLength()
{
    // [T < Float].(T) => T,
    {
        let x1 = length(0);
        let x2 = length(0.0);
        let x3 = length(1f);
    }

    // [T < Float, N].(Vector[T, N]) => Vector[T, N],
    {
        let x1 = length(vec2(0));
        let x2 = length(vec2(0.0));
        let x3 = length(vec2(1f));
    }
    {
        let x1 = length(vec3(-1));
        let x2 = length(vec3(-1.0));
        let x3 = length(vec3(-1f));
    }
    {
        let x1 = length(vec4(-1));
        let x2 = length(vec4(-1.0));
        let x3 = length(vec4(-1f));
    }
}

// 17.5.37
fn testLog()
{
    // [T < Float].(T) => T,
    {
        let x1 = log(0);
        let x2 = log(0.0);
        let x3 = log(1f);
    }

    // [T < Float, N].(Vector[T, N]) => Vector[T, N],
    {
        let x1 = log(vec2(0));
        let x2 = log(vec2(0.0));
        let x3 = log(vec2(1f));
    }
    {
        let x1 = log(vec3(-1));
        let x2 = log(vec3(-1.0));
        let x3 = log(vec3(-1f));
    }
    {
        let x1 = log(vec4(-1));
        let x2 = log(vec4(-1.0));
        let x3 = log(vec4(-1f));
    }
}

// 17.5.38
fn testLog2() {
    // [T < Float].(T) => T,
    {
        let x1 = log2(0);
        let x2 = log2(0.0);
        let x3 = log2(1f);
    }

    // [T < Float, N].(Vector[T, N]) => Vector[T, N],
    {
        let x1 = log2(vec2(0));
        let x2 = log2(vec2(0.0));
        let x3 = log2(vec2(1f));
    }
    {
        let x1 = log2(vec3(-1));
        let x2 = log2(vec3(-1.0));
        let x3 = log2(vec3(-1f));
    }
    {
        let x1 = log2(vec4(-1));
        let x2 = log2(vec4(-1.0));
        let x3 = log2(vec4(-1f));
    }
}

// 17.5.39
fn testMax()
{
    // [T < Number].(T, T) => T,
    {
        let x1 = max(-1, 0);
        let x2 = max(-1, 1);
        let x3 = max(0, 1);
        let x4 = max(-1, 0);
        let x5 = max(-1, 1);
    }
    // [T < Number, N].(Vector[T, N], Vector[T, N]) => Vector[T, N],
    {
        let x1 = max(vec2(0), vec2(0));
        let x2 = max(vec2(0), vec2(0i));
        let x3 = max(vec2(0), vec2(0u));
        let x4 = max(vec2(0), vec2(0.0));
        let x5 = max(vec2(0), vec2(0f));
    }
    {
        let x1 = max(vec3(0), vec3(0));
        let x2 = max(vec3(0), vec3(0i));
        let x3 = max(vec3(0), vec3(0u));
        let x4 = max(vec3(0), vec3(0.0));
        let x5 = max(vec3(0), vec3(0f));
    }
    {
        let x1 = max(vec4(0), vec4(0));
        let x2 = max(vec4(0), vec4(0i));
        let x3 = max(vec4(0), vec4(0u));
        let x4 = max(vec4(0), vec4(0.0));
        let x5 = max(vec4(0), vec4(0f));
    }
}

// 17.5.40
fn testMin()
{
    // [T < Number].(T, T) => T,
    {
        let x1 = min(-1, 0);
        let x2 = min(-1, 1);
        let x3 = min(0, 1);
        let x4 = min(-1, 0);
        let x5 = min(-1, 1);
    }
    // [T < Number, N].(Vector[T, N], Vector[T, N]) => Vector[T, N],
    {
        let x1 = min(vec2(0), vec2(0));
        let x2 = min(vec2(0), vec2(0i));
        let x3 = min(vec2(0), vec2(0u));
        let x4 = min(vec2(0), vec2(0.0));
        let x5 = min(vec2(0), vec2(0f));
    }
    {
        let x1 = min(vec3(0), vec3(0));
        let x2 = min(vec3(0), vec3(0i));
        let x3 = min(vec3(0), vec3(0u));
        let x4 = min(vec3(0), vec3(0.0));
        let x5 = min(vec3(0), vec3(0f));
    }
    {
        let x1 = min(vec4(0), vec4(0));
        let x2 = min(vec4(0), vec4(0i));
        let x3 = min(vec4(0), vec4(0u));
        let x4 = min(vec4(0), vec4(0.0));
        let x5 = min(vec4(0), vec4(0f));
    }
}

// 17.5.41
fn testMix()
{
    // [T < Float].(T, T, T) => T,
    {
        let x1 = mix(-1, 0, 1);
        let x2 = mix(-1, 0, 1.0);
        let x3 = mix(-1, 1, 2f);
    }
    // [T < Float, N].(Vector[T, N], Vector[T, N], Vector[T, N]) => Vector[T, N],
    {
        let x1 = mix(vec2(0), vec2(0), vec2(0));
        let x2 = mix(vec2(0), vec2(0), vec2(0.0));
        let x3 = mix(vec2(0), vec2(0), vec2(0f));
    }
    {
        let x1 = mix(vec3(0), vec3(0), vec3(0));
        let x2 = mix(vec3(0), vec3(0), vec3(0.0));
        let x3 = mix(vec3(0), vec3(0), vec3(0f));
    }
    {
        let x1 = mix(vec4(0), vec4(0), vec4(0));
        let x2 = mix(vec4(0), vec4(0), vec4(0.0));
        let x3 = mix(vec4(0), vec4(0), vec4(0f));
    }
    // [T < Float, N].(Vector[T, N], Vector[T, N], T) => Vector[T, N],
    {
        let x1 = mix(vec2(0), vec2(0), 0);
        let x2 = mix(vec2(0), vec2(0), 0.0);
        let x3 = mix(vec2(0), vec2(0), 0f);
    }
    {
        let x1 = mix(vec3(0), vec3(0), 0);
        let x2 = mix(vec3(0), vec3(0), 0.0);
        let x3 = mix(vec3(0), vec3(0), 0f);
    }
    {
        let x1 = mix(vec4(0), vec4(0), 0);
        let x2 = mix(vec4(0), vec4(0), 0.0);
        let x3 = mix(vec4(0), vec4(0), 0f);
    }
}

// 17.5.42
fn testModf()
{
    // FIXME: this needs the special return types __modf_result_*
}

// 17.5.43
fn testNormalize()
{
    // [T < Float, N].(Vector[T, N]) => Vector[T, N],
    {
        let x1 = normalize(vec2(0));
        let x2 = normalize(vec2(0.0));
        let x3 = normalize(vec2(1f));
    }
    {
        let x1 = normalize(vec3(-1));
        let x2 = normalize(vec3(-1.0));
        let x3 = normalize(vec3(-1f));
    }
    {
        let x1 = normalize(vec4(-1));
        let x2 = normalize(vec4(-1.0));
        let x3 = normalize(vec4(-1f));
    }
}

// 17.5.44
fn testPow()
{
    // [T < Float].(T, T) => T,
    {
        let x1 = pow(0, 1);
        let x2 = pow(0, 1.0);
        let x3 = pow(0, 1f);
        let x4 = pow(0.0, 1.0);
        let x5 = pow(1.0, 2f);
        let x6 = pow(1f, 2f);
    }
    // [T < Float, N].(Vector[T, N], Vector[T, N]) => Vector[T, N],
    {
        let x1 = pow(vec2(0),   vec2(1)  );
        let x2 = pow(vec2(0),   vec2(0.0));
        let x3 = pow(vec2(0),   vec2(1f) );
        let x4 = pow(vec2(0.0), vec2(0.0));
        let x5 = pow(vec2(0.0), vec2(0f) );
        let x6 = pow(vec2(1f),  vec2(1f) );
    }
    {
        let x1 = pow(vec3(0),   vec3(1)  );
        let x2 = pow(vec3(0),   vec3(0.0));
        let x3 = pow(vec3(0),   vec3(1f) );
        let x4 = pow(vec3(0.0), vec3(0.0));
        let x5 = pow(vec3(0.0), vec3(0f) );
        let x6 = pow(vec3(1f),  vec3(1f) );
    }
    {
        let x1 = pow(vec4(0),   vec4(1)  );
        let x2 = pow(vec4(0),   vec4(0.0));
        let x3 = pow(vec4(0),   vec4(1f) );
        let x4 = pow(vec4(0.0), vec4(0.0));
        let x5 = pow(vec4(0.0), vec4(0f) );
        let x6 = pow(vec4(1f),  vec4(1f) );
    }
}

// 17.5.45
fn testQuantizeToF16() {
    // [].(F32) => F32,
    {
        let x1 = quantizeToF16(0);
        let x2 = quantizeToF16(0.0);
        let x3 = quantizeToF16(0f);
    }

    // [N].(Vector[F32, N]) => Vector[F32, N],
    {
        let x1 = quantizeToF16(vec2(0));
        let x2 = quantizeToF16(vec2(0.0));
        let x3 = quantizeToF16(vec2(0f));
    }
    {
        let x1 = quantizeToF16(vec3(0));
        let x2 = quantizeToF16(vec3(0.0));
        let x3 = quantizeToF16(vec3(0f));
    }
    {
        let x1 = quantizeToF16(vec4(0));
        let x2 = quantizeToF16(vec4(0.0));
        let x3 = quantizeToF16(vec4(0f));
    }
}

// 17.5.46
fn testRadians()
{
    // [T < Float].(T) => T,
    {
        let x1 = radians(0);
        let x2 = radians(0.0);
        let x3 = radians(1f);
    }

    // [T < Float, N].(Vector[T, N]) => Vector[T, N],
    {
        let x1 = radians(vec2(0));
        let x2 = radians(vec2(0.0));
        let x3 = radians(vec2(1f));
    }
    {
        let x1 = radians(vec3(-1));
        let x2 = radians(vec3(-1.0));
        let x3 = radians(vec3(-1f));
    }
    {
        let x1 = radians(vec4(-1));
        let x2 = radians(vec4(-1.0));
        let x3 = radians(vec4(-1f));
    }
}

// 17.5.47
fn testReflect()
{
    // [T < Float, N].(Vector[T, N], Vector[T, N]) => Vector[T, N],
    {
        let x1 = reflect(vec2(0),   vec2(1)  );
        let x2 = reflect(vec2(0),   vec2(0.0));
        let x3 = reflect(vec2(0),   vec2(1f) );
        let x4 = reflect(vec2(0.0), vec2(0.0));
        let x5 = reflect(vec2(0.0), vec2(0f) );
        let x6 = reflect(vec2(1f),  vec2(1f) );
    }
    {
        let x1 = reflect(vec3(0),   vec3(1)  );
        let x2 = reflect(vec3(0),   vec3(0.0));
        let x3 = reflect(vec3(0),   vec3(1f) );
        let x4 = reflect(vec3(0.0), vec3(0.0));
        let x5 = reflect(vec3(0.0), vec3(0f) );
        let x6 = reflect(vec3(1f),  vec3(1f) );
    }
    {
        let x1 = reflect(vec4(0),   vec4(1)  );
        let x2 = reflect(vec4(0),   vec4(0.0));
        let x3 = reflect(vec4(0),   vec4(1f) );
        let x4 = reflect(vec4(0.0), vec4(0.0));
        let x5 = reflect(vec4(0.0), vec4(0f) );
        let x6 = reflect(vec4(1f),  vec4(1f) );
    }
}

// 17.5.48
fn testRefract()
{
    // [T < Float, N].(Vector[T, N], Vector[T, N], T) => Vector[T, N],
    {
        let x1 = refract(vec2(0), vec2(0), 1);
        let x2 = refract(vec2(0), vec2(0), 0.0);
        let x3 = refract(vec2(0), vec2(0), 1f);
    }
    {
        let x1 = refract(vec3(0), vec3(0), 1);
        let x2 = refract(vec3(0), vec3(0), 0.0);
        let x3 = refract(vec3(0), vec3(0), 1f);
    }
    {
        let x1 = refract(vec4(0), vec4(0), 1);
        let x2 = refract(vec4(0), vec4(0), 0.0);
        let x3 = refract(vec4(0), vec4(0), 1f);
    }
}

// 17.5.49
fn testReverseBits()
{
    // [T < ConcreteInteger].(T) => T,
    {
        let x1 = reverseBits(0);
        let x2 = reverseBits(0i);
        let x3 = reverseBits(0u);
    }
    // [T < ConcreteInteger, N].(Vector[T, N]) => Vector[T, N],
    {
        let x1 = reverseBits(vec2(0));
        let x2 = reverseBits(vec2(0i));
        let x3 = reverseBits(vec2(0u));
    }
    {
        let x1 = reverseBits(vec3(0));
        let x2 = reverseBits(vec3(0i));
        let x3 = reverseBits(vec3(0u));
    }
    {
        let x1 = reverseBits(vec4(0));
        let x2 = reverseBits(vec4(0i));
        let x3 = reverseBits(vec4(0u));
    }
}

// 17.5.50
fn testRound()
{
    // [T < Float].(T) => T,
    {
        let x1 = round(0);
        let x2 = round(0.0);
        let x3 = round(1f);
    }

    // [T < Float, N].(Vector[T, N]) => Vector[T, N],
    {
        let x1 = round(vec2(0));
        let x2 = round(vec2(0.0));
        let x3 = round(vec2(1f));
    }
    {
        let x1 = round(vec3(-1));
        let x2 = round(vec3(-1.0));
        let x3 = round(vec3(-1f));
    }
    {
        let x1 = round(vec4(-1));
        let x2 = round(vec4(-1.0));
        let x3 = round(vec4(-1f));
    }
}

// 17.5.51
fn testSaturate()
{
    // [T < Float].(T) => T,
    {
        let x1 = saturate(0);
        let x2 = saturate(0.0);
        let x3 = saturate(1f);
    }

    // [T < Float, N].(Vector[T, N]) => Vector[T, N],
    {
        let x1 = saturate(vec2(0));
        let x2 = saturate(vec2(0.0));
        let x3 = saturate(vec2(1f));
    }
    {
        let x1 = saturate(vec3(-1));
        let x2 = saturate(vec3(-1.0));
        let x3 = saturate(vec3(-1f));
    }
    {
        let x1 = saturate(vec4(-1));
        let x2 = saturate(vec4(-1.0));
        let x3 = saturate(vec4(-1f));
    }
}

// 17.5.52
fn testSign()
{
    // [T < SignedNumber].(T) => T,
    {
        let x1 = sign(0);
        let x2 = sign(0i);
        let x3 = sign(0.0);
        let x4 = sign(1f);
    }

    // [T < SignedNumber, N].(Vector[T, N]) => Vector[T, N],
    {
        let x1 = sign(vec2(0));
        let x2 = sign(vec2(0i));
        let x3 = sign(vec2(0.0));
        let x4 = sign(vec2(1f));
    }
    {
        let x1 = sign(vec3(-1));
        let x2 = sign(vec3(-1i));
        let x3 = sign(vec3(-1.0));
        let x4 = sign(vec3(-1f));
    }
    {
        let x1 = sign(vec4(-1));
        let x2 = sign(vec4(-1i));
        let x3 = sign(vec4(-1.0));
        let x4 = sign(vec4(-1f));
    }
}

// 17.5.53. sin
// 17.5.54. sinh
// Tested in testTrigonometric and testTrigonometricHyperbolic

// 17.5.55
fn testSmoothstep()
{
    // [T < Float].(T, T, T) => T,
    {
        let x1 = smoothstep(-1, 0, 1);
        let x2 = smoothstep(-1, 0, 1.0);
        let x3 = smoothstep(-1, 1, 2f);
    }
    // [T < Float, N].(Vector[T, N], Vector[T, N], Vector[T, N]) => Vector[T, N],
    {
        let x1 = smoothstep(vec2(0), vec2(0), vec2(0));
        let x2 = smoothstep(vec2(0), vec2(0), vec2(0.0));
        let x3 = smoothstep(vec2(0), vec2(0), vec2(0f));
    }
    {
        let x1 = smoothstep(vec3(0), vec3(0), vec3(0));
        let x2 = smoothstep(vec3(0), vec3(0), vec3(0.0));
        let x3 = smoothstep(vec3(0), vec3(0), vec3(0f));
    }
    {
        let x1 = smoothstep(vec4(0), vec4(0), vec4(0));
        let x2 = smoothstep(vec4(0), vec4(0), vec4(0.0));
        let x3 = smoothstep(vec4(0), vec4(0), vec4(0f));
    }
}

// 17.5.56
fn testSqrt()
{
    // [T < Float].(T) => T,
    {
        let x1 = sqrt(0);
        let x2 = sqrt(0.0);
        let x3 = sqrt(1f);
    }

    // [T < Float, N].(Vector[T, N]) => Vector[T, N],
    {
        let x1 = sqrt(vec2(0));
        let x2 = sqrt(vec2(0.0));
        let x3 = sqrt(vec2(1f));
    }
    {
        let x1 = sqrt(vec3(-1));
        let x2 = sqrt(vec3(-1.0));
        let x3 = sqrt(vec3(-1f));
    }
    {
        let x1 = sqrt(vec4(-1));
        let x2 = sqrt(vec4(-1.0));
        let x3 = sqrt(vec4(-1f));
    }
}

// 17.5.57
fn testStep()
{
    // [T < Float].(T, T) => T,
    {
        let x1 = step(0, 1);
        let x2 = step(0, 1.0);
        let x3 = step(1, 2f);
    }
    // [T < Float, N].(Vector[T, N], Vector[T, N], Vector[T, N]) => Vector[T, N],
    {
        let x1 = step(vec2(0), vec2(0));
        let x2 = step(vec2(0), vec2(0.0));
        let x3 = step(vec2(0), vec2(0f));
    }
    {
        let x1 = step(vec3(0), vec3(0));
        let x2 = step(vec3(0), vec3(0.0));
        let x3 = step(vec3(0), vec3(0f));
    }
    {
        let x1 = step(vec4(0), vec4(0));
        let x2 = step(vec4(0), vec4(0.0));
        let x3 = step(vec4(0), vec4(0f));
    }
}

// 17.5.58. tan
// 17.5.59. tanh
// Tested in testTrigonometric and testTrigonometricHyperbolic

// 17.5.60
fn testTranspose()
{
    // [T < Float, C, R].(Matrix[T, C, R]) => Matrix[T, R, C],
    {
        const x = 1;
        let x1 = transpose(mat2x2(0, 0, 0, x));
        let x2 = transpose(mat2x3(0, 0, 0, 0, 0, x));
        let x3 = transpose(mat2x4(0, 0, 0, 0, 0, 0, 0, x));
        let x4 = transpose(mat3x2(0, 0, 0, 0, 0, x));
        let x5 = transpose(mat3x3(0, 0, 0, 0, 0, 0, 0, 0, x));
        let x6 = transpose(mat3x4(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, x));
        let x7 = transpose(mat4x2(0, 0, 0, 0, 0, 0, 0, x));
        let x8 = transpose(mat4x3(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, x));
        let x9 = transpose(mat4x4(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, x));
    }
    {
        const x = 1.0;
        let x1 = transpose(mat2x2(0, 0, 0, x));
        let x2 = transpose(mat2x3(0, 0, 0, 0, 0, x));
        let x3 = transpose(mat2x4(0, 0, 0, 0, 0, 0, 0, x));
        let x4 = transpose(mat3x2(0, 0, 0, 0, 0, x));
        let x5 = transpose(mat3x3(0, 0, 0, 0, 0, 0, 0, 0, x));
        let x6 = transpose(mat3x4(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, x));
        let x7 = transpose(mat4x2(0, 0, 0, 0, 0, 0, 0, x));
        let x8 = transpose(mat4x3(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, x));
        let x9 = transpose(mat4x4(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, x));
    }
    {
        const x = 1f;
        let x1 = transpose(mat2x2(0, 0, 0, x));
        let x2 = transpose(mat2x3(0, 0, 0, 0, 0, x));
        let x3 = transpose(mat2x4(0, 0, 0, 0, 0, 0, 0, x));
        let x4 = transpose(mat3x2(0, 0, 0, 0, 0, x));
        let x5 = transpose(mat3x3(0, 0, 0, 0, 0, 0, 0, 0, x));
        let x6 = transpose(mat3x4(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, x));
        let x7 = transpose(mat4x2(0, 0, 0, 0, 0, 0, 0, x));
        let x8 = transpose(mat4x3(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, x));
        let x9 = transpose(mat4x4(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, x));
    }
}

// 17.5.61
fn testTrunc()
{
    // [T < Float].(T) => T,
    {
        let x1 = trunc(0);
        let x2 = trunc(0.0);
        let x3 = trunc(1f);
    }

    // [T < Float, N].(Vector[T, N]) => Vector[T, N],
    {
        let x1 = trunc(vec2(0));
        let x2 = trunc(vec2(0.0));
        let x3 = trunc(vec2(1f));
    }
    {
        let x1 = trunc(vec3(-1));
        let x2 = trunc(vec3(-1.0));
        let x3 = trunc(vec3(-1f));
    }
    {
        let x1 = trunc(vec4(-1));
        let x2 = trunc(vec4(-1.0));
        let x3 = trunc(vec4(-1f));
    }
}
