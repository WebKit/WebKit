// RUN: %wgslc

fn testAdd() {
  {
    _ = 1 + 2;
    _ = 1i + 1;
    _ = 1 + 1i;
    _ = 1u + 1;
    _ = 1 + 1u;
    _ = 1.0 + 2;
    _ = 1 + 2.0;
    _ = 1.0 + 2.0;
  }


  {
    _ = vec2<f32>(0, 0) + 1;
    _ = vec3<f32>(0, 0, 0) + 1;
    _ = vec4<f32>(0, 0, 0, 0) + 1;
  }

  {
    _ = 1 + vec2<f32>(0, 0);
    _ = 1 + vec3<f32>(0, 0, 0);
    _ = 1 + vec4<f32>(0, 0, 0, 0);
  }

  {
    _ = vec2<f32>(0, 0) + vec2<f32>(1, 1);
    _ = vec3<f32>(0, 0, 0) + vec3<f32>(1, 1, 1);
    _ = vec4<f32>(0, 0, 0, 0) + vec4<f32>(1, 1, 1, 1);
  }

  {
    _ = mat2x2<f32>(0, 0, 0, 0) + mat2x2<f32>(1, 1, 1, 1);
    _ = mat2x3<f32>(0, 0, 0, 0, 0, 0) + mat2x3<f32>(1, 1, 1, 1, 1, 1);
    _ = mat2x4<f32>(0, 0, 0, 0, 0, 0, 0, 0) + mat2x4<f32>(1, 1, 1, 1, 1, 1, 1, 1);
  }
}

fn testAddEq() {
  {
    var x = 0;
    x += 1;
  }
}

fn testMultiply() {
  {
    _ = 0 * 0;
    _ = 0i * 0i;
    _ = 0u * 0u;
    _ = 0.0 * 0.0;
    _ = 0.0f * 0.0f;
  }

  let v2 = vec2<f32>(0, 0);
  let v4 = vec4<f32>(0, 0, 0, 0);
  let m = mat2x4<f32>(0, 0, 0, 0, 0, 0, 0, 0);
  _ = m * v2;
  _ = v4 * m;
  _ = vec2(1, 1) * 1;
  _ = 1 * vec2(1, 1);
  _ = vec2(1, 1) * vec2(1, 1);

  _ = mat2x2(0, 0, 0, 0) * mat2x2(0, 0, 0, 0);
  _ = mat2x2(0, 0, 0, 0) * mat3x2(0, 0, 0, 0, 0, 0);
  _ = mat2x2(0, 0, 0, 0) * mat4x2(0, 0, 0, 0, 0, 0, 0, 0);
}

fn testDivision() {
   _ = 0 / 1;
   _ = 0i / 1i;
   _ = 0u / 1u;
   _ = 0.0 / 1.0;
   _ = 0.0f / 1.0f;
   _ = vec2(0.0) / 1.0;
   _ = 0.0 / vec2(1.0, 1.0);
   _ = vec2(0.0, 0.0) / vec2(1.0, 1.0);
}

fn testModulo() {
   _ = 0 % 1;
   _ = 0i % 1i;
   _ = 0u % 1u;
   _ = 0.0 % 1.0;
   _ = 0.0f % 1.0f;
   _ = vec2(0.0) % 1.0;
   _ = 0.0 % vec2(1.0, 1.0);
   _ = vec2(0.0, 0.0) % vec2(1.0, 1.0);
}

fn testTextureSample() {
  {
    let t: texture_1d<f32>;
    let s: sampler;
    _ = textureSample(t, s, 1);
  }

  {
    let t: texture_2d<f32>;
    let s: sampler;
    _ = textureSample(t, s, vec2<f32>(0, 0));
  }

  {
    let t: texture_2d<f32>;
    let s: sampler;
    _ = textureSample(t, s, vec2<f32>(0, 0), vec2<i32>(1, 1));
  }

  {
    let t: texture_2d_array<f32>;
    let s: sampler;
    _ = textureSample(t, s, vec2<f32>(0, 0), 0);
  }

  {
    let t: texture_2d_array<f32>;
    let s: sampler;
    _ = textureSample(t, s, vec2<f32>(0, 0), 0, vec2<i32>(1, 1));
  }

  {
    let t: texture_3d<f32>;
    let s: sampler;
    _ = textureSample(t, s, vec3<f32>(0, 0, 0));
  }

  {
    let t: texture_cube<f32>;
    let s: sampler;
    _ = textureSample(t, s, vec3<f32>(0, 0, 0));
  }

  {
    let t: texture_3d<f32>;
    let s: sampler;
    _ = textureSample(t, s, vec3<f32>(0, 0, 0), vec3<i32>(0, 0, 0));
  }

  {
    let t: texture_cube_array<f32>;
    let s: sampler;
    _ = textureSample(t, s, vec3<f32>(0, 0, 0), 0);
  }
}

fn testTextureLoad()
{
    // [T < ConcreteInteger, U < ConcreteInteger, S < Concrete32BitNumber].(Texture[S, Texture1d], T, U) => Vector[S, 4],
    {
        {
            let t: texture_1d<f32>;
            _ = textureLoad(t, 0, 0);
            _ = textureLoad(t, 0i, 0u);
            _ = textureLoad(t, 0u, 0i);
        }
        {
            let t: texture_1d<i32>;
            _ = textureLoad(t, 0, 0);
            _ = textureLoad(t, 0i, 0u);
            _ = textureLoad(t, 0u, 0i);
        }
        {
            let t: texture_1d<u32>;
            _ = textureLoad(t, 0, 0);
            _ = textureLoad(t, 0i, 0u);
            _ = textureLoad(t, 0u, 0i);
        }
    }

    // [T < ConcreteInteger, U < ConcreteInteger, S < Concrete32BitNumber].(Texture[S, Texture2d], Vector[T, 2], U) => Vector[S, 4],
    {
        {
            let t: texture_2d<f32>;
            _ = textureLoad(t, vec2(0), 0);
            _ = textureLoad(t, vec2(0i), 0u);
            _ = textureLoad(t, vec2(0u), 0i);
        }
        {
            let t: texture_2d<i32>;
            _ = textureLoad(t, vec2(0), 0);
            _ = textureLoad(t, vec2(0i), 0u);
            _ = textureLoad(t, vec2(0u), 0i);
        }
        {
            let t: texture_2d<u32>;
            _ = textureLoad(t, vec2(0), 0);
            _ = textureLoad(t, vec2(0i), 0u);
            _ = textureLoad(t, vec2(0u), 0i);
        }
    }

    // [T < ConcreteInteger, V < ConcreteInteger, U < ConcreteInteger, S < Concrete32BitNumber].(Texture[S, Texture2dArray], Vector[T, 2], V, U) => Vector[S, 4],
    {
        {
            let t: texture_2d_array<f32>;
            _ = textureLoad(t, vec2(0), 0, 0);
            _ = textureLoad(t, vec2(0i), 0u, 0i);
            _ = textureLoad(t, vec2(0u), 0i, 0u);
        }
        {
            let t: texture_2d_array<i32>;
            _ = textureLoad(t, vec2(0), 0, 0);
            _ = textureLoad(t, vec2(0i), 0u, 0i);
            _ = textureLoad(t, vec2(0u), 0i, 0u);
        }
        {
            let t: texture_2d_array<u32>;
            _ = textureLoad(t, vec2(0), 0, 0);
            _ = textureLoad(t, vec2(0i), 0u, 0i);
            _ = textureLoad(t, vec2(0u), 0i, 0u);
        }
    }

    // [T < ConcreteInteger, U < ConcreteInteger, S < Concrete32BitNumber].(Texture[S, Texture3d], Vector[T, 3], U) => Vector[S, 4],
    {
        {
            let t: texture_3d<f32>;
            _ = textureLoad(t, vec3(0), 0);
            _ = textureLoad(t, vec3(0i), 0u);
            _ = textureLoad(t, vec3(0u), 0i);
        }
        {
            let t: texture_3d<i32>;
            _ = textureLoad(t, vec3(0), 0);
            _ = textureLoad(t, vec3(0i), 0u);
            _ = textureLoad(t, vec3(0u), 0i);
        }
        {
            let t: texture_3d<u32>;
            _ = textureLoad(t, vec3(0), 0);
            _ = textureLoad(t, vec3(0i), 0u);
            _ = textureLoad(t, vec3(0u), 0i);
        }
    }

    // [T < ConcreteInteger, U < ConcreteInteger, S < Concrete32BitNumber].(Texture[S, TextureMultisampled2d], Vector[T, 2], U) => Vector[S, 4],
    {
        {
            let t: texture_multisampled_2d<f32>;
            _ = textureLoad(t, vec2(0), 0);
            _ = textureLoad(t, vec2(0i), 0u);
            _ = textureLoad(t, vec2(0u), 0i);
        }
        {
            let t: texture_multisampled_2d<i32>;
            _ = textureLoad(t, vec2(0), 0);
            _ = textureLoad(t, vec2(0i), 0u);
            _ = textureLoad(t, vec2(0u), 0i);
        }
        {
            let t: texture_multisampled_2d<u32>;
            _ = textureLoad(t, vec2(0), 0);
            _ = textureLoad(t, vec2(0i), 0u);
            _ = textureLoad(t, vec2(0u), 0i);
        }
    }

    // [T < ConcreteInteger].(TextureExternal, Vector[T, 2]) => Vector[F32, 4],
    {
        let t: texture_external;
        _ = textureLoad(t, vec2(0));
        _ = textureLoad(t, vec2(0i));
        _ = textureLoad(t, vec2(0u));
    }
}

fn testUnaryMinus() {
  let x = 1;
  _ = -x;
  _ = -vec2(1, 1);
}

fn testBinaryMinus() {
  _ = vec2(1, 1) - 1;
  _ = 1 - vec2(1, 1);
  _ = vec2(1, 1) - vec2(1, 1);
}

fn testComparison() {
  {
    _ = false == true;
    _ = 0 == 1;
    _ = 0i == 1i;
    _ = 0u == 1u;
    _ = 0.0 == 1.0;
    _ = 0.0f == 1.0f;
    _ = vec2(false) == vec2(true);
    _ = vec2(0) == vec2(1);
    _ = vec2(0i) == vec2(1i);
    _ = vec2(0u) == vec2(1u);
    _ = vec2(0.0) == vec2(1.0);
    _ = vec2(0.0f) == vec2(1.0f);
  }

  {
    _ = false != true;
    _ = 0 != 1;
    _ = 0i != 1i;
    _ = 0u != 1u;
    _ = 0.0 != 1.0;
    _ = 0.0f != 1.0f;
    _ = vec2(false) != vec2(true);
    _ = vec2(0) != vec2(1);
    _ = vec2(0i) != vec2(1i);
    _ = vec2(0u) != vec2(1u);
    _ = vec2(0.0) != vec2(1.0);
    _ = vec2(0.0f) != vec2(1.0f);
  }

  {
    _ = 0 > 1;
    _ = 0i > 1i;
    _ = 0u > 1u;
    _ = 0.0 > 1.0;
    _ = 0.0f > 1.0f;
    _ = vec2(0) > vec2(1);
    _ = vec2(0i) > vec2(1i);
    _ = vec2(0u) > vec2(1u);
    _ = vec2(0.0) > vec2(1.0);
    _ = vec2(0.0f) > vec2(1.0f);
  }

  {
    _ = 0 >= 1;
    _ = 0i >= 1i;
    _ = 0u >= 1u;
    _ = 0.0 >= 1.0;
    _ = 0.0f >= 1.0f;
    _ = vec2(0) >= vec2(1);
    _ = vec2(0i) >= vec2(1i);
    _ = vec2(0u) >= vec2(1u);
    _ = vec2(0.0) >= vec2(1.0);
    _ = vec2(0.0f) >= vec2(1.0f);
  }

  {
    _ = 0 < 1;
    _ = 0i < 1i;
    _ = 0u < 1u;
    _ = 0.0 < 1.0;
    _ = 0.0f < 1.0f;
    _ = vec2(0) < vec2(1);
    _ = vec2(0i) < vec2(1i);
    _ = vec2(0u) < vec2(1u);
    _ = vec2(0.0) < vec2(1.0);
    _ = vec2(0.0f) < vec2(1.0f);
  }

  {
    _ = 0 <= 1;
    _ = 0i <= 1i;
    _ = 0u <= 1u;
    _ = 0.0 <= 1.0;
    _ = 0.0f <= 1.0f;
    _ = vec2(0) <= vec2(1);
    _ = vec2(0i) <= vec2(1i);
    _ = vec2(0u) <= vec2(1u);
    _ = vec2(0.0) <= vec2(1.0);
    _ = vec2(0.0f) <= vec2(1.0f);
  }
}

fn testBitwise()
{
  {
    _ = ~0;
    _ = ~0i;
    _ = ~0u;
  }

  {
    _ = 0 & 1;
    _ = 0i & 1i;
    _ = 0u & 1u;
  }

  {
    _ = 0 | 1;
    _ = 0i | 1i;
    _ = 0u | 1u;
  }

  {
    _ = 0 ^ 1;
    _ = 0i ^ 1i;
    _ = 0u ^ 1u;
  }
}

// 7.6. Logical Expressions (https://gpuweb.github.io/gpuweb/wgsl/#logical-expr)

fn testLogicalNegation()
{
    // [].(Bool) => Bool,
    _ = !true;
    _ = !false;

    // [N].(Vector[Bool, N]) => Vector[Bool, N],
    _ = !vec2(true);
    _ = !vec3(true);
    _ = !vec4(true);
    _ = !vec2(false);
    _ = !vec3(false);
    _ = !vec4(false);
}

fn testShortCircuitingOr()
{
    // [].(Bool, Bool) => Bool,
    _ = false || false;
    _ = true || false;
    _ = false || true;
    _ = true || true;
}

fn testShortCircuitingAnd()
{
    // [].(Bool, Bool) => Bool,
    _ = false && false;
    _ = true && false;
    _ = false && true;
    _ = true && true;
}

fn testLogicalOr()
{
    // [].(Bool, Bool) => Bool,
    _ = false | false;
    _ = true | false;
    _ = false | true;
    _ = true | true;

    // [N].(Vector[Bool, N], Vector[Bool, N]) => Vector[Bool, N],
    _ = vec2(false) | vec2(false);
    _ = vec2( true) | vec2(false);
    _ = vec2(false) | vec2( true);
    _ = vec2( true) | vec2( true);
    _ = vec3(false) | vec3(false);
    _ = vec3( true) | vec3(false);
    _ = vec3(false) | vec3( true);
    _ = vec3( true) | vec3( true);
    _ = vec4(false) | vec4(false);
    _ = vec4( true) | vec4(false);
    _ = vec4(false) | vec4( true);
    _ = vec4( true) | vec4( true);
}

fn testLogicalAnd()
{
    // [].(Bool, Bool) => Bool,
    _ = false & false;
    _ = true & false;
    _ = false & true;
    _ = true & true;

    // [N].(Vector[Bool, N], Vector[Bool, N]) => Vector[Bool, N],
    _ = vec2(false) & vec2(false);
    _ = vec2( true) & vec2(false);
    _ = vec2(false) & vec2( true);
    _ = vec2( true) & vec2( true);
    _ = vec3(false) & vec3(false);
    _ = vec3( true) & vec3(false);
    _ = vec3(false) & vec3( true);
    _ = vec3( true) & vec3( true);
    _ = vec4(false) & vec4(false);
    _ = vec4( true) & vec4(false);
    _ = vec4(false) & vec4( true);
    _ = vec4( true) & vec4( true);
}

// 16.1. Constructor Built-in Functions

// 16.1.1. Zero Value Built-in Functions
struct S { x: i32 };
fn testZeroValueBuiltInFunctions()
{
    _ = bool();
    _ = i32();
    _ = u32();
    _ = f32();

    _ = vec2<f32>();
    _ = vec3<f32>();
    _ = vec4<f32>();

    _ = mat2x2<f32>();
    _ = mat2x3<f32>();
    _ = mat2x4<f32>();
    _ = mat3x2<f32>();
    _ = mat3x3<f32>();
    _ = mat3x4<f32>();
    _ = mat4x2<f32>();
    _ = mat4x3<f32>();
    _ = mat4x4<f32>();
    _ = S();
    _ = array<f32, 1>();
}

// 16.1.2. Value Constructor Built-in Functions

// 16.1.2.1.
fn testArray()
{
    _ = array<f32, 1>(0);
    _ = array<S, 2>(S(0), S(1));
}

// 16.1.2.2.
fn testBool()
{
    _ = bool(true);
    _ = bool(0);
    _ = bool(0i);
    _ = bool(0u);
    _ = bool(0.0);
    _ = bool(0f);
}

// 16.1.2.3. f16
// FIXME: add support for f16

// 16.1.2.4.
fn testF32()
{
    _ = f32(true);
    _ = f32(0);
    _ = f32(0i);
    _ = f32(0u);
    _ = f32(0.0);
    _ = f32(0f);
}

// 16.1.2.5.
fn testI32()
{
    _ = i32(true);
    _ = i32(0);
    _ = i32(0i);
    _ = i32(0u);
    _ = i32(0.0);
    _ = i32(0f);
}

// 16.1.2.6 - 14: matCxR
fn testMatrix()
{
  {
    _ = mat2x2<f32>(mat2x2(0.0, 0.0, 0.0, 0.0));
    _ = mat2x2(mat2x2(0.0, 0.0, 0.0, 0.0));
    _ = mat2x2(0.0, 0.0, 0.0, 0.0);
    _ = mat2x2(vec2(0.0, 0.0), vec2(0.0, 0.0));
  }

  {
    _ = mat2x3<f32>(mat2x3(0.0, 0.0, 0.0, 0.0, 0.0, 0.0));
    _ = mat2x3(mat2x3(0.0, 0.0, 0.0, 0.0, 0.0, 0.0));
    _ = mat2x3(0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
    _ = mat2x3(vec3(0.0, 0.0, 0.0), vec3(0.0, 0.0, 0.0));
  }

  {
    _ = mat2x4<f32>(mat2x4(0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0));
    _ = mat2x4(mat2x4(0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0));
    _ = mat2x4(0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
    _ = mat2x4(vec4(0.0, 0.0, 0.0, 0.0), vec4(0.0, 0.0, 0.0, 0.0));
  }

  {
    _ = mat3x2<f32>(mat3x2(0.0, 0.0, 0.0, 0.0, 0.0, 0.0));
    _ = mat3x2(mat3x2(0.0, 0.0, 0.0, 0.0, 0.0, 0.0));
    _ = mat3x2(0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
    _ = mat3x2(vec2(0.0, 0.0), vec2(0.0, 0.0), vec2(0.0, 0.0));
  }

  {
    _ = mat3x3<f32>(mat3x3(0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0));
    _ = mat3x3(mat3x3(0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0));
    _ = mat3x3(0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
    _ = mat3x3(vec3(0.0, 0.0, 0.0), vec3(0.0, 0.0, 0.0), vec3(0.0, 0.0, 0.0));
  }

  {
    _ = mat3x4<f32>(mat3x4(0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0));
    _ = mat3x4(mat3x4(0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0));
    _ = mat3x4(0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
    _ = mat3x4(vec4(0.0, 0.0, 0.0, 0.0), vec4(0.0, 0.0, 0.0, 0.0), vec4(0.0, 0.0, 0.0, 0.0));
  }

  {
    _ = mat4x2<f32>(mat4x2(0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0));
    _ = mat4x2(mat4x2(0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0));
    _ = mat4x2(0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
    _ = mat4x2(vec2(0.0, 0.0), vec2(0.0, 0.0), vec2(0.0, 0.0), vec2(0.0, 0.0));
  }

  {
    _ = mat4x3<f32>(mat4x3(0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0));
    _ = mat4x3(mat4x3(0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0));
    _ = mat4x3(0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
    _ = mat4x3(vec3(0.0, 0.0, 0.0), vec3(0.0, 0.0, 0.0), vec3(0.0, 0.0, 0.0), vec3(0.0, 0.0, 0.0));
  }

  {
    _ = mat4x4<f32>(mat4x4(0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0));
    _ = mat4x4(mat4x4(0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0));
    _ = mat4x4(mat4x4(0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0));
    _ = mat4x4(vec4(0.0, 0.0, 0.0, 0.0), vec4(0.0, 0.0, 0.0, 0.0), vec4(0.0, 0.0, 0.0, 0.0), vec4(0.0, 0.0, 0.0, 0.0));
  }
}

// 16.1.2.15.
fn testStructures()
{
    _ = S(42);
    _ = S(42i);
}

// 16.1.2.16.
fn testU32()
{
    _ = u32(true);
    _ = u32(0);
    _ = u32(0i);
    _ = u32(0u);
    _ = u32(0.0);
    _ = u32(0f);
}

// 16.1.2.17.
fn testVec2() {
  _ = vec2<f32>(0);
  _ = vec2<f32>(0, 0);
  _ = vec2<i32>(vec2<f32>(0));
  _ = vec2(vec2(0, 0));
  _ = vec2(0, 0);
}

// 16.1.2.18.
fn testVec3() {
  _ = vec3<f32>(0);
  _ = vec3<f32>(0, 0, 0);
  _ = vec3<i32>(vec3<f32>(0));
  _ = vec3(vec3(0, 0, 0));
  _ = vec3(0, 0, 0);
  _ = vec3(vec2(0, 0), 0);
  _ = vec3(0, vec2(0, 0));
}

// 16.1.2.19.
fn testVec4() {
  _ = vec4<f32>(0);
  _ = vec4<f32>(0, 0, 0, 0);
  _ = vec4<i32>(vec4<f32>(0));
  _ = vec4(vec4(0, 0, 0, 0));
  _ = vec4(0, 0, 0, 0);
  _ = vec4(0, vec2(0, 0), 0);
  _ = vec4(0, 0, vec2(0, 0));
  _ = vec4(vec2(0, 0), 0, 0);
  _ = vec4(vec2(0, 0), vec2(0, 0));
  _ = vec4(vec3(0, 0, 0), 0);
  _ = vec4(0, vec3(0, 0, 0));
}

// 17.3. Logical Built-in Functions (https://www.w3.org/TR/WGSL/#logical-builtin-functions)

// 17.3.1
fn testAll()
{
    // [N].(Vector[Bool, N]) => Bool,
    _ = all(vec2(false, true));
    _ = all(vec3(true, true, true));
    _ = all(vec4(false, false, false, false));

    // [N].(Bool) => Bool,
    _ = all(true);
    _ = all(false);
}

// 17.3.2
fn testAny()
{
    // [N].(Vector[Bool, N]) => Bool,
    _ = any(vec2(false, true));
    _ = any(vec3(true, true, true));
    _ = any(vec4(false, false, false, false));

    // [N].(Bool) => Bool,
    _ = any(true);
    _ = any(false);
}

// 17.3.3
fn testSelect()
{
    // [T < Scalar].(T, T, Bool) => T,
    {
        _ = select(13, 42,   false);
        _ = select(13, 42i,  false);
        _ = select(13, 42u,  true);
        _ = select(13, 42f,  true);
        _ = select(13, 42.0, true);
    }

    // [T < Scalar, N].(Vector[T, N], Vector[T, N], Bool) => Vector[T, N],
    {
        _ = select(vec2(13), vec2(42),   false);
        _ = select(vec2(13), vec2(42i),  false);
        _ = select(vec2(13), vec2(42u),  true);
        _ = select(vec2(13), vec2(42f),  true);
        _ = select(vec2(13), vec2(42.0), true);
    }
    {
        _ = select(vec3(13), vec3(42),   false);
        _ = select(vec3(13), vec3(42i),  false);
        _ = select(vec3(13), vec3(42u),  true);
        _ = select(vec3(13), vec3(42f),  true);
        _ = select(vec3(13), vec3(42.0), true);
    }
    {
        _ = select(vec4(13), vec4(42),   false);
        _ = select(vec4(13), vec4(42i),  false);
        _ = select(vec4(13), vec4(42u),  true);
        _ = select(vec4(13), vec4(42f),  true);
        _ = select(vec4(13), vec4(42.0), true);
    }

    // [T < Scalar, N].(Vector[T, N], Vector[T, N], Vector[Bool, N]) => Vector[T, N],
    {
        _ = select(vec2(13), vec2(42),   vec2(false));
        _ = select(vec2(13), vec2(42i),  vec2(false));
        _ = select(vec2(13), vec2(42u),  vec2(true));
        _ = select(vec2(13), vec2(42f),  vec2(true));
        _ = select(vec2(13), vec2(42.0), vec2(true));
    }
    {
        _ = select(vec3(13), vec3(42),   vec3(false));
        _ = select(vec3(13), vec3(42i),  vec3(false));
        _ = select(vec3(13), vec3(42u),  vec3(true));
        _ = select(vec3(13), vec3(42f),  vec3(true));
        _ = select(vec3(13), vec3(42.0), vec3(true));
    }
    {
        _ = select(vec4(13), vec4(42),   vec4(false));
        _ = select(vec4(13), vec4(42i),  vec4(false));
        _ = select(vec4(13), vec4(42u),  vec4(true));
        _ = select(vec4(13), vec4(42f),  vec4(true));
        _ = select(vec4(13), vec4(42.0), vec4(true));
    }
}

// 17.5. Numeric Built-in Functions (https://www.w3.org/TR/WGSL/#numeric-builtin-functions)

// Trigonometric
fn testTrigonometric()
{
  {
    _ = acos(0.0);
    _ = acos(vec2(0.0, 0.0));
    _ = acos(vec3(0.0, 0.0, 0.0));
    _ = acos(vec4(0.0, 0.0, 0.0, 0.0));
  }

  {
    _ = asin(0.0);
    _ = asin(vec2(0.0, 0.0));
    _ = asin(vec3(0.0, 0.0, 0.0));
    _ = asin(vec4(0.0, 0.0, 0.0, 0.0));
  }

  {
    _ = atan(0.0);
    _ = atan(vec2(0.0, 0.0));
    _ = atan(vec3(0.0, 0.0, 0.0));
    _ = atan(vec4(0.0, 0.0, 0.0, 0.0));
  }

  {
    _ = cos(0.0);
    _ = cos(vec2(0.0, 0.0));
    _ = cos(vec3(0.0, 0.0, 0.0));
    _ = cos(vec4(0.0, 0.0, 0.0, 0.0));
  }

  {
    _ = sin(0.0);
    _ = sin(vec2(0.0, 0.0));
    _ = sin(vec3(0.0, 0.0, 0.0));
    _ = sin(vec4(0.0, 0.0, 0.0, 0.0));
  }

  {
    _ = tan(0.0);
    _ = tan(vec2(0.0, 0.0));
    _ = tan(vec3(0.0, 0.0, 0.0));
    _ = tan(vec4(0.0, 0.0, 0.0, 0.0));
  }
}

fn testTrigonometricHyperbolic()
{
  {
    _ = acosh(0.0);
    _ = acosh(vec2(0.0, 0.0));
    _ = acosh(vec3(0.0, 0.0, 0.0));
    _ = acosh(vec4(0.0, 0.0, 0.0, 0.0));
  }

  {
    _ = asinh(0.0);
    _ = asinh(vec2(0.0, 0.0));
    _ = asinh(vec3(0.0, 0.0, 0.0));
    _ = asinh(vec4(0.0, 0.0, 0.0, 0.0));
  }

  {
    _ = atanh(0.0);
    _ = atanh(vec2(0.0, 0.0));
    _ = atanh(vec3(0.0, 0.0, 0.0));
    _ = atanh(vec4(0.0, 0.0, 0.0, 0.0));
  }

  {
    _ = cosh(0.0);
    _ = cosh(vec2(0.0, 0.0));
    _ = cosh(vec3(0.0, 0.0, 0.0));
    _ = cosh(vec4(0.0, 0.0, 0.0, 0.0));
  }

  {
    _ = sinh(0.0);
    _ = sinh(vec2(0.0, 0.0));
    _ = sinh(vec3(0.0, 0.0, 0.0));
    _ = sinh(vec4(0.0, 0.0, 0.0, 0.0));
  }

  {
    _ = tanh(0.0);
    _ = tanh(vec2(0.0, 0.0));
    _ = tanh(vec3(0.0, 0.0, 0.0));
    _ = tanh(vec4(0.0, 0.0, 0.0, 0.0));
  }
}


// 17.5.1
fn testAbs()
{
    // [T < Float].(T) => T,
    {
        _ = abs(0);
        _ = abs(1i);
        _ = abs(1u);
        _ = abs(0.0);
        _ = abs(1f);
    }

    // [T < Float, N].(Vector[T, N]) => Vector[T, N],
    {
        _ = abs(vec2(0, 1));
        _ = abs(vec2(1i, 2i));
        _ = abs(vec2(1u, 2u));
        _ = abs(vec2(0.0, 1.0));
        _ = abs(vec2(1f, 2f));
    }
    {
        _ = abs(vec3(-1, 0, 1));
        _ = abs(vec3(-1i, 1i, 2i));
        _ = abs(vec3(0u, 1u, 2u));
        _ = abs(vec3(-1.0, 0.0, 1.0));
        _ = abs(vec3(-1f, 1f, 2f));
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
        _ = atan2(0, 1);
        _ = atan2(0, 1.0);
        _ = atan2(1, 2f);
    }
    // [T < Float, N].(Vector[T, N], Vector[T, N], Vector[T, N]) => Vector[T, N],
    {
        _ = atan2(vec2(0), vec2(0));
        _ = atan2(vec2(0), vec2(0.0));
        _ = atan2(vec2(0), vec2(0f));
    }
    {
        _ = atan2(vec3(0), vec3(0));
        _ = atan2(vec3(0), vec3(0.0));
        _ = atan2(vec3(0), vec3(0f));
    }
    {
        _ = atan2(vec4(0), vec4(0));
        _ = atan2(vec4(0), vec4(0.0));
        _ = atan2(vec4(0), vec4(0f));
    }
}

// 17.5.9
fn testCeil()
{
    // [T < Float].(T) => T,
    {
        _ = ceil(0);
        _ = ceil(0.0);
        _ = ceil(1f);
    }

    // [T < Float, N].(Vector[T, N]) => Vector[T, N],
    {
        _ = ceil(vec2(0, 1));
        _ = ceil(vec2(0.0, 1.0));
        _ = ceil(vec2(1f, 2f));
    }
    {
        _ = ceil(vec3(-1, 0, 1));
        _ = ceil(vec3(-1.0, 0.0, 1.0));
        _ = ceil(vec3(-1f, 1f, 2f));
    }
    {
        _ = ceil(vec4(-1, 0, 1, 2));
        _ = ceil(vec4(-1.0, 0.0, 1.0, 2.0));
        _ = ceil(vec4(-1f, 1f, 2f, 3f));
    }
}

// 17.5.10
fn testClamp()
{
    // [T < Number].(T, T, T) => T,
    {
        _ = clamp(-1, 0, 1);
        _ = clamp(-1, 1, 2i);
        _ = clamp(0, 1, 2u);
        _ = clamp(-1, 0, 1.0);
        _ = clamp(-1, 1, 2f);
    }
    // [T < Number, N].(Vector[T, N], Vector[T, N], Vector[T, N]) => Vector[T, N],
    {
        _ = clamp(vec2(0), vec2(0), vec2(0));
        _ = clamp(vec2(0), vec2(0), vec2(0i));
        _ = clamp(vec2(0), vec2(0), vec2(0u));
        _ = clamp(vec2(0), vec2(0), vec2(0.0));
        _ = clamp(vec2(0), vec2(0), vec2(0f));
    }
    {
        _ = clamp(vec3(0), vec3(0), vec3(0));
        _ = clamp(vec3(0), vec3(0), vec3(0i));
        _ = clamp(vec3(0), vec3(0), vec3(0u));
        _ = clamp(vec3(0), vec3(0), vec3(0.0));
        _ = clamp(vec3(0), vec3(0), vec3(0f));
    }
    {
        _ = clamp(vec4(0), vec4(0), vec4(0));
        _ = clamp(vec4(0), vec4(0), vec4(0i));
        _ = clamp(vec4(0), vec4(0), vec4(0u));
        _ = clamp(vec4(0), vec4(0), vec4(0.0));
        _ = clamp(vec4(0), vec4(0), vec4(0f));
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
        _ = countLeadingZeros(1);
        _ = countLeadingZeros(1i);
        _ = countLeadingZeros(1u);
    }
    {
        _ = countOneBits(1);
        _ = countOneBits(1i);
        _ = countOneBits(1u);
    }
    {
        _ = countTrailingZeros(1);
        _ = countTrailingZeros(1i);
        _ = countTrailingZeros(1u);
    }
    // [T < ConcreteInteger, N].(Vector[T, N]) => Vector[T, N],
    {
        _ = countLeadingZeros(vec2(1, 1));
        _ = countLeadingZeros(vec2(1i, 1i));
        _ = countLeadingZeros(vec2(1u, 1u));
    }
    {
        _ = countOneBits(vec3(1, 1, 1));
        _ = countOneBits(vec3(1i, 1i, 1i));
        _ = countOneBits(vec3(1u, 1u, 1u));
    }
    {
        _ = countTrailingZeros(vec4(1, 1, 1, 1));
        _ = countTrailingZeros(vec4(1i, 1i, 1i, 1i));
        _ = countTrailingZeros(vec4(1u, 1u, 1u, 1u));
    }
}

// 17.5.16
fn testCross()
{
    // [T < Float].(Vector[T, 3], Vector[T, 3]) => Vector[T, 3],
    _ = cross(vec3(1, 1, 1), vec3(1f, 2f, 3f));
    _ = cross(vec3(1.0, 1.0, 1.0), vec3(1f, 2f, 3f));
    _ = cross(vec3(1f, 1f, 1f), vec3(1f, 2f, 3f));
}

// 17.5.17
fn testDegress()
{
    // [T < Float].(T) => T,
    {
        _ = degrees(0);
        _ = degrees(0.0);
        _ = degrees(1f);
    }
    // [T < Float, N].(Vector[T, N]) => Vector[T, N],
    {
        _ = degrees(vec2(0, 1));
        _ = degrees(vec2(0.0, 1.0));
        _ = degrees(vec2(1f, 2f));
    }
    {
        _ = degrees(vec3(-1, 0, 1));
        _ = degrees(vec3(-1.0, 0.0, 1.0));
        _ = degrees(vec3(-1f, 1f, 2f));
    }
    {
        _ = degrees(vec4(-1, 0, 1, 2));
        _ = degrees(vec4(-1.0, 0.0, 1.0, 2.0));
        _ = degrees(vec4(-1f, 1f, 2f, 3f));
    }
}

// 17.5.18
fn testDeterminant()
{
    // [T < Float, C].(Matrix[T, C, C]) => T,
    _ = determinant(mat2x2(1, 1, 1, 1));
    _ = determinant(mat3x3(1, 1, 1, 1, 1, 1, 1, 1, 1));
    _ = determinant(mat4x4(1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1));
}

// 17.5.19
fn testDistance()
{
    // [T < Float].(T, T) => T,
    {
        _ = distance(0, 1);
        _ = distance(0, 1.0);
        _ = distance(0, 1f);
        _ = distance(0.0, 1.0);
        _ = distance(1.0, 2f);
        _ = distance(1f, 2f);
    }
    // [T < Float, N].(Vector[T, N], Vector[T, N]) => T,
    {
        _ = distance(vec2(0),   vec2(1)  );
        _ = distance(vec2(0),   vec2(0.0));
        _ = distance(vec2(0),   vec2(1f) );
        _ = distance(vec2(0.0), vec2(0.0));
        _ = distance(vec2(0.0), vec2(0f) );
        _ = distance(vec2(1f),  vec2(1f) );
    }
    {
        _ = distance(vec3(0),   vec3(1)  );
        _ = distance(vec3(0),   vec3(0.0));
        _ = distance(vec3(0),   vec3(1f) );
        _ = distance(vec3(0.0), vec3(0.0));
        _ = distance(vec3(0.0), vec3(0f) );
        _ = distance(vec3(1f),  vec3(1f) );
    }
    {
        _ = distance(vec4(0),   vec4(1)  );
        _ = distance(vec4(0),   vec4(0.0));
        _ = distance(vec4(0),   vec4(1f) );
        _ = distance(vec4(0.0), vec4(0.0));
        _ = distance(vec4(0.0), vec4(0f) );
        _ = distance(vec4(1f),  vec4(1f) );
    }
}

// 17.5.20
fn testDot()
{
    // [T < Number, N].(Vector[T, N], Vector[T, N]) => T,
    {
        _ = dot(vec2(0),   vec2(1)  );
        _ = dot(vec2(0),   vec2(1f) );
        _ = dot(vec2(0),   vec2(1i) );
        _ = dot(vec2(0),   vec2(1u) );
        _ = dot(vec2(1i),  vec2(1i) );
        _ = dot(vec2(1u),  vec2(1u) );
        _ = dot(vec2(0),   vec2(0.0));
        _ = dot(vec2(0.0), vec2(0.0));
        _ = dot(vec2(0.0), vec2(0f) );
        _ = dot(vec2(1f),  vec2(1f) );
    }
    {
        _ = dot(vec3(0),   vec3(1)  );
        _ = dot(vec3(0),   vec3(1f) );
        _ = dot(vec3(0),   vec3(1i) );
        _ = dot(vec3(0),   vec3(1u) );
        _ = dot(vec3(1i),  vec3(1i) );
        _ = dot(vec3(1u),  vec3(1u) );
        _ = dot(vec3(0),   vec3(0.0));
        _ = dot(vec3(0.0), vec3(0.0));
        _ = dot(vec3(0.0), vec3(0f) );
        _ = dot(vec3(1f),  vec3(1f) );
    }
    {
        _ = dot(vec4(0),   vec4(1)  );
        _ = dot(vec4(0),   vec4(1f) );
        _ = dot(vec4(0),   vec4(1i) );
        _ = dot(vec4(0),   vec4(1u) );
        _ = dot(vec4(1i),  vec4(1i) );
        _ = dot(vec4(1u),  vec4(1u) );
        _ = dot(vec4(0),   vec4(0.0));
        _ = dot(vec4(0.0), vec4(0.0));
        _ = dot(vec4(0.0), vec4(0f) );
        _ = dot(vec4(1f),  vec4(1f) );
    }
}

// 17.5.21 & 17.5.22
fn testExpAndExp2() {
    // [T < Float].(T) => T,
    {
        _ = exp(0);
        _ = exp(0.0);
        _ = exp(1f);
    }
    {
        _ = exp2(0);
        _ = exp2(0.0);
        _ = exp2(1f);
    }

    // [T < Float, N].(Vector[T, N]) => Vector[T, N],
    {
        _ = exp(vec2(0, 1));
        _ = exp(vec2(0.0, 1.0));
        _ = exp(vec2(1f, 2f));
    }
    {
        _ = exp(vec3(-1, 0, 1));
        _ = exp(vec3(-1.0, 0.0, 1.0));
        _ = exp(vec3(-1f, 1f, 2f));
    }
    {
        _ = exp(vec4(-1, 0, 1, 2));
        _ = exp(vec4(-1.0, 0.0, 1.0, 2.0));
        _ = exp(vec4(-1f, 1f, 2f, 3f));
    }

    {
        _ = exp2(vec2(0, 1));
        _ = exp2(vec2(0.0, 1.0));
        _ = exp2(vec2(1f, 2f));
    }
    {
        _ = exp2(vec3(-1, 0, 1));
        _ = exp2(vec3(-1.0, 0.0, 1.0));
        _ = exp2(vec3(-1f, 1f, 2f));
    }
    {
        _ = exp2(vec4(-1, 0, 1, 2));
        _ = exp2(vec4(-1.0, 0.0, 1.0, 2.0));
        _ = exp2(vec4(-1f, 1f, 2f, 3f));
    }
}

// 17.5.23 & 17.5.24
fn testExtractBits()
{
    // signed
    // [].(I32, U32, U32) => I32,
    {
        _ = extractBits(0, 1, 1);
        _ = extractBits(0i, 1, 1);
        _ = extractBits(0i, 1u, 1u);
    }
    // [N].(Vector[I32, N], U32, U32) => Vector[I32, N],
    {
        _ = extractBits(vec2(0), 1, 1);
        _ = extractBits(vec2(0i), 1, 1);
        _ = extractBits(vec2(0i), 1u, 1u);
    }
    {
        _ = extractBits(vec3(0), 1, 1);
        _ = extractBits(vec3(0i), 1, 1);
        _ = extractBits(vec3(0i), 1u, 1u);
    }
    {
        _ = extractBits(vec4(0), 1, 1);
        _ = extractBits(vec4(0i), 1, 1);
        _ = extractBits(vec4(0i), 1u, 1u);
    }

    // unsigned
    // [].(U32, U32, U32) => U32,
    {
        _ = extractBits(0, 1, 1);
        _ = extractBits(0u, 1, 1);
        _ = extractBits(0u, 1u, 1u);
    }

    // [N].(Vector[U32, N], U32, U32) => Vector[U32, N],
    {
        _ = extractBits(vec2(0), 1, 1);
        _ = extractBits(vec2(0u), 1, 1);
        _ = extractBits(vec2(0u), 1u, 1u);
    }
    {
        _ = extractBits(vec3(0), 1, 1);
        _ = extractBits(vec3(0u), 1, 1);
        _ = extractBits(vec3(0u), 1u, 1u);
    }
    {
        _ = extractBits(vec4(0), 1, 1);
        _ = extractBits(vec4(0u), 1, 1);
        _ = extractBits(vec4(0u), 1u, 1u);
    }
}

// 17.5.25
fn testFaceForward()
{
    // [T < Float, N].(Vector[T, N], Vector[T, N], Vector[T, N]) => Vector[T, N],
    {
        _ = faceForward(vec2(0), vec2(1),   vec2(2));
        _ = faceForward(vec2(0), vec2(1),   vec2(2.0));
        _ = faceForward(vec2(0), vec2(1.0), vec2(2f));
    }
    {
        _ = faceForward(vec3(0), vec3(1),   vec3(2));
        _ = faceForward(vec3(0), vec3(1),   vec3(2.0));
        _ = faceForward(vec3(0), vec3(1.0), vec3(2f));
    }
    {
        _ = faceForward(vec4(0), vec4(1),   vec4(2));
        _ = faceForward(vec4(0), vec4(1),   vec4(2.0));
        _ = faceForward(vec4(0), vec4(1.0), vec4(2f));
    }
}

// 17.5.26 & 17.5.27
fn testFirstLeadingBit()
{
    // signed
    // [].(I32) => I32,
    {
        _ = firstLeadingBit(0);
        _ = firstLeadingBit(0i);
    }
    // [N].(Vector[I32, N]) => Vector[I32, N],
    {
        _ = firstLeadingBit(vec2(0));
        _ = firstLeadingBit(vec2(0i));
    }
    {
        _ = firstLeadingBit(vec3(0));
        _ = firstLeadingBit(vec3(0i));
    }
    {
        _ = firstLeadingBit(vec4(0));
        _ = firstLeadingBit(vec4(0i));
    }

    // unsigned
    // [].(U32) => U32,
    {
        _ = firstLeadingBit(0);
        _ = firstLeadingBit(0u);
    }

    // [N].(Vector[U32, N]) => Vector[U32, N],
    {
        _ = firstLeadingBit(vec2(0));
        _ = firstLeadingBit(vec2(0u));
    }
    {
        _ = firstLeadingBit(vec3(0));
        _ = firstLeadingBit(vec3(0u));
    }
    {
        _ = firstLeadingBit(vec4(0));
        _ = firstLeadingBit(vec4(0u));
    }
}

// 17.5.28
fn testFirstTrailingBit()
{
    // [T < ConcreteInteger].(T) => T,
    {
        _ = firstTrailingBit(0);
        _ = firstTrailingBit(0i);
        _ = firstTrailingBit(0u);
    }

    // [T < ConcreteInteger, N].(Vector[T, N]) => Vector[T, N],
    {
        _ = firstTrailingBit(vec2(0));
        _ = firstTrailingBit(vec2(0i));
        _ = firstTrailingBit(vec2(0u));
    }
    {
        _ = firstTrailingBit(vec3(0));
        _ = firstTrailingBit(vec3(0i));
        _ = firstTrailingBit(vec3(0u));
    }
    {
        _ = firstTrailingBit(vec4(0));
        _ = firstTrailingBit(vec4(0i));
        _ = firstTrailingBit(vec4(0u));
    }
}

// 17.5.29
fn testFloor()
{
    // [T < Float].(T) => T,
    {
        _ = floor(0);
        _ = floor(0.0);
        _ = floor(1f);
    }

    // [T < Float, N].(Vector[T, N]) => Vector[T, N],
    {
        _ = floor(vec2(0, 1));
        _ = floor(vec2(0.0, 1.0));
        _ = floor(vec2(1f, 2f));
    }
    {
        _ = floor(vec3(-1, 0, 1));
        _ = floor(vec3(-1.0, 0.0, 1.0));
        _ = floor(vec3(-1f, 1f, 2f));
    }
    {
        _ = floor(vec4(-1, 0, 1, 2));
        _ = floor(vec4(-1.0, 0.0, 1.0, 2.0));
        _ = floor(vec4(-1f, 1f, 2f, 3f));
    }
}

// 17.5.30
fn testFma()
{
    // [T < Float].(T, T, T) => T,
    {
        _ = fma(-1, 0, 1);
        _ = fma(-1, 0, 1.0);
        _ = fma(-1, 1, 2f);
    }
    // [T < Float, N].(Vector[T, N], Vector[T, N], Vector[T, N]) => Vector[T, N],
    {
        _ = fma(vec2(0), vec2(0), vec2(0));
        _ = fma(vec2(0), vec2(0), vec2(0.0));
        _ = fma(vec2(0), vec2(0), vec2(0f));
    }
    {
        _ = fma(vec3(0), vec3(0), vec3(0));
        _ = fma(vec3(0), vec3(0), vec3(0.0));
        _ = fma(vec3(0), vec3(0), vec3(0f));
    }
    {
        _ = fma(vec4(0), vec4(0), vec4(0));
        _ = fma(vec4(0), vec4(0), vec4(0.0));
        _ = fma(vec4(0), vec4(0), vec4(0f));
    }
}

// 17.5.31
fn testFract()
{
    // [T < Float].(T) => T,
    {
        _ = fract(0);
        _ = fract(0.0);
        _ = fract(1f);
    }

    // [T < Float, N].(Vector[T, N]) => Vector[T, N],
    {
        _ = fract(vec2(0));
        _ = fract(vec2(0.0));
        _ = fract(vec2(1f));
    }
    {
        _ = fract(vec3(-1));
        _ = fract(vec3(-1.0));
        _ = fract(vec3(-1f));
    }
    {
        _ = fract(vec4(-1));
        _ = fract(vec4(-1.0));
        _ = fract(vec4(-1f));
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
        _ = insertBits(0, 0, 0, 0);
        _ = insertBits(0, 0i, 0, 0);
        _ = insertBits(0, 0u, 0, 0);
    }

    // [T < ConcreteInteger, N].(Vector[T, N], Vector[T, N], U32, U32) => Vector[T, N],
    {
        _ = insertBits(vec2(0), vec2(0), 0, 0);
        _ = insertBits(vec2(0), vec2(0i), 0, 0);
        _ = insertBits(vec2(0), vec2(0u), 0, 0);
    }
    {
        _ = insertBits(vec3(0), vec3(0), 0, 0);
        _ = insertBits(vec3(0), vec3(0i), 0, 0);
        _ = insertBits(vec3(0), vec3(0u), 0, 0);
    }
    {
        _ = insertBits(vec4(0), vec4(0), 0, 0);
        _ = insertBits(vec4(0), vec4(0i), 0, 0);
        _ = insertBits(vec4(0), vec4(0u), 0, 0);
    }
}

// 17.5.34
fn testInverseSqrt()
{
    // [T < Float].(T) => T,
    {
        _ = inverseSqrt(0);
        _ = inverseSqrt(0.0);
        _ = inverseSqrt(1f);
    }

    // [T < Float, N].(Vector[T, N]) => Vector[T, N],
    {
        _ = inverseSqrt(vec2(0));
        _ = inverseSqrt(vec2(0.0));
        _ = inverseSqrt(vec2(1f));
    }
    {
        _ = inverseSqrt(vec3(-1));
        _ = inverseSqrt(vec3(-1.0));
        _ = inverseSqrt(vec3(-1f));
    }
    {
        _ = inverseSqrt(vec4(-1));
        _ = inverseSqrt(vec4(-1.0));
        _ = inverseSqrt(vec4(-1f));
    }
}

// 17.5.35
fn testLdexp()
{
    // [T < ConcreteFloat].(T, I32) => T,
    {
        _ = ldexp(0f, 1);
    }
    // [].(AbstractFloat, AbstractInt) => AbstractFloat,
    {
        _ = ldexp(0, 1);
        _ = ldexp(0.0, 1);
    }
    // [T < ConcreteFloat, N].(Vector[T, N], Vector[I32, N]) => Vector[T, N],
    {
        _ = ldexp(vec2(0f), vec2(1));
        _ = ldexp(vec3(0f), vec3(1));
        _ = ldexp(vec4(0f), vec4(1));
    }
    // [N].(Vector[AbstractFloat, N], Vector[AbstractInt, N]) => Vector[AbstractFloat, N],
    {
        _ = ldexp(vec2(0), vec2(1));
        _ = ldexp(vec3(0), vec3(1));
        _ = ldexp(vec4(0), vec4(1));

        _ = ldexp(vec2(0.0), vec2(1));
        _ = ldexp(vec3(0.0), vec3(1));
        _ = ldexp(vec4(0.0), vec4(1));
    }
}

// 17.5.36
fn testLength()
{
    // [T < Float].(T) => T,
    {
        _ = length(0);
        _ = length(0.0);
        _ = length(1f);
    }

    // [T < Float, N].(Vector[T, N]) => Vector[T, N],
    {
        _ = length(vec2(0));
        _ = length(vec2(0.0));
        _ = length(vec2(1f));
    }
    {
        _ = length(vec3(-1));
        _ = length(vec3(-1.0));
        _ = length(vec3(-1f));
    }
    {
        _ = length(vec4(-1));
        _ = length(vec4(-1.0));
        _ = length(vec4(-1f));
    }
}

// 17.5.37
fn testLog()
{
    // [T < Float].(T) => T,
    {
        _ = log(0);
        _ = log(0.0);
        _ = log(1f);
    }

    // [T < Float, N].(Vector[T, N]) => Vector[T, N],
    {
        _ = log(vec2(0));
        _ = log(vec2(0.0));
        _ = log(vec2(1f));
    }
    {
        _ = log(vec3(-1));
        _ = log(vec3(-1.0));
        _ = log(vec3(-1f));
    }
    {
        _ = log(vec4(-1));
        _ = log(vec4(-1.0));
        _ = log(vec4(-1f));
    }
}

// 17.5.38
fn testLog2() {
    // [T < Float].(T) => T,
    {
        _ = log2(0);
        _ = log2(0.0);
        _ = log2(1f);
    }

    // [T < Float, N].(Vector[T, N]) => Vector[T, N],
    {
        _ = log2(vec2(0));
        _ = log2(vec2(0.0));
        _ = log2(vec2(1f));
    }
    {
        _ = log2(vec3(-1));
        _ = log2(vec3(-1.0));
        _ = log2(vec3(-1f));
    }
    {
        _ = log2(vec4(-1));
        _ = log2(vec4(-1.0));
        _ = log2(vec4(-1f));
    }
}

// 17.5.39
fn testMax()
{
    // [T < Number].(T, T) => T,
    {
        _ = max(-1, 0);
        _ = max(-1, 1);
        _ = max(0, 1);
        _ = max(-1, 0);
        _ = max(-1, 1);
    }
    // [T < Number, N].(Vector[T, N], Vector[T, N]) => Vector[T, N],
    {
        _ = max(vec2(0), vec2(0));
        _ = max(vec2(0), vec2(0i));
        _ = max(vec2(0), vec2(0u));
        _ = max(vec2(0), vec2(0.0));
        _ = max(vec2(0), vec2(0f));
    }
    {
        _ = max(vec3(0), vec3(0));
        _ = max(vec3(0), vec3(0i));
        _ = max(vec3(0), vec3(0u));
        _ = max(vec3(0), vec3(0.0));
        _ = max(vec3(0), vec3(0f));
    }
    {
        _ = max(vec4(0), vec4(0));
        _ = max(vec4(0), vec4(0i));
        _ = max(vec4(0), vec4(0u));
        _ = max(vec4(0), vec4(0.0));
        _ = max(vec4(0), vec4(0f));
    }
}

// 17.5.40
fn testMin()
{
    // [T < Number].(T, T) => T,
    {
        _ = min(-1, 0);
        _ = min(-1, 1);
        _ = min(0, 1);
        _ = min(-1, 0);
        _ = min(-1, 1);
    }
    // [T < Number, N].(Vector[T, N], Vector[T, N]) => Vector[T, N],
    {
        _ = min(vec2(0), vec2(0));
        _ = min(vec2(0), vec2(0i));
        _ = min(vec2(0), vec2(0u));
        _ = min(vec2(0), vec2(0.0));
        _ = min(vec2(0), vec2(0f));
    }
    {
        _ = min(vec3(0), vec3(0));
        _ = min(vec3(0), vec3(0i));
        _ = min(vec3(0), vec3(0u));
        _ = min(vec3(0), vec3(0.0));
        _ = min(vec3(0), vec3(0f));
    }
    {
        _ = min(vec4(0), vec4(0));
        _ = min(vec4(0), vec4(0i));
        _ = min(vec4(0), vec4(0u));
        _ = min(vec4(0), vec4(0.0));
        _ = min(vec4(0), vec4(0f));
    }
}

// 17.5.41
fn testMix()
{
    // [T < Float].(T, T, T) => T,
    {
        _ = mix(-1, 0, 1);
        _ = mix(-1, 0, 1.0);
        _ = mix(-1, 1, 2f);
    }
    // [T < Float, N].(Vector[T, N], Vector[T, N], Vector[T, N]) => Vector[T, N],
    {
        _ = mix(vec2(0), vec2(0), vec2(0));
        _ = mix(vec2(0), vec2(0), vec2(0.0));
        _ = mix(vec2(0), vec2(0), vec2(0f));
    }
    {
        _ = mix(vec3(0), vec3(0), vec3(0));
        _ = mix(vec3(0), vec3(0), vec3(0.0));
        _ = mix(vec3(0), vec3(0), vec3(0f));
    }
    {
        _ = mix(vec4(0), vec4(0), vec4(0));
        _ = mix(vec4(0), vec4(0), vec4(0.0));
        _ = mix(vec4(0), vec4(0), vec4(0f));
    }
    // [T < Float, N].(Vector[T, N], Vector[T, N], T) => Vector[T, N],
    {
        _ = mix(vec2(0), vec2(0), 0);
        _ = mix(vec2(0), vec2(0), 0.0);
        _ = mix(vec2(0), vec2(0), 0f);
    }
    {
        _ = mix(vec3(0), vec3(0), 0);
        _ = mix(vec3(0), vec3(0), 0.0);
        _ = mix(vec3(0), vec3(0), 0f);
    }
    {
        _ = mix(vec4(0), vec4(0), 0);
        _ = mix(vec4(0), vec4(0), 0.0);
        _ = mix(vec4(0), vec4(0), 0f);
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
        _ = normalize(vec2(0));
        _ = normalize(vec2(0.0));
        _ = normalize(vec2(1f));
    }
    {
        _ = normalize(vec3(-1));
        _ = normalize(vec3(-1.0));
        _ = normalize(vec3(-1f));
    }
    {
        _ = normalize(vec4(-1));
        _ = normalize(vec4(-1.0));
        _ = normalize(vec4(-1f));
    }
}

// 17.5.44
fn testPow()
{
    // [T < Float].(T, T) => T,
    {
        _ = pow(0, 1);
        _ = pow(0, 1.0);
        _ = pow(0, 1f);
        _ = pow(0.0, 1.0);
        _ = pow(1.0, 2f);
        _ = pow(1f, 2f);
    }
    // [T < Float, N].(Vector[T, N], Vector[T, N]) => Vector[T, N],
    {
        _ = pow(vec2(0),   vec2(1)  );
        _ = pow(vec2(0),   vec2(0.0));
        _ = pow(vec2(0),   vec2(1f) );
        _ = pow(vec2(0.0), vec2(0.0));
        _ = pow(vec2(0.0), vec2(0f) );
        _ = pow(vec2(1f),  vec2(1f) );
    }
    {
        _ = pow(vec3(0),   vec3(1)  );
        _ = pow(vec3(0),   vec3(0.0));
        _ = pow(vec3(0),   vec3(1f) );
        _ = pow(vec3(0.0), vec3(0.0));
        _ = pow(vec3(0.0), vec3(0f) );
        _ = pow(vec3(1f),  vec3(1f) );
    }
    {
        _ = pow(vec4(0),   vec4(1)  );
        _ = pow(vec4(0),   vec4(0.0));
        _ = pow(vec4(0),   vec4(1f) );
        _ = pow(vec4(0.0), vec4(0.0));
        _ = pow(vec4(0.0), vec4(0f) );
        _ = pow(vec4(1f),  vec4(1f) );
    }
}

// 17.5.45
fn testQuantizeToF16() {
    // [].(F32) => F32,
    {
        _ = quantizeToF16(0);
        _ = quantizeToF16(0.0);
        _ = quantizeToF16(0f);
    }

    // [N].(Vector[F32, N]) => Vector[F32, N],
    {
        _ = quantizeToF16(vec2(0));
        _ = quantizeToF16(vec2(0.0));
        _ = quantizeToF16(vec2(0f));
    }
    {
        _ = quantizeToF16(vec3(0));
        _ = quantizeToF16(vec3(0.0));
        _ = quantizeToF16(vec3(0f));
    }
    {
        _ = quantizeToF16(vec4(0));
        _ = quantizeToF16(vec4(0.0));
        _ = quantizeToF16(vec4(0f));
    }
}

// 17.5.46
fn testRadians()
{
    // [T < Float].(T) => T,
    {
        _ = radians(0);
        _ = radians(0.0);
        _ = radians(1f);
    }

    // [T < Float, N].(Vector[T, N]) => Vector[T, N],
    {
        _ = radians(vec2(0));
        _ = radians(vec2(0.0));
        _ = radians(vec2(1f));
    }
    {
        _ = radians(vec3(-1));
        _ = radians(vec3(-1.0));
        _ = radians(vec3(-1f));
    }
    {
        _ = radians(vec4(-1));
        _ = radians(vec4(-1.0));
        _ = radians(vec4(-1f));
    }
}

// 17.5.47
fn testReflect()
{
    // [T < Float, N].(Vector[T, N], Vector[T, N]) => Vector[T, N],
    {
        _ = reflect(vec2(0),   vec2(1)  );
        _ = reflect(vec2(0),   vec2(0.0));
        _ = reflect(vec2(0),   vec2(1f) );
        _ = reflect(vec2(0.0), vec2(0.0));
        _ = reflect(vec2(0.0), vec2(0f) );
        _ = reflect(vec2(1f),  vec2(1f) );
    }
    {
        _ = reflect(vec3(0),   vec3(1)  );
        _ = reflect(vec3(0),   vec3(0.0));
        _ = reflect(vec3(0),   vec3(1f) );
        _ = reflect(vec3(0.0), vec3(0.0));
        _ = reflect(vec3(0.0), vec3(0f) );
        _ = reflect(vec3(1f),  vec3(1f) );
    }
    {
        _ = reflect(vec4(0),   vec4(1)  );
        _ = reflect(vec4(0),   vec4(0.0));
        _ = reflect(vec4(0),   vec4(1f) );
        _ = reflect(vec4(0.0), vec4(0.0));
        _ = reflect(vec4(0.0), vec4(0f) );
        _ = reflect(vec4(1f),  vec4(1f) );
    }
}

// 17.5.48
fn testRefract()
{
    // [T < Float, N].(Vector[T, N], Vector[T, N], T) => Vector[T, N],
    {
        _ = refract(vec2(0), vec2(0), 1);
        _ = refract(vec2(0), vec2(0), 0.0);
        _ = refract(vec2(0), vec2(0), 1f);
    }
    {
        _ = refract(vec3(0), vec3(0), 1);
        _ = refract(vec3(0), vec3(0), 0.0);
        _ = refract(vec3(0), vec3(0), 1f);
    }
    {
        _ = refract(vec4(0), vec4(0), 1);
        _ = refract(vec4(0), vec4(0), 0.0);
        _ = refract(vec4(0), vec4(0), 1f);
    }
}

// 17.5.49
fn testReverseBits()
{
    // [T < ConcreteInteger].(T) => T,
    {
        _ = reverseBits(0);
        _ = reverseBits(0i);
        _ = reverseBits(0u);
    }
    // [T < ConcreteInteger, N].(Vector[T, N]) => Vector[T, N],
    {
        _ = reverseBits(vec2(0));
        _ = reverseBits(vec2(0i));
        _ = reverseBits(vec2(0u));
    }
    {
        _ = reverseBits(vec3(0));
        _ = reverseBits(vec3(0i));
        _ = reverseBits(vec3(0u));
    }
    {
        _ = reverseBits(vec4(0));
        _ = reverseBits(vec4(0i));
        _ = reverseBits(vec4(0u));
    }
}

// 17.5.50
fn testRound()
{
    // [T < Float].(T) => T,
    {
        _ = round(0);
        _ = round(0.0);
        _ = round(1f);
    }

    // [T < Float, N].(Vector[T, N]) => Vector[T, N],
    {
        _ = round(vec2(0));
        _ = round(vec2(0.0));
        _ = round(vec2(1f));
    }
    {
        _ = round(vec3(-1));
        _ = round(vec3(-1.0));
        _ = round(vec3(-1f));
    }
    {
        _ = round(vec4(-1));
        _ = round(vec4(-1.0));
        _ = round(vec4(-1f));
    }
}

// 17.5.51
fn testSaturate()
{
    // [T < Float].(T) => T,
    {
        _ = saturate(0);
        _ = saturate(0.0);
        _ = saturate(1f);
    }

    // [T < Float, N].(Vector[T, N]) => Vector[T, N],
    {
        _ = saturate(vec2(0));
        _ = saturate(vec2(0.0));
        _ = saturate(vec2(1f));
    }
    {
        _ = saturate(vec3(-1));
        _ = saturate(vec3(-1.0));
        _ = saturate(vec3(-1f));
    }
    {
        _ = saturate(vec4(-1));
        _ = saturate(vec4(-1.0));
        _ = saturate(vec4(-1f));
    }
}

// 17.5.52
fn testSign()
{
    // [T < SignedNumber].(T) => T,
    {
        _ = sign(0);
        _ = sign(0i);
        _ = sign(0.0);
        _ = sign(1f);
    }

    // [T < SignedNumber, N].(Vector[T, N]) => Vector[T, N],
    {
        _ = sign(vec2(0));
        _ = sign(vec2(0i));
        _ = sign(vec2(0.0));
        _ = sign(vec2(1f));
    }
    {
        _ = sign(vec3(-1));
        _ = sign(vec3(-1i));
        _ = sign(vec3(-1.0));
        _ = sign(vec3(-1f));
    }
    {
        _ = sign(vec4(-1));
        _ = sign(vec4(-1i));
        _ = sign(vec4(-1.0));
        _ = sign(vec4(-1f));
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
        _ = smoothstep(-1, 0, 1);
        _ = smoothstep(-1, 0, 1.0);
        _ = smoothstep(-1, 1, 2f);
    }
    // [T < Float, N].(Vector[T, N], Vector[T, N], Vector[T, N]) => Vector[T, N],
    {
        _ = smoothstep(vec2(0), vec2(0), vec2(0));
        _ = smoothstep(vec2(0), vec2(0), vec2(0.0));
        _ = smoothstep(vec2(0), vec2(0), vec2(0f));
    }
    {
        _ = smoothstep(vec3(0), vec3(0), vec3(0));
        _ = smoothstep(vec3(0), vec3(0), vec3(0.0));
        _ = smoothstep(vec3(0), vec3(0), vec3(0f));
    }
    {
        _ = smoothstep(vec4(0), vec4(0), vec4(0));
        _ = smoothstep(vec4(0), vec4(0), vec4(0.0));
        _ = smoothstep(vec4(0), vec4(0), vec4(0f));
    }
}

// 17.5.56
fn testSqrt()
{
    // [T < Float].(T) => T,
    {
        _ = sqrt(0);
        _ = sqrt(0.0);
        _ = sqrt(1f);
    }

    // [T < Float, N].(Vector[T, N]) => Vector[T, N],
    {
        _ = sqrt(vec2(0));
        _ = sqrt(vec2(0.0));
        _ = sqrt(vec2(1f));
    }
    {
        _ = sqrt(vec3(-1));
        _ = sqrt(vec3(-1.0));
        _ = sqrt(vec3(-1f));
    }
    {
        _ = sqrt(vec4(-1));
        _ = sqrt(vec4(-1.0));
        _ = sqrt(vec4(-1f));
    }
}

// 17.5.57
fn testStep()
{
    // [T < Float].(T, T) => T,
    {
        _ = step(0, 1);
        _ = step(0, 1.0);
        _ = step(1, 2f);
    }
    // [T < Float, N].(Vector[T, N], Vector[T, N], Vector[T, N]) => Vector[T, N],
    {
        _ = step(vec2(0), vec2(0));
        _ = step(vec2(0), vec2(0.0));
        _ = step(vec2(0), vec2(0f));
    }
    {
        _ = step(vec3(0), vec3(0));
        _ = step(vec3(0), vec3(0.0));
        _ = step(vec3(0), vec3(0f));
    }
    {
        _ = step(vec4(0), vec4(0));
        _ = step(vec4(0), vec4(0.0));
        _ = step(vec4(0), vec4(0f));
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
        _ = transpose(mat2x2(0, 0, 0, x));
        _ = transpose(mat2x3(0, 0, 0, 0, 0, x));
        _ = transpose(mat2x4(0, 0, 0, 0, 0, 0, 0, x));
        _ = transpose(mat3x2(0, 0, 0, 0, 0, x));
        _ = transpose(mat3x3(0, 0, 0, 0, 0, 0, 0, 0, x));
        _ = transpose(mat3x4(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, x));
        _ = transpose(mat4x2(0, 0, 0, 0, 0, 0, 0, x));
        _ = transpose(mat4x3(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, x));
        _ = transpose(mat4x4(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, x));
    }
    {
        const x = 1.0;
        _ = transpose(mat2x2(0, 0, 0, x));
        _ = transpose(mat2x3(0, 0, 0, 0, 0, x));
        _ = transpose(mat2x4(0, 0, 0, 0, 0, 0, 0, x));
        _ = transpose(mat3x2(0, 0, 0, 0, 0, x));
        _ = transpose(mat3x3(0, 0, 0, 0, 0, 0, 0, 0, x));
        _ = transpose(mat3x4(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, x));
        _ = transpose(mat4x2(0, 0, 0, 0, 0, 0, 0, x));
        _ = transpose(mat4x3(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, x));
        _ = transpose(mat4x4(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, x));
    }
    {
        const x = 1f;
        _ = transpose(mat2x2(0, 0, 0, x));
        _ = transpose(mat2x3(0, 0, 0, 0, 0, x));
        _ = transpose(mat2x4(0, 0, 0, 0, 0, 0, 0, x));
        _ = transpose(mat3x2(0, 0, 0, 0, 0, x));
        _ = transpose(mat3x3(0, 0, 0, 0, 0, 0, 0, 0, x));
        _ = transpose(mat3x4(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, x));
        _ = transpose(mat4x2(0, 0, 0, 0, 0, 0, 0, x));
        _ = transpose(mat4x3(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, x));
        _ = transpose(mat4x4(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, x));
    }
}

// 17.5.61
fn testTrunc()
{
    // [T < Float].(T) => T,
    {
        _ = trunc(0);
        _ = trunc(0.0);
        _ = trunc(1f);
    }

    // [T < Float, N].(Vector[T, N]) => Vector[T, N],
    {
        _ = trunc(vec2(0));
        _ = trunc(vec2(0.0));
        _ = trunc(vec2(1f));
    }
    {
        _ = trunc(vec3(-1));
        _ = trunc(vec3(-1.0));
        _ = trunc(vec3(-1f));
    }
    {
        _ = trunc(vec4(-1));
        _ = trunc(vec4(-1.0));
        _ = trunc(vec4(-1f));
    }
}
