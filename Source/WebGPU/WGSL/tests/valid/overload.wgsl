// RUN: %wgslc

// 8.6. Logical Expressions (https://gpuweb.github.io/gpuweb/wgsl/#logical-expr)

// RUN: %metal-compile testLogicalNegation
@compute @workgroup_size(1)
fn testLogicalNegation()
{
    var t = true;

    // [].(Bool) => Bool,
    { const x: bool = !true; }
    { const x : bool = !false; }
    { let x : bool = !t; }

    // [N].(Vector[Bool, N]) => Vector[Bool, N],
    { const x: vec2<bool> = !vec2(true); }
    { const x: vec3<bool> = !vec3(true); }
    { const x: vec4<bool> = !vec4(true); }

    { const x: vec2<bool> = !vec2(false); }
    { const x: vec3<bool> = !vec3(false); }
    { const x: vec4<bool> = !vec4(false); }

    { let x: vec2<bool> = !vec2(t); }
    { let x: vec3<bool> = !vec3(t); }
    { let x: vec4<bool> = !vec4(t); }
}

// RUN: %metal-compile testShortCircuitingOr
@compute @workgroup_size(1)
fn testShortCircuitingOr()
{
    var t = true;
    // [].(Bool, Bool) => Bool,
    { const x: bool = false || false; }
    { const x: bool = true || false; }
    { const x: bool = false || true; }
    { const x: bool = true || true; }
    { let x: bool = t || t; }
}

// RUN: %metal-compile testShortCircuitingAnd
@compute @workgroup_size(1)
fn testShortCircuitingAnd()
{
    var t = true;
    // [].(Bool, Bool) => Bool,
    { const x: bool = false && false; }
    { const x: bool = true && false; }
    { const x: bool = false && true; }
    { const x: bool = true && true; }
    { let x: bool = t && t; }
}

// RUN: %metal-compile testLogicalOr
@compute @workgroup_size(1)
fn testLogicalOr()
{
    var t = true;

    // [].(Bool, Bool) => Bool,
    { const x: bool = false | false; }
    { const x: bool = true | false; }
    { const x: bool = false | true; }
    { const x: bool = true | true; }
    { let x: bool = t | t; }

    // [N].(Vector[Bool, N], Vector[Bool, N]) => Vector[Bool, N],
    { const x: vec2<bool> = vec2(false) | vec2(false); }
    { const x: vec2<bool> = vec2( true) | vec2(false); }
    { const x: vec2<bool> = vec2(false) | vec2( true); }
    { const x: vec2<bool> = vec2( true) | vec2( true); }
    { const x: vec3<bool> = vec3(false) | vec3(false); }
    { const x: vec3<bool> = vec3( true) | vec3(false); }
    { const x: vec3<bool> = vec3(false) | vec3( true); }
    { const x: vec3<bool> = vec3( true) | vec3( true); }
    { const x: vec4<bool> = vec4(false) | vec4(false); }
    { const x: vec4<bool> = vec4( true) | vec4(false); }
    { const x: vec4<bool> = vec4(false) | vec4( true); }
    { const x: vec4<bool> = vec4( true) | vec4( true); }

    { let x: vec2<bool> = vec2(t) | vec2(t); }
    { let x: vec3<bool> = vec3(t) | vec3(t); }
    { let x: vec4<bool> = vec4(t) | vec4(t); }
}

// RUN: %metal-compile testLogicalAnd
@compute @workgroup_size(1)
fn testLogicalAnd()
{
    var t = true;

    // [].(Bool, Bool) => Bool,
    { const x: bool = false & false; }
    { const x: bool = true & false; }
    { const x: bool = false & true; }
    { const x: bool = true & true; }
    { let x: bool = t & t; }

    // [N].(Vector[Bool, N], Vector[Bool, N]) => Vector[Bool, N],
    { const x: vec2<bool> = vec2(false) & vec2(false); }
    { const x: vec2<bool> = vec2( true) & vec2(false); }
    { const x: vec2<bool> = vec2(false) & vec2( true); }
    { const x: vec2<bool> = vec2( true) & vec2( true); }
    { const x: vec3<bool> = vec3(false) & vec3(false); }
    { const x: vec3<bool> = vec3( true) & vec3(false); }
    { const x: vec3<bool> = vec3(false) & vec3( true); }
    { const x: vec3<bool> = vec3( true) & vec3( true); }
    { const x: vec4<bool> = vec4(false) & vec4(false); }
    { const x: vec4<bool> = vec4( true) & vec4(false); }
    { const x: vec4<bool> = vec4(false) & vec4( true); }
    { const x: vec4<bool> = vec4( true) & vec4( true); }

    { let x: vec2<bool> = vec2(t) & vec2(t); }
    { let x: vec3<bool> = vec3(t) & vec3(t); }
    { let x: vec4<bool> = vec4(t) & vec4(t); }
}

// 8.7. Arithmetic Expressions (https://www.w3.org/TR/WGSL/#arithmetic-expr)

// RUN: %metal-compile testUnaryMinus
@compute @workgroup_size(1)
fn testUnaryMinus()
{
    let x1: i32 = -1;
    let x2: i32 = -x1;
    let x3: f32 = -1;
    let x4: f16 = -1;
    let x5: i32 = -1i;
    let x6: f32 = -1f;
    let x7: f16 = -1h;
    _ = -x3;
    _ = -x4;

    let x8: vec2<i32> = -vec2(1);
    let x9: vec2<i32> = -x8;
    let x10: vec2<f32> = -vec2(1);
    let x11: vec2<f16> = -vec2(1);
    let x12: vec2<i32> = -vec2(1i);
    let x13: vec2<f32> = -vec2(1f);
    let x14: vec2<f16> = -vec2(1h);
    _ = -x10;
    _ = -x11;
}

// RUN: %metal-compile testBinaryMinus
@compute @workgroup_size(1)
fn testBinaryMinus()
{
    let i = 1i;
    let u = 1u;
    let f = 1f;
    let h = 1h;
    var v2i = vec2i(0);
    var v2u = vec2u(0);
    var v2f = vec2f(0);
    var v2h = vec2h(0);
    var m2f = mat2x2f(0, 0, 0, 0);
    var m2h = mat2x2h(0, 0, 0, 0);

    { let x: u32 = 2 - 1; }
    { let x: i32 = 2 - 1; }
    { let x: f32 = 2 - 1; }
    { let x: f16 = 2 - 1; }

    { let x: i32 = 1i - 1; }
    { let x: i32 = i - 1; }

    { let x: u32 = 1u - 1; }
    { let x: u32 = u - 1; }

    { let x: f32 = 1.0 - 2; }
    { let x: f16 = 1.0 - 2; }

    { let x: f32 = 2.0f - 1; }
    { let x: f32 = f - 1; }

    { let x: f16 = 2.0h - 1; }
    { let x: f16 = h - 1; }

    { let x: vec2<i32> = vec2(1, 1) - 1; }
    { let x: vec2<i32> = 1 - vec2(1, 1); }
    { let x: vec2<i32> = vec2(1, 1) - vec2(1, 1); }
    { let x: vec2<i32> = v2i - 1; }
    { let x: vec2<i32> = 1 - v2i; }
    { let x: vec2<i32> = v2i - v2i; }

    { let x: vec2<u32> = vec2(1, 1) - 1; }
    { let x: vec2<u32> = 1 - vec2(1, 1); }
    { let x: vec2<u32> = vec2(1, 1) - vec2(1, 1); }
    { let x: vec2<u32> = v2u - 1; }
    { let x: vec2<u32> = 1 - v2u; }
    { let x: vec2<u32> = v2u - v2u; }

    { let x: vec2<f32> = vec2(1, 1) - 1; }
    { let x: vec2<f32> = 1 - vec2(1, 1); }
    { let x: vec2<f32> = vec2(1, 1) - vec2(1, 1); }
    { let x: vec2<f32> = v2f - 1; }
    { let x: vec2<f32> = 1 - v2f; }
    { let x: vec2<f32> = v2f - v2f; }

    { let x: vec2<f16> = vec2(1, 1) - 1; }
    { let x: vec2<f16> = 1 - vec2(1, 1); }
    { let x: vec2<f16> = vec2(1, 1) - vec2(1, 1); }
    { let x: vec2<f16> = v2h - 1; }
    { let x: vec2<f16> = 1 - v2h; }
    { let x: vec2<f16> = v2h - v2h; }

    { let x: mat2x2<f32> = mat2x2(2, 2, 2, 2) - mat2x2(1, 1, 1, 1); }
    { let x: mat2x2<f32> = m2f - m2f; }
    { let x: mat2x2<f16> = mat2x2(2, 2, 2, 2) - mat2x2(1, 1, 1, 1); }
    { let x: mat2x2<f16> = m2h - m2h; }
}

// RUN: %metal-compile testAdd
@compute @workgroup_size(1)
fn testAdd()
{
    let i = 1i;
    let u = 1u;
    let f = 1f;
    let h = 1h;
    var v2i = vec2i(0);
    var v2u = vec2u(0);
    var v2f = vec2f(0);
    var v2h = vec2h(0);
    var m2f = mat2x2f(0, 0, 0, 0);
    var m2h = mat2x2h(0, 0, 0, 0);

    { let x: u32 = 2 + 1; }
    { let x: i32 = 2 + 1; }
    { let x: f32 = 2 + 1; }
    { let x: f16 = 2 + 1; }

    { let x: i32 = 1i + 1; }
    { let x: i32 = i + 1; }

    { let x: u32 = 1u + 1; }
    { let x: u32 = u + 1; }

    { let x: f32 = 1.0 + 2; }
    { let x: f16 = 1.0 + 2; }

    { let x: f32 = 2.0f + 1; }
    { let x: f32 = f + 1; }

    { let x: f16 = 2.0h + 1; }
    { let x: f16 = h + 1; }

    { let x: vec2<i32> = vec2(1, 1) + 1; }
    { let x: vec2<i32> = 1 + vec2(1, 1); }
    { let x: vec2<i32> = vec2(1, 1) + vec2(1, 1); }
    { let x: vec2<i32> = v2i + 1; }
    { let x: vec2<i32> = 1 + v2i; }
    { let x: vec2<i32> = v2i + v2i; }

    { let x: vec2<u32> = vec2(1, 1) + 1; }
    { let x: vec2<u32> = 1 + vec2(1, 1); }
    { let x: vec2<u32> = vec2(1, 1) + vec2(1, 1); }
    { let x: vec2<u32> = v2u + 1; }
    { let x: vec2<u32> = 1 + v2u; }
    { let x: vec2<u32> = v2u + v2u; }

    { let x: vec2<f32> = vec2(1, 1) + 1; }
    { let x: vec2<f32> = 1 + vec2(1, 1); }
    { let x: vec2<f32> = vec2(1, 1) + vec2(1, 1); }
    { let x: vec2<f32> = v2f + 1; }
    { let x: vec2<f32> = 1 + v2f; }
    { let x: vec2<f32> = v2f + v2f; }

    { let x: vec2<f16> = vec2(1, 1) + 1; }
    { let x: vec2<f16> = 1 + vec2(1, 1); }
    { let x: vec2<f16> = vec2(1, 1) + vec2(1, 1); }
    { let x: vec2<f16> = v2h + 1; }
    { let x: vec2<f16> = 1 + v2h; }
    { let x: vec2<f16> = v2h + v2h; }

    { let x: mat2x2<f32> = mat2x2(2, 2, 2, 2) + mat2x2(1, 1, 1, 1); }
    { let x: mat2x2<f32> = m2f + m2f; }
    { let x: mat2x2<f16> = mat2x2(2, 2, 2, 2) + mat2x2(1, 1, 1, 1); }
    { let x: mat2x2<f16> = m2h + m2h; }
}

// RUN: %metal-compile testAddEq
@compute @workgroup_size(1)
fn testAddEq() {
  {
    var x = 0;
    x += 1;
  }
}

// RUN: %metal-compile testMultiply
@compute @workgroup_size(1)
fn testMultiply()
{
    let i = 1i;
    let u = 1u;
    let f = 1f;
    let h = 1h;
    var v2i = vec2i(0);
    var v2u = vec2u(0);
    var v2f = vec2f(0);
    var v2h = vec2h(0);
    var v4f = vec4f(0);
    var v4h = vec4h(0);
    var m2f = mat2x2f(0, 0, 0, 0);
    var m2h = mat2x2h(0, 0, 0, 0);
    var m24f = mat2x4f(0, 0, 0, 0, 0, 0, 0, 0);
    var m24h = mat2x4h(0, 0, 0, 0, 0, 0, 0, 0);

    { let x: u32 = 2 * 1; }
    { let x: i32 = 2 * 1; }
    { let x: f32 = 2 * 1; }
    { let x: f16 = 2 * 1; }

    { let x: i32 = 1i * 1; }
    { let x: i32 = i * 1; }

    { let x: u32 = 1u * 1; }
    { let x: u32 = u * 1; }

    { let x: f32 = 1.0 * 2; }
    { let x: f16 = 1.0 * 2; }

    { let x: f32 = 2.0f * 1; }
    { let x: f32 = f * 1; }

    { let x: f16 = 2.0h * 1; }
    { let x: f16 = h * 1; }

    { let x: vec2<i32> = vec2(1, 1) * 1; }
    { let x: vec2<i32> = 1 * vec2(1, 1); }
    { let x: vec2<i32> = vec2(1, 1) * vec2(1, 1); }
    { let x: vec2<i32> = v2i * 1; }
    { let x: vec2<i32> = 1 * v2i; }
    { let x: vec2<i32> = v2i * v2i; }

    { let x: vec2<u32> = vec2(1, 1) * 1; }
    { let x: vec2<u32> = 1 * vec2(1, 1); }
    { let x: vec2<u32> = vec2(1, 1) * vec2(1, 1); }
    { let x: vec2<u32> = v2u * 1; }
    { let x: vec2<u32> = 1 * v2u; }
    { let x: vec2<u32> = v2u * v2u; }

    { let x: vec2<f32> = vec2(1, 1) * 1; }
    { let x: vec2<f32> = 1 * vec2(1, 1); }
    { let x: vec2<f32> = vec2(1, 1) * vec2(1, 1); }
    { let x: vec2<f32> = v2f * 1; }
    { let x: vec2<f32> = 1 * v2f; }
    { let x: vec2<f32> = v2f * v2f; }

    { let x: vec2<f16> = vec2(1, 1) * 1; }
    { let x: vec2<f16> = 1 * vec2(1, 1); }
    { let x: vec2<f16> = vec2(1, 1) * vec2(1, 1); }
    { let x: vec2<f16> = v2h * 1; }
    { let x: vec2<f16> = 1 * v2h; }
    { let x: vec2<f16> = v2h * v2h; }

    { let x: vec4f = mat2x4f(0, 0, 0, 0, 0, 0, 0, 0) * vec2f(0); }
    { let x: vec2f = vec4f(0) * mat2x4f(0, 0, 0, 0, 0, 0, 0, 0); }
    { let x: mat2x4f = mat2x4f(0, 0, 0, 0, 0, 0, 0, 0) * 2; }
    { let x: mat2x4f = 2 * mat2x4f(0, 0, 0, 0, 0, 0, 0, 0); };
    { let x: mat2x2f = mat2x2f(0, 0, 0, 0) * mat2x2f(0, 0, 0, 0); };
    { let x: mat2x4f = mat2x4f(0, 0, 0, 0, 0, 0, 0, 0) * mat2x2f(0, 0, 0, 0); };

    { let x: vec4f = m24f * v2f; }
    { let x: vec2f = v4f * m24f; }
    { let x: mat2x4f = m24f * 2; }
    { let x: mat2x4f = 2 * m24f; };
    { let x: mat2x2f = m2f * m2f; };
    { let x: mat2x4f = m24f * m2f; };

    { let x: vec4h = mat2x4h(0, 0, 0, 0, 0, 0, 0, 0) * vec2h(0); }
    { let x: vec2h = vec4h(0) * mat2x4h(0, 0, 0, 0, 0, 0, 0, 0); }
    { let x: mat2x4h = mat2x4h(0, 0, 0, 0, 0, 0, 0, 0) * 2; }
    { let x: mat2x4h = 2 * mat2x4h(0, 0, 0, 0, 0, 0, 0, 0); };
    { let x: mat2x2h = mat2x2h(0, 0, 0, 0) * mat2x2h(0, 0, 0, 0); };
    { let x: mat2x4h = mat2x4h(0, 0, 0, 0, 0, 0, 0, 0) * mat2x2h(0, 0, 0, 0); };

    { let x: vec4h = m24h * v2h; }
    { let x: vec2h = v4h * m24h; }
    { let x: mat2x4h = m24h * 2; }
    { let x: mat2x4h = 2 * m24h; };
    { let x: mat2x2h = m2h * m2h; };
    { let x: mat2x4h = m24h * m2h; };

    v2f *= v2f;
    v2f *= 2;
    m2f *= m2f;
    // FIXME: this requires type checking compound assignment
    // m2f *= 2;

    v2h *= v2h;
    v2h *= 2;
    m2h *= m2h;
    // hIXME: this requires type checking compound assignment
    // m2h *= 2;

}

// RUN: %metal-compile testDivision
@compute @workgroup_size(1)
fn testDivision()
{
    let i = 1i;
    let u = 1u;
    let f = 1f;
    let h = 1h;
    var v2i = vec2i(0);
    var v2u = vec2u(0);
    var v2f = vec2f(0);
    var v2h = vec2h(0);
    var v4f = vec4f(0);
    var v4h = vec4h(0);

    { let x: u32 = 2 / 1; }
    { let x: i32 = 2 / 1; }
    { let x: f32 = 2 / 1; }
    { let x: f16 = 2 / 1; }

    { let x: i32 = 1i / 1; }
    { let x: i32 = i / 1; }

    { let x: u32 = 1u / 1; }
    { let x: u32 = u / 1; }

    { let x: f32 = 1.0 / 2; }
    { let x: f16 = 1.0 / 2; }

    { let x: f32 = 2.0f / 1; }
    { let x: f32 = f / 1; }

    { let x: f16 = 2.0h / 1; }
    { let x: f16 = h / 1; }

    { let x: vec2<i32> = vec2(1, 1) / 1; }
    { let x: vec2<i32> = 1 / vec2(1, 1); }
    { let x: vec2<i32> = vec2(1, 1) / vec2(1, 1); }
    { let x: vec2<i32> = v2i / 1; }
    { let x: vec2<i32> = 1 / v2i; }
    { let x: vec2<i32> = v2i / v2i; }

    { let x: vec2<u32> = vec2(1, 1) / 1; }
    { let x: vec2<u32> = 1 / vec2(1, 1); }
    { let x: vec2<u32> = vec2(1, 1) / vec2(1, 1); }
    { let x: vec2<u32> = v2u / 1; }
    { let x: vec2<u32> = 1 / v2u; }
    { let x: vec2<u32> = v2u / v2u; }

    { let x: vec2<f32> = vec2(1, 1) / 1; }
    { let x: vec2<f32> = 1 / vec2(1, 1); }
    { let x: vec2<f32> = vec2(1, 1) / vec2(1, 1); }
    { let x: vec2<f32> = v2f / 1; }
    { let x: vec2<f32> = 1 / v2f; }
    { let x: vec2<f32> = v2f / v2f; }

    { let x: vec2<f16> = vec2(1, 1) / 1; }
    { let x: vec2<f16> = 1 / vec2(1, 1); }
    { let x: vec2<f16> = vec2(1, 1) / vec2(1, 1); }
    { let x: vec2<f16> = v2h / 1; }
    { let x: vec2<f16> = 1 / v2h; }
    { let x: vec2<f16> = v2h / v2h; }
}

// RUN: %metal-compile testModulo
@compute @workgroup_size(1)
fn testModulo()
{
    let i = 1i;
    let u = 1u;
    let f = 1f;
    let h = 1h;
    var v2i = vec2i(0);
    var v2u = vec2u(0);
    var v2f = vec2f(0);
    var v2h = vec2h(0);
    var v4f = vec4f(0);
    var v4h = vec4h(0);

    { let x: u32 = 2 % 1; }
    { let x: i32 = 2 % 1; }
    { let x: f32 = 2 % 1; }
    { let x: f16 = 2 % 1; }

    { let x: i32 = 1i % 1; }
    { let x: i32 = i % 1; }

    { let x: u32 = 1u % 1; }
    { let x: u32 = u % 1; }

    { let x: f32 = 1.0 % 2; }
    { let x: f16 = 1.0 % 2; }

    { let x: f32 = 2.0f % 1; }
    { let x: f32 = f % 1; }

    { let x: f16 = 2.0h % 1; }
    { let x: f16 = h % 1; }

    { let x: vec2<i32> = vec2(1, 1) % 1; }
    { let x: vec2<i32> = 1 % vec2(1, 1); }
    { let x: vec2<i32> = vec2(1, 1) % vec2(1, 1); }
    { let x: vec2<i32> = v2i % 1; }
    { let x: vec2<i32> = 1 % v2i; }
    { let x: vec2<i32> = v2i % v2i; }

    { let x: vec2<u32> = vec2(1, 1) % 1; }
    { let x: vec2<u32> = 1 % vec2(1, 1); }
    { let x: vec2<u32> = vec2(1, 1) % vec2(1, 1); }
    { let x: vec2<u32> = v2u % 1; }
    { let x: vec2<u32> = 1 % v2u; }
    { let x: vec2<u32> = v2u % v2u; }

    { let x: vec2<f32> = vec2(1, 1) % 1; }
    { let x: vec2<f32> = 1 % vec2(1, 1); }
    { let x: vec2<f32> = vec2(1, 1) % vec2(1, 1); }
    { let x: vec2<f32> = v2f % 1; }
    { let x: vec2<f32> = 1 % v2f; }
    { let x: vec2<f32> = v2f % v2f; }

    { let x: vec2<f16> = vec2(1, 1) % 1; }
    { let x: vec2<f16> = 1 % vec2(1, 1); }
    { let x: vec2<f16> = vec2(1, 1) % vec2(1, 1); }
    { let x: vec2<f16> = v2h % 1; }
    { let x: vec2<f16> = 1 % v2h; }
    { let x: vec2<f16> = v2h % v2h; }
}

// 8.8. Comparison Expressions (https://www.w3.org/TR/WGSL/#comparison-expr)

// RUN: %metal-compile testComparison
@compute @workgroup_size(1)
fn testComparison()
{
    let b = false;
    let i = 1i;
    let u = 1u;
    let f = 1f;
    let h = 1h;
    var v2i = vec2i(0);
    var v2u = vec2u(0);
    var v2f = vec2f(0);
    var v2h = vec2h(0);
    var v2b = vec2(true);

    {
        _ = false == true;
        _ = b == true;
        _ = 0 == 1;
        _ = 0i == 1i;
        _ = i == 1;
        _ = 0u == 1u;
        _ = u == 1;
        _ = 0.0 == 1.0;
        _ = 0.0f == 1.0f;
        _ = f == 1.0;
        _ = 0.0h == 1.0h;
        _ = h == 1.0;
        _ = vec2(false) == vec2(true);
        _ = v2b == vec2(true);
        _ = vec2(0) == vec2(1);
        _ = vec2(0i) == vec2(1i);
        _ = v2i == vec2(1);
        _ = vec2(0u) == vec2(1u);
        _ = v2u == vec2(1);
        _ = vec2(0.0) == vec2(1.0);
        _ = vec2(0.0f) == vec2(1.0f);
        _ = v2f == vec2(1.0);
        _ = vec2(0.0h) == vec2(1.0h);
        _ = v2h == vec2(1.0);
    }

    {
        _ = false != true;
        _ = b != true;
        _ = 0 != 1;
        _ = 0i != 1i;
        _ = i != 1;
        _ = 0u != 1u;
        _ = u != 1;
        _ = 0.0 != 1.0;
        _ = 0.0f != 1.0f;
        _ = f != 1.0;
        _ = 0.0h != 1.0h;
        _ = h != 1.0;
        _ = vec2(false) != vec2(true);
        _ = v2b != vec2(true);
        _ = vec2(0) != vec2(1);
        _ = vec2(0i) != vec2(1i);
        _ = v2i != vec2(1);
        _ = vec2(0u) != vec2(1u);
        _ = v2u != vec2(1);
        _ = vec2(0.0) != vec2(1.0);
        _ = vec2(0.0f) != vec2(1.0f);
        _ = v2f != vec2(1.0);
        _ = vec2(0.0h) != vec2(1.0h);
        _ = v2h != vec2(1.0);
    }

    {
        _ = 0 > 1;
        _ = 0i > 1i;
        _ = i > 1;
        _ = 0u > 1u;
        _ = u > 1;
        _ = 0.0 > 1.0;
        _ = 0.0f > 1.0f;
        _ = f > 1.0;
        _ = 0.0h > 1.0h;
        _ = h > 1.0;
        _ = vec2(0) > vec2(1);
        _ = vec2(0i) > vec2(1i);
        _ = v2i > vec2(1);
        _ = vec2(0u) > vec2(1u);
        _ = v2u > vec2(1);
        _ = vec2(0.0) > vec2(1.0);
        _ = vec2(0.0f) > vec2(1.0f);
        _ = v2f > vec2(1.0);
        _ = vec2(0.0h) > vec2(1.0h);
        _ = v2h > vec2(1.0);
    }

    {
        _ = 0 >= 1;
        _ = 0i >= 1i;
        _ = i >= 1;
        _ = 0u >= 1u;
        _ = u >= 1;
        _ = 0.0 >= 1.0;
        _ = 0.0f >= 1.0f;
        _ = f >= 1.0;
        _ = 0.0h >= 1.0h;
        _ = h >= 1.0;
        _ = vec2(0) >= vec2(1);
        _ = vec2(0i) >= vec2(1i);
        _ = v2i >= vec2(1);
        _ = vec2(0u) >= vec2(1u);
        _ = v2u >= vec2(1);
        _ = vec2(0.0) >= vec2(1.0);
        _ = vec2(0.0f) >= vec2(1.0f);
        _ = v2f >= vec2(1.0);
        _ = vec2(0.0h) >= vec2(1.0h);
        _ = v2h >= vec2(1.0);
    }

    {
        _ = 0 < 1;
        _ = 0i < 1i;
        _ = i < 1;
        _ = 0u < 1u;
        _ = u < 1;
        _ = 0.0 < 1.0;
        _ = 0.0f < 1.0f;
        _ = f < 1.0;
        _ = 0.0h < 1.0h;
        _ = h < 1.0;
        _ = vec2(0) < vec2(1);
        _ = vec2(0i) < vec2(1i);
        _ = v2i < vec2(1);
        _ = vec2(0u) < vec2(1u);
        _ = v2u < vec2(1);
        _ = vec2(0.0) < vec2(1.0);
        _ = vec2(0.0f) < vec2(1.0f);
        _ = v2f < vec2(1.0);
        _ = vec2(0.0h) < vec2(1.0h);
        _ = v2h < vec2(1.0);
    }

    {
        _ = 0 <= 1;
        _ = 0i <= 1i;
        _ = i <= 1;
        _ = 0u <= 1u;
        _ = u <= 1;
        _ = 0.0 <= 1.0;
        _ = 0.0f <= 1.0f;
        _ = f <= 1.0;
        _ = 0.0h <= 1.0h;
        _ = h <= 1.0;
        _ = vec2(0) <= vec2(1);
        _ = vec2(0i) <= vec2(1i);
        _ = v2i <= vec2(1);
        _ = vec2(0u) <= vec2(1u);
        _ = v2u <= vec2(1);
        _ = vec2(0.0) <= vec2(1.0);
        _ = vec2(0.0f) <= vec2(1.0f);
        _ = v2f <= vec2(1.0);
        _ = vec2(0.0h) <= vec2(1.0h);
        _ = v2h <= vec2(1.0);
    }
}

// 8.9. Bit Expressions (https://www.w3.org/TR/WGSL/#bit-expr)

// RUN: %metal-compile testBitwise
@compute @workgroup_size(1)
fn testBitwise()
{
    {
        var i = 0i;
        var u = 0u;
        const x1: u32 = ~(-1);
        const x2: i32 = ~1i;
        const x3: u32 = ~1u;
        const x4: vec2<u32> = ~vec2(-1);
        const x5: vec2<i32> = ~vec2(0i);
        const x6: vec2<u32> = ~vec2(0u);
        let x7: i32 = ~i;
        let x8: u32 = ~u;
    }

    {
        var i = 0i;
        var u = 0u;
        const x1: u32 = 0 & 1;
        const x2: i32 = 0i & 1i;
        const x3: u32 = 0u & 1u;
        const x4: vec2<u32> = vec2(0) & vec2(1);
        const x5: vec2<i32> = vec2(0i) & vec2(1i);
        const x6: vec2<u32> = vec2(0u) & vec2(1u);
        let x7: i32 = i & 1;
        let x8: u32 = u & 1;
        i &= 1;
        u &= 1;
    }

    {
        var i = 0i;
        var u = 0u;
        const x1: u32 = 0 | 1;
        const x2: i32 = 0i | 1i;
        const x3: u32 = 0u | 1u;
        const x4: vec2<u32> = vec2(0) | vec2(1);
        const x5: vec2<i32> = vec2(0i) | vec2(1);
        const x6: vec2<u32> = vec2(0u) | vec2(1);
        let x7: i32 = i | 1;
        let x8: u32 = u | 1;
        i |= 1;
        u |= 1;
    }

    {
        var i = 0i;
        var u = 0u;
        const x1: u32 = 0 ^ 1;
        const x2: i32 = 0i ^ 1i;
        const x3: u32 = 0u ^ 1u;
        const x4: vec2<u32> = vec2(0) ^ vec2(1);
        const x5: vec2<i32> = vec2(0i) ^ vec2(1i);
        const x6: vec2<u32> = vec2(0u) ^ vec2(1u);
        let x7: i32 = i ^ 1;
        let x8: u32 = u ^ 1;
        i ^= 1;
        u ^= 1;
    }

    {
        var i = 0i;
        var u = 0u;
        const x1: u32 = 1 << 2;
        const x2: i32 = 1i << 2u;
        const x3: u32 = 1u << 2u;
        const x4: vec2<u32> = vec2(1) << vec2(2);
        const x5: vec2<i32> = vec2(1i) << vec2(2u);
        const x6: vec2<u32> = vec2(1u) << vec2(2u);
        let x7: i32 = i << 1;
        let x8: u32 = u << 1;
        i <<= 1;
        u <<= 1;
    }

    {
        var i = 0i;
        var u = 0u;
        const x: u32 = 1 >> 2;
        const x2: i32 = 1i >> 2u;
        const x3: u32 = 1u >> 2u;
        const x4: vec2<u32> = vec2(1) >> vec2(2);
        const x5: vec2<i32> = vec2(1i) >> vec2(2u);
        const x6: vec2<u32> = vec2(1u) >> vec2(2u);
        let x7: i32 = i >> 1;
        let x8: u32 = u >> 1;
        i >>= 1;
        u >>= 1;
    }
}

// 8.13. Address-Of Expression (https://www.w3.org/TR/WGSL/#address-of-expr)
// RUN: %metal-compile testAddressOf
@compute @workgroup_size(1)
fn testAddressOf()
{
    var x = 1;
    testPointerDeference(&x);

    let y: ptr<function, i32> = &x;
    testPointerDeference(y);

    let z = &x;
    testPointerDeference(z);
}

// 8.14. Indirection Expression (https://www.w3.org/TR/WGSL/#indirection-expr)

fn testPointerDeference(x: ptr<function, i32>) -> i32
{
    return *x;
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
    _ = f16();

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
// RUN: %metal-compile testArray
@compute @workgroup_size(1)
fn testArray()
{
    let b = false;
    let i = 0i;
    let u = 0u;
    let f = 0f;
    let h = 0h;

    { const x: array<bool, 1> = array<bool, 1>(false); }
    { const x: array<f32, 1> = array<f32, 1>(0); }
    { const x: array<f16, 1> = array<f16, 1>(0); }
    { const x: array<i32, 1> = array<i32, 1>(0); }
    { const x: array<u32, 1> = array<u32, 1>(0); }
    { const x : array<S, 2> = array<S, 2>(S(0), S(1)); }

    { let x : array<bool, 1> = array<bool, 1>(b); }
    { let x : array<f32, 1> = array<f32, 1>(f); }
    { let x : array<f16, 1> = array<f16, 1>(h); }
    { let x : array<i32, 1> = array<i32, 1>(i); }
    { let x : array<u32, 1> = array<u32, 1>(u); }
    { let x : array<S, 2> = array<S, 2>(S(i), S(i)); }
    { _ = array<S, 2>(S(i), S(i)); }


    var x1 = 0;
    let x2 = array(x1, 0, 0i, x1);
}

// 16.1.2.2.
// RUN: %metal-compile testBool
@compute @workgroup_size(1)
fn testBool()
{
    let b = false;
    let i = 0i;
    let u = 0u;
    let f = 0f;
    let h = 0h;

    _ = bool(true);
    _ = bool(0);
    _ = bool(0i);
    _ = bool(0u);
    _ = bool(0.0);
    _ = bool(0f);
    _ = bool(0h);
    _ = bool(b);
    _ = bool(i);
    _ = bool(u);
    _ = bool(f);
    _ = bool(h);
}

// 16.1.2.3.
// RUN: %metal-compile testF16
@compute @workgroup_size(1)
fn testF16()
{
    let b = false;
    let i = 0i;
    let u = 0u;
    let f = 0f;
    let h = 0h;

    _ = f16(true);
    _ = f16(0);
    _ = f16(0i);
    _ = f16(0u);
    _ = f16(0.0);
    _ = f16(0f);
    _ = f16(0h);
    _ = f16(b);
    _ = f16(i);
    _ = f16(u);
    _ = f16(f);
    _ = f16(h);
}

// 16.1.2.4.
// RUN: %metal-compile testF32
@compute @workgroup_size(1)
fn testF32()
{
    let b = false;
    let i = 0i;
    let u = 0u;
    let f = 0f;
    let h = 0h;

    _ = f32(true);
    _ = f32(0);
    _ = f32(0i);
    _ = f32(0u);
    _ = f32(0.0);
    _ = f32(0f);
    _ = f32(0h);
    _ = f32(b);
    _ = f32(i);
    _ = f32(u);
    _ = f32(f);
    _ = f32(h);
}

// 16.1.2.5.
// RUN: %metal-compile testI32
@compute @workgroup_size(1)
fn testI32()
{
    let b = false;
    let i = 0i;
    let u = 0u;
    let f = 0f;
    let h = 0h;

    _ = i32(true);
    _ = i32(0);
    _ = i32(0i);
    _ = i32(0u);
    _ = i32(0.0);
    _ = i32(0f);
    _ = i32(0h);
    _ = i32(4294967295u);
    _ = i32(b);
    _ = i32(i);
    _ = i32(u);
    _ = i32(f);
    _ = i32(h);
}

// 16.1.2.6 - 14: matCxR
// RUN: %metal-compile testMatrix
@compute @workgroup_size(1)
fn testMatrix()
{
    let f = 0f;
    let h = 0h;
    {
        _ = mat2x2(0.0, 0.0, 0.0, 0.0);
        _ = mat2x2(0.0f, 0.0, 0.0, 0.0);
        _ = mat2x2(0.0h, 0.0, 0.0, 0.0);
        _ = mat2x2(f, 0.0, 0.0, 0.0);
        _ = mat2x2(h, 0.0, 0.0, 0.0);

        _ = mat2x2<f32>(mat2x2(0.0, 0.0, 0.0, 0.0));
        _ = mat2x2<f32>(mat2x2(0.0f, 0.0, 0.0, 0.0));
        _ = mat2x2<f32>(mat2x2(0.0h, 0.0, 0.0, 0.0));
        _ = mat2x2<f32>(mat2x2(f, 0.0, 0.0, 0.0));
        _ = mat2x2<f32>(mat2x2(h, 0.0, 0.0, 0.0));

        _ = mat2x2<f16>(mat2x2(0.0, 0.0, 0.0, 0.0));
        _ = mat2x2<f16>(mat2x2(0.0f, 0.0, 0.0, 0.0));
        _ = mat2x2<f16>(mat2x2(0.0h, 0.0, 0.0, 0.0));
        _ = mat2x2<f16>(mat2x2(f, 0.0, 0.0, 0.0));
        _ = mat2x2<f16>(mat2x2(h, 0.0, 0.0, 0.0));


        _ = mat2x2(mat2x2(0.0, 0.0, 0.0, 0.0));
        _ = mat2x2(mat2x2(0.0f, 0.0, 0.0, 0.0));
        _ = mat2x2(mat2x2(0.0h, 0.0, 0.0, 0.0));
        _ = mat2x2(mat2x2(f, 0.0, 0.0, 0.0));
        _ = mat2x2(mat2x2(h, 0.0, 0.0, 0.0));

        _ = mat2x2(vec2(0.0, 0.0), vec2(0.0, 0.0));
        _ = mat2x2(vec2(0.0f, 0.0), vec2(0.0, 0.0));
        _ = mat2x2(vec2(0.0h, 0.0), vec2(0.0, 0.0));
        _ = mat2x2(vec2(f, 0.0), vec2(0.0, 0.0));
        _ = mat2x2(vec2(h, 0.0), vec2(0.0, 0.0));
    }

    {
        _ = mat2x3(0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
        _ = mat2x3(0.0f, 0.0, 0.0, 0.0, 0.0, 0.0);
        _ = mat2x3(0.0h, 0.0, 0.0, 0.0, 0.0, 0.0);
        _ = mat2x3(f, 0.0, 0.0, 0.0, 0.0, 0.0);
        _ = mat2x3(h, 0.0, 0.0, 0.0, 0.0, 0.0);

        _ = mat2x3<f32>(mat2x3(0.0, 0.0, 0.0, 0.0, 0.0, 0.0));
        _ = mat2x3<f32>(mat2x3(0.0f, 0.0, 0.0, 0.0, 0.0, 0.0));
        _ = mat2x3<f32>(mat2x3(0.0h, 0.0, 0.0, 0.0, 0.0, 0.0));
        _ = mat2x3<f32>(mat2x3(f, 0.0, 0.0, 0.0, 0.0, 0.0));
        _ = mat2x3<f32>(mat2x3(h, 0.0, 0.0, 0.0, 0.0, 0.0));

        _ = mat2x3<f16>(mat2x3(0.0, 0.0, 0.0, 0.0, 0.0, 0.0));
        _ = mat2x3<f16>(mat2x3(0.0f, 0.0, 0.0, 0.0, 0.0, 0.0));
        _ = mat2x3<f16>(mat2x3(0.0h, 0.0, 0.0, 0.0, 0.0, 0.0));
        _ = mat2x3<f16>(mat2x3(f, 0.0, 0.0, 0.0, 0.0, 0.0));
        _ = mat2x3<f16>(mat2x3(h, 0.0, 0.0, 0.0, 0.0, 0.0));


        _ = mat2x3(mat2x3(0.0, 0.0, 0.0, 0.0, 0.0, 0.0));
        _ = mat2x3(mat2x3(0.0f, 0.0, 0.0, 0.0, 0.0, 0.0));
        _ = mat2x3(mat2x3(0.0h, 0.0, 0.0, 0.0, 0.0, 0.0));
        _ = mat2x3(mat2x3(f, 0.0, 0.0, 0.0, 0.0, 0.0));
        _ = mat2x3(mat2x3(h, 0.0, 0.0, 0.0, 0.0, 0.0));

        _ = mat2x3(vec3(0.0), vec3(0.0));
        _ = mat2x3(vec3(0.0f), vec3(0.0));
        _ = mat2x3(vec3(0.0h), vec3(0.0));
        _ = mat2x3(vec3(f), vec3(0.0));
        _ = mat2x3(vec3(h), vec3(0.0));
    }

    {
        _ = mat2x4(0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
        _ = mat2x4(0.0f, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
        _ = mat2x4(0.0h, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
        _ = mat2x4(f, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
        _ = mat2x4(h, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0);

        _ = mat2x4<f32>(mat2x4(0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0));
        _ = mat2x4<f32>(mat2x4(0.0f, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0));
        _ = mat2x4<f32>(mat2x4(0.0h, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0));
        _ = mat2x4<f32>(mat2x4(f, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0));
        _ = mat2x4<f32>(mat2x4(h, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0));

        _ = mat2x4<f16>(mat2x4(0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0));
        _ = mat2x4<f16>(mat2x4(0.0f, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0));
        _ = mat2x4<f16>(mat2x4(0.0h, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0));
        _ = mat2x4<f16>(mat2x4(f, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0));
        _ = mat2x4<f16>(mat2x4(h, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0));


        _ = mat2x4(mat2x4(0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0));
        _ = mat2x4(mat2x4(0.0f, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0));
        _ = mat2x4(mat2x4(0.0h, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0));
        _ = mat2x4(mat2x4(f, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0));
        _ = mat2x4(mat2x4(h, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0));

        _ = mat2x4(vec4(0.0), vec4(0.0));
        _ = mat2x4(vec4(0.0f), vec4(0.0));
        _ = mat2x4(vec4(0.0h), vec4(0.0));
        _ = mat2x4(vec4(f), vec4(0.0));
        _ = mat2x4(vec4(h), vec4(0.0));
    }

    {
        _ = mat3x2(0.0, 0.0, 0.0, 0.0, 0, 0);
        _ = mat3x2(0.0f, 0.0, 0.0, 0.0, 0, 0);
        _ = mat3x2(0.0h, 0.0, 0.0, 0.0, 0, 0);
        _ = mat3x2(f, 0.0, 0.0, 0.0, 0, 0);
        _ = mat3x2(h, 0.0, 0.0, 0.0, 0, 0);

        _ = mat3x2<f32>(mat3x2(0.0, 0.0, 0.0, 0.0, 0, 0));
        _ = mat3x2<f32>(mat3x2(0.0f, 0.0, 0.0, 0.0, 0, 0));
        _ = mat3x2<f32>(mat3x2(0.0h, 0.0, 0.0, 0.0, 0, 0));
        _ = mat3x2<f32>(mat3x2(f, 0.0, 0.0, 0.0, 0, 0));
        _ = mat3x2<f32>(mat3x2(h, 0.0, 0.0, 0.0, 0, 0));

        _ = mat3x2<f16>(mat3x2(0.0, 0.0, 0.0, 0.0, 0, 0));
        _ = mat3x2<f16>(mat3x2(0.0f, 0.0, 0.0, 0.0, 0, 0));
        _ = mat3x2<f16>(mat3x2(0.0h, 0.0, 0.0, 0.0, 0, 0));
        _ = mat3x2<f16>(mat3x2(f, 0.0, 0.0, 0.0, 0, 0));
        _ = mat3x2<f16>(mat3x2(h, 0.0, 0.0, 0.0, 0, 0));


        _ = mat3x2(mat3x2(0.0, 0.0, 0.0, 0.0, 0, 0));
        _ = mat3x2(mat3x2(0.0f, 0.0, 0.0, 0.0, 0, 0));
        _ = mat3x2(mat3x2(0.0h, 0.0, 0.0, 0.0, 0, 0));
        _ = mat3x2(mat3x2(f, 0.0, 0.0, 0.0, 0, 0));
        _ = mat3x2(mat3x2(h, 0.0, 0.0, 0.0, 0, 0));

        _ = mat3x2(vec2(0.0, 0.0), vec2(0.0, 0.0), vec2(0));
        _ = mat3x2(vec2(0.0f, 0.0), vec2(0.0, 0.0), vec2(0));
        _ = mat3x2(vec2(0.0h, 0.0), vec2(0.0, 0.0), vec2(0));
        _ = mat3x2(vec2(f, 0.0), vec2(0.0, 0.0), vec2(0));
        _ = mat3x2(vec2(h, 0.0), vec2(0.0, 0.0), vec2(0));
    }

    {
        _ = mat3x3(0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0, 0, 0);
        _ = mat3x3(0.0f, 0.0, 0.0, 0.0, 0.0, 0.0, 0, 0, 0);
        _ = mat3x3(0.0h, 0.0, 0.0, 0.0, 0.0, 0.0, 0, 0, 0);
        _ = mat3x3(f, 0.0, 0.0, 0.0, 0.0, 0.0, 0, 0, 0);
        _ = mat3x3(h, 0.0, 0.0, 0.0, 0.0, 0.0, 0, 0, 0);

        _ = mat3x3<f32>(mat3x3(0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0, 0, 0));
        _ = mat3x3<f32>(mat3x3(0.0f, 0.0, 0.0, 0.0, 0.0, 0.0, 0, 0, 0));
        _ = mat3x3<f32>(mat3x3(0.0h, 0.0, 0.0, 0.0, 0.0, 0.0, 0, 0, 0));
        _ = mat3x3<f32>(mat3x3(f, 0.0, 0.0, 0.0, 0.0, 0.0, 0, 0, 0));
        _ = mat3x3<f32>(mat3x3(h, 0.0, 0.0, 0.0, 0.0, 0.0, 0, 0, 0));

        _ = mat3x3<f16>(mat3x3(0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0, 0, 0));
        _ = mat3x3<f16>(mat3x3(0.0f, 0.0, 0.0, 0.0, 0.0, 0.0, 0, 0, 0));
        _ = mat3x3<f16>(mat3x3(0.0h, 0.0, 0.0, 0.0, 0.0, 0.0, 0, 0, 0));
        _ = mat3x3<f16>(mat3x3(f, 0.0, 0.0, 0.0, 0.0, 0.0, 0, 0, 0));
        _ = mat3x3<f16>(mat3x3(h, 0.0, 0.0, 0.0, 0.0, 0.0, 0, 0, 0));


        _ = mat3x3(mat3x3(0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0, 0, 0));
        _ = mat3x3(mat3x3(0.0f, 0.0, 0.0, 0.0, 0.0, 0.0, 0, 0, 0));
        _ = mat3x3(mat3x3(0.0h, 0.0, 0.0, 0.0, 0.0, 0.0, 0, 0, 0));
        _ = mat3x3(mat3x3(f, 0.0, 0.0, 0.0, 0.0, 0.0, 0, 0, 0));
        _ = mat3x3(mat3x3(h, 0.0, 0.0, 0.0, 0.0, 0.0, 0, 0, 0));

        _ = mat3x3(vec3(0.0), vec3(0.0), vec3(0));
        _ = mat3x3(vec3(0.0f), vec3(0.0), vec3(0));
        _ = mat3x3(vec3(0.0h), vec3(0.0), vec3(0));
        _ = mat3x3(vec3(f), vec3(0.0), vec3(0));
        _ = mat3x3(vec3(h), vec3(0.0), vec3(0));
    }

    {
        _ = mat3x4(0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0, 0, 0, 0);
        _ = mat3x4(0.0f, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0, 0, 0, 0);
        _ = mat3x4(0.0h, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0, 0, 0, 0);
        _ = mat3x4(f, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0, 0, 0, 0);
        _ = mat3x4(h, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0, 0, 0, 0);

        _ = mat3x4<f32>(mat3x4(0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0, 0, 0, 0));
        _ = mat3x4<f32>(mat3x4(0.0f, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0, 0, 0, 0));
        _ = mat3x4<f32>(mat3x4(0.0h, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0, 0, 0, 0));
        _ = mat3x4<f32>(mat3x4(f, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0, 0, 0, 0));
        _ = mat3x4<f32>(mat3x4(h, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0, 0, 0, 0));

        _ = mat3x4<f16>(mat3x4(0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0, 0, 0, 0));
        _ = mat3x4<f16>(mat3x4(0.0f, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0, 0, 0, 0));
        _ = mat3x4<f16>(mat3x4(0.0h, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0, 0, 0, 0));
        _ = mat3x4<f16>(mat3x4(f, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0, 0, 0, 0));
        _ = mat3x4<f16>(mat3x4(h, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0, 0, 0, 0));


        _ = mat3x4(mat3x4(0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0, 0, 0, 0));
        _ = mat3x4(mat3x4(0.0f, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0, 0, 0, 0));
        _ = mat3x4(mat3x4(0.0h, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0, 0, 0, 0));
        _ = mat3x4(mat3x4(f, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0, 0, 0, 0));
        _ = mat3x4(mat3x4(h, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0, 0, 0, 0));

        _ = mat3x4(vec4(0.0), vec4(0.0), vec4(0));
        _ = mat3x4(vec4(0.0f), vec4(0.0), vec4(0));
        _ = mat3x4(vec4(0.0h), vec4(0.0), vec4(0));
        _ = mat3x4(vec4(f), vec4(0.0), vec4(0));
        _ = mat3x4(vec4(h), vec4(0.0), vec4(0));
    }

    {
        _ = mat4x2(0.0, 0.0, 0.0, 0.0, 0, 0, 0, 0);
        _ = mat4x2(0.0f, 0.0, 0.0, 0.0, 0, 0, 0, 0);
        _ = mat4x2(0.0h, 0.0, 0.0, 0.0, 0, 0, 0, 0);
        _ = mat4x2(f, 0.0, 0.0, 0.0, 0, 0, 0, 0);
        _ = mat4x2(h, 0.0, 0.0, 0.0, 0, 0, 0, 0);

        _ = mat4x2<f32>(mat4x2(0.0, 0.0, 0.0, 0.0, 0, 0, 0, 0));
        _ = mat4x2<f32>(mat4x2(0.0f, 0.0, 0.0, 0.0, 0, 0, 0, 0));
        _ = mat4x2<f32>(mat4x2(0.0h, 0.0, 0.0, 0.0, 0, 0, 0, 0));
        _ = mat4x2<f32>(mat4x2(f, 0.0, 0.0, 0.0, 0, 0, 0, 0));
        _ = mat4x2<f32>(mat4x2(h, 0.0, 0.0, 0.0, 0, 0, 0, 0));

        _ = mat4x2<f16>(mat4x2(0.0, 0.0, 0.0, 0.0, 0, 0, 0, 0));
        _ = mat4x2<f16>(mat4x2(0.0f, 0.0, 0.0, 0.0, 0, 0, 0, 0));
        _ = mat4x2<f16>(mat4x2(0.0h, 0.0, 0.0, 0.0, 0, 0, 0, 0));
        _ = mat4x2<f16>(mat4x2(f, 0.0, 0.0, 0.0, 0, 0, 0, 0));
        _ = mat4x2<f16>(mat4x2(h, 0.0, 0.0, 0.0, 0, 0, 0, 0));


        _ = mat4x2(mat4x2(0.0, 0.0, 0.0, 0.0, 0, 0, 0, 0));
        _ = mat4x2(mat4x2(0.0f, 0.0, 0.0, 0.0, 0, 0, 0, 0));
        _ = mat4x2(mat4x2(0.0h, 0.0, 0.0, 0.0, 0, 0, 0, 0));
        _ = mat4x2(mat4x2(f, 0.0, 0.0, 0.0, 0, 0, 0, 0));
        _ = mat4x2(mat4x2(h, 0.0, 0.0, 0.0, 0, 0, 0, 0));

        _ = mat4x2(vec2(0.0, 0.0), vec2(0.0, 0.0), vec2(0), vec2(0));
        _ = mat4x2(vec2(0.0f, 0.0), vec2(0.0, 0.0), vec2(0), vec2(0));
        _ = mat4x2(vec2(0.0h, 0.0), vec2(0.0, 0.0), vec2(0), vec2(0));
        _ = mat4x2(vec2(f, 0.0), vec2(0.0, 0.0), vec2(0), vec2(0));
        _ = mat4x2(vec2(h, 0.0), vec2(0.0, 0.0), vec2(0), vec2(0));
    }

    {
        _ = mat4x3(0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0, 0, 0, 0, 0, 0);
        _ = mat4x3(0.0f, 0.0, 0.0, 0.0, 0.0, 0.0, 0, 0, 0, 0, 0, 0);
        _ = mat4x3(0.0h, 0.0, 0.0, 0.0, 0.0, 0.0, 0, 0, 0, 0, 0, 0);
        _ = mat4x3(f, 0.0, 0.0, 0.0, 0.0, 0.0, 0, 0, 0, 0, 0, 0);
        _ = mat4x3(h, 0.0, 0.0, 0.0, 0.0, 0.0, 0, 0, 0, 0, 0, 0);

        _ = mat4x3<f32>(mat4x3(0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0, 0, 0, 0, 0, 0));
        _ = mat4x3<f32>(mat4x3(0.0f, 0.0, 0.0, 0.0, 0.0, 0.0, 0, 0, 0, 0, 0, 0));
        _ = mat4x3<f32>(mat4x3(0.0h, 0.0, 0.0, 0.0, 0.0, 0.0, 0, 0, 0, 0, 0, 0));
        _ = mat4x3<f32>(mat4x3(f, 0.0, 0.0, 0.0, 0.0, 0.0, 0, 0, 0, 0, 0, 0));
        _ = mat4x3<f32>(mat4x3(h, 0.0, 0.0, 0.0, 0.0, 0.0, 0, 0, 0, 0, 0, 0));

        _ = mat4x3<f16>(mat4x3(0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0, 0, 0, 0, 0, 0));
        _ = mat4x3<f16>(mat4x3(0.0f, 0.0, 0.0, 0.0, 0.0, 0.0, 0, 0, 0, 0, 0, 0));
        _ = mat4x3<f16>(mat4x3(0.0h, 0.0, 0.0, 0.0, 0.0, 0.0, 0, 0, 0, 0, 0, 0));
        _ = mat4x3<f16>(mat4x3(f, 0.0, 0.0, 0.0, 0.0, 0.0, 0, 0, 0, 0, 0, 0));
        _ = mat4x3<f16>(mat4x3(h, 0.0, 0.0, 0.0, 0.0, 0.0, 0, 0, 0, 0, 0, 0));


        _ = mat4x3(mat4x3(0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0, 0, 0, 0, 0, 0));
        _ = mat4x3(mat4x3(0.0f, 0.0, 0.0, 0.0, 0.0, 0.0, 0, 0, 0, 0, 0, 0));
        _ = mat4x3(mat4x3(0.0h, 0.0, 0.0, 0.0, 0.0, 0.0, 0, 0, 0, 0, 0, 0));
        _ = mat4x3(mat4x3(f, 0.0, 0.0, 0.0, 0.0, 0.0, 0, 0, 0, 0, 0, 0));
        _ = mat4x3(mat4x3(h, 0.0, 0.0, 0.0, 0.0, 0.0, 0, 0, 0, 0, 0, 0));

        _ = mat4x3(vec3(0.0), vec3(0.0), vec3(0), vec3(0));
        _ = mat4x3(vec3(0.0f), vec3(0.0), vec3(0), vec3(0));
        _ = mat4x3(vec3(0.0h), vec3(0.0), vec3(0), vec3(0));
        _ = mat4x3(vec3(f), vec3(0.0), vec3(0), vec3(0));
        _ = mat4x3(vec3(h), vec3(0.0), vec3(0), vec3(0));
    }

    {
        _ = mat4x4(0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0, 0, 0, 0, 0, 0, 0, 0);
        _ = mat4x4(0.0f, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0, 0, 0, 0, 0, 0, 0, 0);
        _ = mat4x4(0.0h, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0, 0, 0, 0, 0, 0, 0, 0);
        _ = mat4x4(f, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0, 0, 0, 0, 0, 0, 0, 0);
        _ = mat4x4(h, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0, 0, 0, 0, 0, 0, 0, 0);

        _ = mat4x4<f32>(mat4x4(0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0, 0, 0, 0, 0, 0, 0, 0));
        _ = mat4x4<f32>(mat4x4(0.0f, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0, 0, 0, 0, 0, 0, 0, 0));
        _ = mat4x4<f32>(mat4x4(0.0h, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0, 0, 0, 0, 0, 0, 0, 0));
        _ = mat4x4<f32>(mat4x4(f, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0, 0, 0, 0, 0, 0, 0, 0));
        _ = mat4x4<f32>(mat4x4(h, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0, 0, 0, 0, 0, 0, 0, 0));

        _ = mat4x4<f16>(mat4x4(0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0, 0, 0, 0, 0, 0, 0, 0));
        _ = mat4x4<f16>(mat4x4(0.0f, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0, 0, 0, 0, 0, 0, 0, 0));
        _ = mat4x4<f16>(mat4x4(0.0h, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0, 0, 0, 0, 0, 0, 0, 0));
        _ = mat4x4<f16>(mat4x4(f, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0, 0, 0, 0, 0, 0, 0, 0));
        _ = mat4x4<f16>(mat4x4(h, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0, 0, 0, 0, 0, 0, 0, 0));


        _ = mat4x4(mat4x4(0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0, 0, 0, 0, 0, 0, 0, 0));
        _ = mat4x4(mat4x4(0.0f, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0, 0, 0, 0, 0, 0, 0, 0));
        _ = mat4x4(mat4x4(0.0h, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0, 0, 0, 0, 0, 0, 0, 0));
        _ = mat4x4(mat4x4(f, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0, 0, 0, 0, 0, 0, 0, 0));
        _ = mat4x4(mat4x4(h, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0, 0, 0, 0, 0, 0, 0, 0));

        _ = mat4x4(vec4(0.0), vec4(0.0), vec4(0), vec4(0));
        _ = mat4x4(vec4(0.0f), vec4(0.0), vec4(0), vec4(0));
        _ = mat4x4(vec4(0.0h), vec4(0.0), vec4(0), vec4(0));
        _ = mat4x4(vec4(f), vec4(0.0), vec4(0), vec4(0));
        _ = mat4x4(vec4(h), vec4(0.0), vec4(0), vec4(0));
    }
}

// 16.1.2.15.
// RUN: %metal-compile testStructures
@compute @workgroup_size(1)
fn testStructures()
{
    let i = 42;
    _ = S(42);
    _ = S(42i);
    _ = S(i);
}

// 16.1.2.16.
// RUN: %metal-compile testU32
@compute @workgroup_size(1)
fn testU32()
{
    let b = false;
    let i = 0i;
    let u = 0u;
    let f = 0f;
    let h = 0h;

    _ = u32(true);
    _ = u32(0);
    _ = u32(0i);
    _ = u32(0u);
    _ = u32(0.0);
    _ = u32(0f);
    _ = u32(0h);
    _ = u32(4294967295);
    _ = u32(b);
    _ = u32(i);
    _ = u32(u);
    _ = u32(f);
    _ = u32(h);
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

// 16.2. Bit Reinterpretation Built-in Functions (https://www.w3.org/TR/WGSL/#bitcast-builtin)
// RUN: %metal-compile testBitcast
@compute @workgroup_size(1)
fn testBitcast()
{
    let u = 0u;
    let i = 0i;
    let f = 0f;
    let h = 0h;

    { const x =bitcast<vec2<i32>>(vec2(.659341217228384203)); }

    // @const @must_use fn bitcast<T>(e : T) -> T
    { const x: u32 = bitcast<u32>(5u); }
    { const x: i32 = bitcast<i32>(5i); }
    { const x: f32 = bitcast<f32>(5f); }
    { const x: f16 = bitcast<f16>(5h); }
    { let x: u32 = bitcast<u32>(u); }
    { let x: i32 = bitcast<i32>(i); }
    { let x: f32 = bitcast<f32>(f); }
    { let x: f16 = bitcast<f16>(h); }

    // @const @must_use fn bitcast<T>(e : S) -> T
    { const x: u32 = bitcast<u32>(5i); }
    { const x: u32 = bitcast<u32>(5f); }
    { let x: u32 = bitcast<u32>(i); }
    { let x: u32 = bitcast<u32>(f); }

    { const x: i32 = bitcast<i32>(5u); }
    { const x: i32 = bitcast<i32>(5f); }
    { let x: i32 = bitcast<i32>(u); }
    { let x: i32 = bitcast<i32>(f); }

    { const x: f32 = bitcast<f32>(5u); }
    { const x: f32 = bitcast<f32>(5i); }
    { let x: f32 = bitcast<f32>(u); }
    { let x: f32 = bitcast<f32>(i); }

    // @const @must_use fn bitcast<vecN<T>>(e : vecN<S>) -> T

    // vec2
    { const x: vec2<u32> = bitcast<vec2<u32>>(vec2(5u)); }
    { const x: vec2<u32> = bitcast<vec2<u32>>(vec2(5i)); }
    { const x: vec2<u32> = bitcast<vec2<u32>>(vec2(5f)); }
    { let x: vec2<u32> = bitcast<vec2<u32>>(vec2(u)); }
    { let x: vec2<u32> = bitcast<vec2<u32>>(vec2(i)); }
    { let x: vec2<u32> = bitcast<vec2<u32>>(vec2(f)); }

    { const x: vec2<i32> = bitcast<vec2<i32>>(vec2(5u)); }
    { const x: vec2<i32> = bitcast<vec2<i32>>(vec2(5i)); }
    { const x: vec2<i32> = bitcast<vec2<i32>>(vec2(5f)); }
    { let x: vec2<i32> = bitcast<vec2<i32>>(vec2(u)); }
    { let x: vec2<i32> = bitcast<vec2<i32>>(vec2(i)); }
    { let x: vec2<i32> = bitcast<vec2<i32>>(vec2(f)); }

    { const x: vec2<f32> = bitcast<vec2<f32>>(vec2(5u)); }
    { const x: vec2<f32> = bitcast<vec2<f32>>(vec2(5i)); }
    { const x: vec2<f32> = bitcast<vec2<f32>>(vec2(5f)); }
    { let x: vec2<f32> = bitcast<vec2<f32>>(vec2(u)); }
    { let x: vec2<f32> = bitcast<vec2<f32>>(vec2(i)); }
    { let x: vec2<f32> = bitcast<vec2<f32>>(vec2(f)); }

    // vec3
    { const x: vec3<u32> = bitcast<vec3<u32>>(vec3(5u)); }
    { const x: vec3<u32> = bitcast<vec3<u32>>(vec3(5i)); }
    { const x: vec3<u32> = bitcast<vec3<u32>>(vec3(5f)); }
    { let x: vec3<u32> = bitcast<vec3<u32>>(vec3(u)); }
    { let x: vec3<u32> = bitcast<vec3<u32>>(vec3(i)); }
    { let x: vec3<u32> = bitcast<vec3<u32>>(vec3(f)); }

    { const x: vec3<i32> = bitcast<vec3<i32>>(vec3(5u)); }
    { const x: vec3<i32> = bitcast<vec3<i32>>(vec3(5i)); }
    { const x: vec3<i32> = bitcast<vec3<i32>>(vec3(5f)); }
    { let x: vec3<i32> = bitcast<vec3<i32>>(vec3(u)); }
    { let x: vec3<i32> = bitcast<vec3<i32>>(vec3(i)); }
    { let x: vec3<i32> = bitcast<vec3<i32>>(vec3(f)); }

    { const x: vec3<f32> = bitcast<vec3<f32>>(vec3(5u)); }
    { const x: vec3<f32> = bitcast<vec3<f32>>(vec3(5i)); }
    { const x: vec3<f32> = bitcast<vec3<f32>>(vec3(5f)); }
    { let x: vec3<f32> = bitcast<vec3<f32>>(vec3(u)); }
    { let x: vec3<f32> = bitcast<vec3<f32>>(vec3(i)); }
    { let x: vec3<f32> = bitcast<vec3<f32>>(vec3(f)); }

    // vec4
    { const x: vec4<u32> = bitcast<vec4<u32>>(vec4(5u)); }
    { const x: vec4<u32> = bitcast<vec4<u32>>(vec4(5i)); }
    { const x: vec4<u32> = bitcast<vec4<u32>>(vec4(5f)); }
    { let x: vec4<u32> = bitcast<vec4<u32>>(vec4(u)); }
    { let x: vec4<u32> = bitcast<vec4<u32>>(vec4(i)); }
    { let x: vec4<u32> = bitcast<vec4<u32>>(vec4(f)); }

    { const x: vec4<i32> = bitcast<vec4<i32>>(vec4(5u)); }
    { const x: vec4<i32> = bitcast<vec4<i32>>(vec4(5i)); }
    { const x: vec4<i32> = bitcast<vec4<i32>>(vec4(5f)); }
    { let x: vec4<i32> = bitcast<vec4<i32>>(vec4(u)); }
    { let x: vec4<i32> = bitcast<vec4<i32>>(vec4(i)); }
    { let x: vec4<i32> = bitcast<vec4<i32>>(vec4(f)); }

    { const x: vec4<f32> = bitcast<vec4<f32>>(vec4(5u)); }
    { const x: vec4<f32> = bitcast<vec4<f32>>(vec4(5i)); }
    { const x: vec4<f32> = bitcast<vec4<f32>>(vec4(5f)); }
    { let x: vec4<f32> = bitcast<vec4<f32>>(vec4(u)); }
    { let x: vec4<f32> = bitcast<vec4<f32>>(vec4(i)); }
    { let x: vec4<f32> = bitcast<vec4<f32>>(vec4(f)); }

    // @const @must_use fn bitcast<u32>(e : AbstractInt) -> T
    { const x: u32 = bitcast<u32>(4294967295); }

    // @const @must_use fn bitcast<vecN<u32>>(e : vecN<AbstractInt>) -> T
    { const x: vec2<u32> = bitcast<vec2<u32>>(vec2(4294967295)); }
    { const x: vec3<u32> = bitcast<vec3<u32>>(vec3(4294967295)); }
    { const x: vec4<u32> = bitcast<vec4<u32>>(vec4(4294967295)); }

    // @const @must_use fn bitcast<T>(e : vec2<f16>) -> T
    { const x: u32 = bitcast<u32>(vec2(5h)); }
    { let x: u32 = bitcast<u32>(vec2(h)); }
    { const x: i32 = bitcast<i32>(vec2(5h)); }
    { let x: i32 = bitcast<i32>(vec2(h)); }
    { const x: f32 = bitcast<f32>(vec2(5h)); }
    { let x: f32 = bitcast<f32>(vec2(h)); }

    // @const @must_use fn bitcast<vec2<T>>(e : vec4<f16>) -> vec2<T>
    { const x: vec2<u32> = bitcast<vec2<u32>>(vec4(5h)); }
    { let x: vec2<u32> = bitcast<vec2<u32>>(vec4(h)); }
    { const x: vec2<i32> = bitcast<vec2<i32>>(vec4(5h)); }
    { let x: vec2<i32> = bitcast<vec2<i32>>(vec4(h)); }
    { const x: vec2<f32> = bitcast<vec2<f32>>(vec4(5h)); }
    { let x: vec2<f32> = bitcast<vec2<f32>>(vec4(h)); }

    // @const @must_use fn bitcast<vec2<f16>>(e : T) -> vec2<f16>
    { const x: vec2<f16> = bitcast<vec2<f16>>(5u); }
    { const x: vec2<f16> = bitcast<vec2<f16>>(5i); }
    { const x: vec2<f16> = bitcast<vec2<f16>>(5f); }
    { let x: vec2<f16> = bitcast<vec2<f16>>(u); }
    { let x: vec2<f16> = bitcast<vec2<f16>>(i); }
    { let x: vec2<f16> = bitcast<vec2<f16>>(f); }

    // @const @must_use fn bitcast<vec4<f16>>(e : vec2<T>) -> vec4<f16>
    { const x: vec4<f16> = bitcast<vec4<f16>>(vec2(5u)); }
    { const x: vec4<f16> = bitcast<vec4<f16>>(vec2(5i)); }
    { const x: vec4<f16> = bitcast<vec4<f16>>(vec2(5f)); }
    { let x: vec4<f16> = bitcast<vec4<f16>>(vec2(u)); }
    { let x: vec4<f16> = bitcast<vec4<f16>>(vec2(i)); }
    { let x: vec4<f16> = bitcast<vec4<f16>>(vec2(f)); }
}

// 16.3. Logical Built-in Functions (https://www.w3.org/TR/WGSL/#logical-builtin-functions)

// 16.3.1
// RUN: %metal-compile testAll
@compute @workgroup_size(1)
fn testAll()
{
    let b = false;

    // [N].(Vector[Bool, N]) => Bool,
    _ = all(vec2(false, true));
    _ = all(vec3(true, true, true));
    _ = all(vec4(false, false, false, false));
    _ = all(vec2(b));
    _ = all(vec3(b));
    _ = all(vec4(b));

    // [N].(Bool) => Bool,
    _ = all(true);
    _ = all(false);
    _ = all(b);
}

// 16.3.2
// RUN: %metal-compile testAny
@compute @workgroup_size(1)
fn testAny()
{
    let b = false;

    // [N].(Vector[Bool, N]) => Bool,
    _ = any(vec2(false, true));
    _ = any(vec3(true, true, true));
    _ = any(vec4(false, false, false, false));
    _ = any(vec2(b));
    _ = any(vec3(b));
    _ = any(vec4(b));

    // [N].(Bool) => Bool,
    _ = any(true);
    _ = any(false);
    _ = any(b);
}

// 16.3.3
// RUN: %metal-compile testSelect
@compute @workgroup_size(1)
fn testSelect()
{
    let b = false;
    // [T < Scalar].(T, T, Bool) => T,
    {
        _ = select(13, 42,   false);
        _ = select(13, 42i,  false);
        _ = select(13, 42u,  true);
        _ = select(13, 42f,  true);
        _ = select(13, 42h,  true);
        _ = select(13, 42.0, true);
        _ = select(13, 42,   b);
        _ = select(13, 42i,  b);
        _ = select(13, 42u,  b);
        _ = select(13, 42f,  b);
        _ = select(13, 42h,  b);
        _ = select(13, 42.0, b);
    }

    // [T < Scalar, N].(Vector[T, N], Vector[T, N], Bool) => Vector[T, N],
    {
        _ = select(vec2(13), vec2(42),   false);
        _ = select(vec2(13), vec2(42i),  false);
        _ = select(vec2(13), vec2(42u),  true);
        _ = select(vec2(13), vec2(42f),  true);
        _ = select(vec2(13), vec2(42h),  true);
        _ = select(vec2(13), vec2(42.0), true);
        _ = select(vec2(13), vec2(42),   b);
        _ = select(vec2(13), vec2(42i),  b);
        _ = select(vec2(13), vec2(42u),  b);
        _ = select(vec2(13), vec2(42f),  b);
        _ = select(vec2(13), vec2(42h),  b);
        _ = select(vec2(13), vec2(42.0), b);
    }
    {
        _ = select(vec3(13), vec3(42),   false);
        _ = select(vec3(13), vec3(42i),  false);
        _ = select(vec3(13), vec3(42u),  true);
        _ = select(vec3(13), vec3(42f),  true);
        _ = select(vec3(13), vec3(42h),  true);
        _ = select(vec3(13), vec3(42.0), true);
        _ = select(vec3(13), vec3(42),   b);
        _ = select(vec3(13), vec3(42i),  b);
        _ = select(vec3(13), vec3(42u),  b);
        _ = select(vec3(13), vec3(42f),  b);
        _ = select(vec3(13), vec3(42h),  b);
        _ = select(vec3(13), vec3(42.0), b);
    }
    {
        _ = select(vec4(13), vec4(42),   false);
        _ = select(vec4(13), vec4(42i),  false);
        _ = select(vec4(13), vec4(42u),  true);
        _ = select(vec4(13), vec4(42f),  true);
        _ = select(vec4(13), vec4(42h),  true);
        _ = select(vec4(13), vec4(42.0), true);
        _ = select(vec4(13), vec4(42),   b);
        _ = select(vec4(13), vec4(42i),  b);
        _ = select(vec4(13), vec4(42u),  b);
        _ = select(vec4(13), vec4(42f),  b);
        _ = select(vec4(13), vec4(42h),  b);
        _ = select(vec4(13), vec4(42.0), b);
    }

    // [T < Scalar, N].(Vector[T, N], Vector[T, N], Vector[Bool, N]) => Vector[T, N],
    {
        _ = select(vec2(13), vec2(42),   vec2(false));
        _ = select(vec2(13), vec2(42i),  vec2(false));
        _ = select(vec2(13), vec2(42u),  vec2(true));
        _ = select(vec2(13), vec2(42f),  vec2(true));
        _ = select(vec2(13), vec2(42h),  vec2(true));
        _ = select(vec2(13), vec2(42.0), vec2(true));
        _ = select(vec2(13), vec2(42),   vec2(b));
        _ = select(vec2(13), vec2(42i),  vec2(b));
        _ = select(vec2(13), vec2(42u),  vec2(b));
        _ = select(vec2(13), vec2(42f),  vec2(b));
        _ = select(vec2(13), vec2(42h),  vec2(b));
        _ = select(vec2(13), vec2(42.0), vec2(b));
    }
    {
        _ = select(vec3(13), vec3(42),   vec3(false));
        _ = select(vec3(13), vec3(42i),  vec3(false));
        _ = select(vec3(13), vec3(42u),  vec3(true));
        _ = select(vec3(13), vec3(42f),  vec3(true));
        _ = select(vec3(13), vec3(42h),  vec3(true));
        _ = select(vec3(13), vec3(42.0), vec3(true));
        _ = select(vec3(13), vec3(42),   vec3(b));
        _ = select(vec3(13), vec3(42i),  vec3(b));
        _ = select(vec3(13), vec3(42u),  vec3(b));
        _ = select(vec3(13), vec3(42f),  vec3(b));
        _ = select(vec3(13), vec3(42h),  vec3(b));
        _ = select(vec3(13), vec3(42.0), vec3(b));
    }
    {
        _ = select(vec4(13), vec4(42),   vec4(false));
        _ = select(vec4(13), vec4(42i),  vec4(false));
        _ = select(vec4(13), vec4(42u),  vec4(true));
        _ = select(vec4(13), vec4(42f),  vec4(true));
        _ = select(vec4(13), vec4(42h),  vec4(true));
        _ = select(vec4(13), vec4(42.0), vec4(true));
        _ = select(vec4(13), vec4(42),   vec4(b));
        _ = select(vec4(13), vec4(42i),  vec4(b));
        _ = select(vec4(13), vec4(42u),  vec4(b));
        _ = select(vec4(13), vec4(42f),  vec4(b));
        _ = select(vec4(13), vec4(42h),  vec4(b));
        _ = select(vec4(13), vec4(42.0), vec4(b));
    }
}

// 16.4. Array Built-in Functions

@group(4) @binding(0) var<storage, read> a1: array<i32>;
@group(4) @binding(1) var<storage, read_write> a2: array<i32>;

// 16.4.1.
// RUN: %metal-compile testArrayLength
@compute @workgroup_size(1)
fn testArrayLength()
{
    // [T].(Ptr[Storage, Array[T], Read]) => U32,
    _ = arrayLength(&a1);

    // [T].(Ptr[Storage, Array[T], ReadWrite]) => U32,
    _ = arrayLength(&a2);
}

// 16.5. Numeric Built-in Functions (https://www.w3.org/TR/WGSL/#numeric-builtin-functions)

// Trigonometric
// RUN: %metal-compile testTrigonometric
@compute @workgroup_size(1)
fn testTrigonometric()
{
  let f = 0.0;
  let v2f = vec2f(0.0);
  let v3f = vec3f(0.0);
  let v4f = vec4f(0.0);
  {
    _ = acos(0.0);
    _ = acos(vec2(0.0, 0.0));
    _ = acos(vec3(0.0, 0.0, 0.0));
    _ = acos(vec4(0.0, 0.0, 0.0, 0.0));
    _ = acos(f);
    _ = acos(v2f);
    _ = acos(v3f);
    _ = acos(v4f);
  }

  {
    _ = asin(0.0);
    _ = asin(vec2(0.0, 0.0));
    _ = asin(vec3(0.0, 0.0, 0.0));
    _ = asin(vec4(0.0, 0.0, 0.0, 0.0));
    _ = asin(f);
    _ = asin(v2f);
    _ = asin(v3f);
    _ = asin(v4f);
  }

  {
    _ = atan(0.0);
    _ = atan(vec2(0.0, 0.0));
    _ = atan(vec3(0.0, 0.0, 0.0));
    _ = atan(vec4(0.0, 0.0, 0.0, 0.0));
    _ = atan(f);
    _ = atan(v2f);
    _ = atan(v3f);
    _ = atan(v4f);
  }

  {
    _ = cos(0.0);
    _ = cos(vec2(0.0, 0.0));
    _ = cos(vec3(0.0, 0.0, 0.0));
    _ = cos(vec4(0.0, 0.0, 0.0, 0.0));
    _ = cos(f);
    _ = cos(v2f);
    _ = cos(v3f);
    _ = cos(v4f);
  }

  {
    _ = sin(0.0);
    _ = sin(vec2(0.0, 0.0));
    _ = sin(vec3(0.0, 0.0, 0.0));
    _ = sin(vec4(0.0, 0.0, 0.0, 0.0));
    _ = sin(f);
    _ = sin(v2f);
    _ = sin(v3f);
    _ = sin(v4f);
  }

  {
    _ = tan(0.0);
    _ = tan(vec2(0.0, 0.0));
    _ = tan(vec3(0.0, 0.0, 0.0));
    _ = tan(vec4(0.0, 0.0, 0.0, 0.0));
    _ = tan(f);
    _ = tan(v2f);
    _ = tan(v3f);
    _ = tan(v4f);
  }
}

// RUN: %metal-compile testTrigonometricHyperbolic
@compute @workgroup_size(1)
fn testTrigonometricHyperbolic()
{
  let f = 0.0;
  let v2f = vec2f(0.0);
  let v3f = vec3f(0.0);
  let v4f = vec4f(0.0);
  {
    _ = acosh(1.0);
    _ = acosh(vec2(1.0, 1.0));
    _ = acosh(vec3(1.0, 1.0, 1.0));
    _ = acosh(vec4(1.0, 1.0, 1.0, 1.0));
    _ = acosh(f);
    _ = acosh(v2f);
    _ = acosh(v3f);
    _ = acosh(v4f);
  }

  {
    _ = asinh(0.0);
    _ = asinh(vec2(0.0, 0.0));
    _ = asinh(vec3(0.0, 0.0, 0.0));
    _ = asinh(vec4(0.0, 0.0, 0.0, 0.0));
    _ = asinh(f);
    _ = asinh(v2f);
    _ = asinh(v3f);
    _ = asinh(v4f);
  }

  {
    _ = atanh(0.0);
    _ = atanh(vec2(0.0, 0.0));
    _ = atanh(vec3(0.0, 0.0, 0.0));
    _ = atanh(vec4(0.0, 0.0, 0.0, 0.0));
    _ = atanh(f);
    _ = atanh(v2f);
    _ = atanh(v3f);
    _ = atanh(v4f);
  }

  {
    _ = cosh(0.0);
    _ = cosh(vec2(0.0, 0.0));
    _ = cosh(vec3(0.0, 0.0, 0.0));
    _ = cosh(vec4(0.0, 0.0, 0.0, 0.0));
    _ = cosh(f);
    _ = cosh(v2f);
    _ = cosh(v3f);
    _ = cosh(v4f);
  }

  {
    _ = sinh(0.0);
    _ = sinh(vec2(0.0, 0.0));
    _ = sinh(vec3(0.0, 0.0, 0.0));
    _ = sinh(vec4(0.0, 0.0, 0.0, 0.0));
    _ = sinh(f);
    _ = sinh(v2f);
    _ = sinh(v3f);
    _ = sinh(v4f);
  }

  {
    _ = tanh(0.0);
    _ = tanh(vec2(0.0, 0.0));
    _ = tanh(vec3(0.0, 0.0, 0.0));
    _ = tanh(vec4(0.0, 0.0, 0.0, 0.0));
    _ = tanh(f);
    _ = tanh(v2f);
    _ = tanh(v3f);
    _ = tanh(v4f);
  }
}


// 16.5.1
// RUN: %metal-compile testAbs
@compute @workgroup_size(1)
fn testAbs()
{
    let i = 0i;
    let u = 0u;
    let f = 0f;
    let h = 0h;
    // [T < Number].(T) => T,
    {
        _ = abs(0);
        _ = abs(1i);
        _ = abs(1u);
        _ = abs(0.0);
        _ = abs(1h);
        _ = abs(1f);
        _ = abs(i);
        _ = abs(u);
        _ = abs(h);
        _ = abs(f);
    }

    // [T < Number, N].(Vector[T, N]) => Vector[T, N],
    {
        _ = abs(vec2(0, 1));
        _ = abs(vec2(1i, 2i));
        _ = abs(vec2(1u, 2u));
        _ = abs(vec2(0.0, 1.0));
        _ = abs(vec2(1f, 2f));
        _ = abs(vec2(i));
        _ = abs(vec2(u));
        _ = abs(vec2(h));
        _ = abs(vec2(f));
    }
    {
        _ = abs(vec3(-1, 0, 1));
        _ = abs(vec3(-1i, 1i, 2i));
        _ = abs(vec3(0u, 1u, 2u));
        _ = abs(vec3(-1.0, 0.0, 1.0));
        _ = abs(vec3(-1f, 1f, 2f));
        _ = abs(vec3(i));
        _ = abs(vec3(u));
        _ = abs(vec3(h));
        _ = abs(vec3(f));
    }
    {
        _ = abs(vec4(-1, 0, 0, 1));
        _ = abs(vec4(-1i, 1i, 1i, 2i));
        _ = abs(vec4(0u, 1u, 1u, 2u));
        _ = abs(vec4(-1.0, 0.0, 0.0, 1.0));
        _ = abs(vec4(-1f, 1f, 1f, 2f));
        _ = abs(vec4(i));
        _ = abs(vec4(u));
        _ = abs(vec4(h));
        _ = abs(vec4(f));
    }
}

// 16.5.2. acos
// 16.5.3. acosh
// 16.5.4. asin
// 16.5.5. asinh
// 16.5.6. atan
// 16.5.7. atanh
// Tested in testTrigonometric and testTrigonometricHyperbolic

// 16.5.8
// RUN: %metal-compile testAtan2
@compute @workgroup_size(1)
fn testAtan2() {
    let f = 0f;
    let h = 0h;

    // [T < Float].(T, T) => T,
    {
        _ = atan2(0, 1);
        _ = atan2(0, 1.0);
        _ = atan2(1, 2f);
        _ = atan2(1, 2h);
        _ = atan2(f, f);
        _ = atan2(h, h);
    }
    // [T < Float, N].(Vector[T, N], Vector[T, N], Vector[T, N]) => Vector[T, N],
    {
        _ = atan2(vec2(0), vec2(0));
        _ = atan2(vec2(0), vec2(0.0));
        _ = atan2(vec2(0), vec2(0f));
        _ = atan2(vec2(0), vec2(0h));
        _ = atan2(vec2(f), vec2(f));
        _ = atan2(vec2(h), vec2(h));
    }
    {
        _ = atan2(vec3(0), vec3(0));
        _ = atan2(vec3(0), vec3(0.0));
        _ = atan2(vec3(0), vec3(0f));
        _ = atan2(vec3(0), vec3(0h));
        _ = atan2(vec3(f), vec3(f));
        _ = atan2(vec3(h), vec3(h));
    }
    {
        _ = atan2(vec4(0), vec4(0));
        _ = atan2(vec4(0), vec4(0.0));
        _ = atan2(vec4(0), vec4(0f));
        _ = atan2(vec4(0), vec4(0h));
        _ = atan2(vec4(f), vec4(f));
        _ = atan2(vec4(h), vec4(h));
    }
}

// 16.5.9
// RUN: %metal-compile testCeil
@compute @workgroup_size(1)
fn testCeil()
{
    let f = 0f;
    let h = 0h;

    // [T < Float].(T) => T,
    {
        _ = ceil(0);
        _ = ceil(0.0);
        _ = ceil(1f);
        _ = ceil(1h);
        _ = ceil(f);
        _ = ceil(h);
    }

    // [T < Float, N].(Vector[T, N]) => Vector[T, N],
    {
        _ = ceil(vec2(0, 1));
        _ = ceil(vec2(0.0, 1.0));
        _ = ceil(vec2(1f, 2f));
        _ = ceil(vec2(1h, 2h));
        _ = ceil(vec2(f));
        _ = ceil(vec2(h));
    }
    {
        _ = ceil(vec3(-1, 0, 1));
        _ = ceil(vec3(-1.0, 0.0, 1.0));
        _ = ceil(vec3(-1f, 1f, 2f));
        _ = ceil(vec3(1h, 2h, 3h));
        _ = ceil(vec3(f));
        _ = ceil(vec3(h));
    }
    {
        _ = ceil(vec4(-1, 0, 1, 2));
        _ = ceil(vec4(-1.0, 0.0, 1.0, 2.0));
        _ = ceil(vec4(-1f, 1f, 2f, 3f));
        _ = ceil(vec4(1h, 2h, 3h, 4h));
        _ = ceil(vec4(f));
        _ = ceil(vec4(h));
    }
}

// 16.5.10
// RUN: %metal-compile testCeil
@compute @workgroup_size(1)
fn testClamp()
{
    let i = 1i;
    let u = 1u;
    let f = 1f;
    let h = 1h;

    // [T < Number].(T, T, T) => T,
    {
        _ = clamp(-1, 0, 1);
        _ = clamp(-1, 1, 2i);
        _ = clamp(0, 1, 2u);
        _ = clamp(-1, 0, 1.0);
        _ = clamp(-1, 1, 2f);
        _ = clamp(-1, 1, 2h);
        _ = clamp(i, i, i);
        _ = clamp(u, u, u);
        _ = clamp(f, f, f);
        _ = clamp(h, h, h);
    }
    // [T < Number, N].(Vector[T, N], Vector[T, N], Vector[T, N]) => Vector[T, N],
    {
        _ = clamp(vec2(0), vec2(0), vec2(0));
        _ = clamp(vec2(0), vec2(0), vec2(0i));
        _ = clamp(vec2(0), vec2(0), vec2(0u));
        _ = clamp(vec2(0), vec2(0), vec2(0.0));
        _ = clamp(vec2(0), vec2(0), vec2(0f));
        _ = clamp(vec2(-1), vec2(1), vec2(2h));
        _ = clamp(vec2(i), vec2(i), vec2(i));
        _ = clamp(vec2(u), vec2(u), vec2(u));
        _ = clamp(vec2(f), vec2(f), vec2(f));
        _ = clamp(vec2(h), vec2(h), vec2(h));
    }
    {
        _ = clamp(vec3(0), vec3(0), vec3(0));
        _ = clamp(vec3(0), vec3(0), vec3(0i));
        _ = clamp(vec3(0), vec3(0), vec3(0u));
        _ = clamp(vec3(0), vec3(0), vec3(0.0));
        _ = clamp(vec3(0), vec3(0), vec3(0f));
        _ = clamp(vec3(-1), vec3(1), vec3(2h));
        _ = clamp(vec3(i), vec3(i), vec3(i));
        _ = clamp(vec3(u), vec3(u), vec3(u));
        _ = clamp(vec3(f), vec3(f), vec3(f));
        _ = clamp(vec3(h), vec3(h), vec3(h));
    }
    {
        _ = clamp(vec4(0), vec4(0), vec4(0));
        _ = clamp(vec4(0), vec4(0), vec4(0i));
        _ = clamp(vec4(0), vec4(0), vec4(0u));
        _ = clamp(vec4(0), vec4(0), vec4(0.0));
        _ = clamp(vec4(0), vec4(0), vec4(0f));
        _ = clamp(vec4(-1), vec4(1), vec4(2h));
        _ = clamp(vec4(i), vec4(i), vec4(i));
        _ = clamp(vec4(u), vec4(u), vec4(u));
        _ = clamp(vec4(f), vec4(f), vec4(f));
        _ = clamp(vec4(h), vec4(h), vec4(h));
    }
}

// 16.5.11. cos
// 16.5.12. cosh
// Tested in testTrigonometric and testTrigonometricHyperbolic

// 16.5.13-15 (Bit counting)
// RUN: %metal-compile testBitCounting
@compute @workgroup_size(1)
fn testBitCounting()
{
    // [T < ConcreteInteger].(T) => T,
    {
        let i = 1i;
        let u = 1u;
        _ = countLeadingZeros(1);
        _ = countLeadingZeros(1i);
        _ = countLeadingZeros(1u);
        let r1: i32 = countLeadingZeros(i);
        let r2: u32 = countLeadingZeros(u);
    }
    {
        let i = 1i;
        let u = 1u;
        _ = countOneBits(1);
        _ = countOneBits(1i);
        _ = countOneBits(1u);
        let r1: i32 = countOneBits(i);
        let r2: u32 = countOneBits(u);
    }
    {
        let i = 1i;
        let u = 1u;
        _ = countTrailingZeros(1);
        _ = countTrailingZeros(1i);
        _ = countTrailingZeros(1u);
        let r1: i32 = countTrailingZeros(i);
        let r2: u32 = countTrailingZeros(u);
    }
    // [T < ConcreteInteger, N].(Vector[T, N]) => Vector[T, N],
    {
        let vi = vec2(1i);
        let vu = vec2(1u);
        _ = countLeadingZeros(vec2(1, 1));
        _ = countLeadingZeros(vec2(1i, 1i));
        _ = countLeadingZeros(vec2(1u, 1u));
        let r1: vec2i = countLeadingZeros(vi);
        let r2: vec2u = countLeadingZeros(vu);
    }
    {
        let vi = vec3(1i);
        let vu = vec3(1u);
        _ = countOneBits(vec3(1, 1, 1));
        _ = countOneBits(vec3(1i, 1i, 1i));
        _ = countOneBits(vec3(1u, 1u, 1u));
        let r1: vec3i = countOneBits(vi);
        let r2: vec3u = countOneBits(vu);
    }
    {
        let vi = vec4(1i);
        let vu = vec4(1u);
        _ = countTrailingZeros(vec4(1, 1, 1, 1));
        _ = countTrailingZeros(vec4(1i, 1i, 1i, 1i));
        _ = countTrailingZeros(vec4(1u, 1u, 1u, 1u));
        let r1: vec4i = countTrailingZeros(vi);
        let r2: vec4u = countTrailingZeros(vu);
    }
}

// 16.5.16
// RUN: %metal-compile testCross
@compute @workgroup_size(1)
fn testCross()
{
    let f = 0f;
    let h = 0h;

    // [T < Float].(Vector[T, 3], Vector[T, 3]) => Vector[T, 3],
    _ = cross(vec3(1, 1, 1), vec3(1f, 2f, 3f));
    _ = cross(vec3(1.0, 1.0, 1.0), vec3(1f, 2f, 3f));
    _ = cross(vec3(1f, 1f, 1f), vec3(1f, 2f, 3f));
    _ = cross(vec3(f), vec3(f));

    _ = cross(vec3(1, 1, 1), vec3(1h, 2h, 3h));
    _ = cross(vec3(1.0, 1.0, 1.0), vec3(1h, 2h, 3h));
    _ = cross(vec3(1h, 1h, 1h), vec3(1h, 2h, 3h));
    _ = cross(vec3(h), vec3(h));
}

// 16.5.17
// RUN: %metal-compile testDegress
@compute @workgroup_size(1)
fn testDegress()
{
    let f = 0f;
    let h = 0h;

    // [T < Float].(T) => T,
    {
        _ = degrees(0);
        _ = degrees(0.0);
        _ = degrees(1f);
        _ = degrees(1h);
        _ = degrees(f);
        _ = degrees(h);
    }
    // [T < Float, N].(Vector[T, N]) => Vector[T, N],
    {
        _ = degrees(vec2(0, 1));
        _ = degrees(vec2(0.0, 1.0));
        _ = degrees(vec2(1f, 2f));
        _ = degrees(vec2(1h));
        _ = degrees(vec2(f));
        _ = degrees(vec2(h));
    }
    {
        _ = degrees(vec3(-1, 0, 1));
        _ = degrees(vec3(-1.0, 0.0, 1.0));
        _ = degrees(vec3(-1f, 1f, 2f));
        _ = degrees(vec3(1h));
        _ = degrees(vec3(f));
        _ = degrees(vec3(h));
    }
    {
        _ = degrees(vec4(-1, 0, 1, 2));
        _ = degrees(vec4(-1.0, 0.0, 1.0, 2.0));
        _ = degrees(vec4(-1f, 1f, 2f, 3f));
        _ = degrees(vec4(1h));
        _ = degrees(vec4(f));
        _ = degrees(vec4(h));
    }
}

// 16.5.18
// RUN: %metal-compile testDegress
@compute @workgroup_size(1)
fn testDeterminant()
{
    let f = 1f;
    let h = 1h;

    // [T < Float, C].(Matrix[T, C, C]) => T,
    _ = determinant(mat2x2(1, 1, 1, 1));
    _ = determinant(mat3x3(1, 1, 1, 1, 1, 1, 1, 1, 1));
    _ = determinant(mat4x4(1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1));

    _ = determinant(mat2x2(1f, 1f, 1f, 1f));
    _ = determinant(mat3x3(1f, 1f, 1f, 1f, 1f, 1f, 1f, 1f, 1f));
    _ = determinant(mat4x4(1f, 1f, 1f, 1f, 1f, 1f, 1f, 1f, 1f, 1f, 1f, 1f, 1f, 1f, 1f, 1f));

    _ = determinant(mat2x2(1h, 1h, 1h, 1h));
    _ = determinant(mat3x3(1h, 1h, 1h, 1h, 1h, 1h, 1h, 1h, 1h));
    _ = determinant(mat4x4(1h, 1h, 1h, 1h, 1h, 1h, 1h, 1h, 1h, 1h, 1h, 1h, 1h, 1h, 1h, 1h));

    _ = determinant(mat2x2(f, f, f, f));
    _ = determinant(mat3x3(f, f, f, f, f, f, f, f, f));
    _ = determinant(mat4x4(f, f, f, f, f, f, f, f, f, f, f, f, f, f, f, f));

    _ = determinant(mat2x2(h, h, h, h));
    _ = determinant(mat3x3(h, h, h, h, h, h, h, h, h));
    _ = determinant(mat4x4(h, h, h, h, h, h, h, h, h, h, h, h, h, h, h, h));
}

// 16.5.19
// RUN: %metal-compile testDistance
@compute @workgroup_size(1)
fn testDistance()
{
    // [T < Float].(T, T) => T,
    var a = 1.f;
    let b = 2.f;
    {
        _ = distance(0, 1);
        _ = distance(0, 1.0);
        _ = distance(0, 1f);
        _ = distance(0.0, 1.0);
        _ = distance(1.0, 2f);
        _ = distance(1f, 2f);
        _ = distance(a, b);
    }
    // [T < Float, N].(Vector[T, N], Vector[T, N]) => T,
    {
        _ = distance(vec2(0),   vec2(1)  );
        _ = distance(vec2(0),   vec2(0.0));
        _ = distance(vec2(0),   vec2(1f) );
        _ = distance(vec2(0.0), vec2(0.0));
        _ = distance(vec2(0.0), vec2(0f) );
        _ = distance(vec2(1f),  vec2(1f) );
        _ = distance(vec2(a), vec2(b));
    }
    {
        _ = distance(vec3(0),   vec3(1)  );
        _ = distance(vec3(0),   vec3(0.0));
        _ = distance(vec3(0),   vec3(1f) );
        _ = distance(vec3(0.0), vec3(0.0));
        _ = distance(vec3(0.0), vec3(0f) );
        _ = distance(vec3(1f),  vec3(1f) );
        _ = distance(vec3(a), vec3(b));
    }
    {
        _ = distance(vec4(0),   vec4(1)  );
        _ = distance(vec4(0),   vec4(0.0));
        _ = distance(vec4(0),   vec4(1f) );
        _ = distance(vec4(0.0), vec4(0.0));
        _ = distance(vec4(0.0), vec4(0f) );
        _ = distance(vec4(1f),  vec4(1f) );
        _ = distance(vec4(a), vec4(b));
    }
}

// 16.5.20
// RUN: %metal-compile testDot
@compute @workgroup_size(1)
fn testDot()
{
    let i = 1i;
    let u = 1u;
    let f = 1f;
    let h = 1h;

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
        _ = dot(vec2(i), vec2(i));
        _ = dot(vec2(u), vec2(u));
        _ = dot(vec2(f), vec2(f));
        _ = dot(vec2(h), vec2(h));
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
        _ = dot(vec3(i), vec3(i));
        _ = dot(vec3(u), vec3(u));
        _ = dot(vec3(f), vec3(f));
        _ = dot(vec3(h), vec3(h));
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
        _ = dot(vec4(i), vec4(i));
        _ = dot(vec4(u), vec4(u));
        _ = dot(vec4(f), vec4(f));
        _ = dot(vec4(h), vec4(h));
    }
}

// 16.5.21
// RUN: %metal-compile testDot4U8Packed
@compute @workgroup_size(1)
fn testDot4U8Packed()
{
    let u = 0u;
    { const x: u32 = dot4U8Packed(0u, 0u); }
    { let x: u32 = dot4U8Packed(u, u); }
}

// 16.5.22
// RUN: %metal-compile testDot4I8Packed
@compute @workgroup_size(1)
fn testDot4I8Packed()
{
    let u = 0u;
    { const x: i32 = dot4I8Packed(0u, 0u); }
    { let x: i32 = dot4I8Packed(u, u); }
}

// 16.5.21 & 16.5.22
// RUN: %metal-compile testExpAndExp2
@compute @workgroup_size(1)
fn testExpAndExp2()
{
    let f = 0f;
    let h = 0h;

    // [T < Float].(T) => T,
    {
        _ = exp(0);
        _ = exp(0.0);
        _ = exp(1f);
        _ = exp(1h);
        _ = exp(f);
        _ = exp(h);
    }
    {
        _ = exp2(0);
        _ = exp2(0.0);
        _ = exp2(1f);
        _ = exp2(1h);
        _ = exp2(f);
        _ = exp2(h);
    }

    // [T < Float, N].(Vector[T, N]) => Vector[T, N],
    {
        _ = exp(vec2(0, 1));
        _ = exp(vec2(0.0, 1.0));
        _ = exp(vec2(1f, 2f));
        _ = exp(vec2(1h));
        _ = exp(vec2(f));
        _ = exp(vec2(h));
    }
    {
        _ = exp(vec3(-1, 0, 1));
        _ = exp(vec3(-1.0, 0.0, 1.0));
        _ = exp(vec3(-1f, 1f, 2f));
        _ = exp(vec3(1h));
        _ = exp(vec3(f));
        _ = exp(vec3(h));
    }
    {
        _ = exp(vec4(-1, 0, 1, 2));
        _ = exp(vec4(-1.0, 0.0, 1.0, 2.0));
        _ = exp(vec4(-1f, 1f, 2f, 3f));
        _ = exp(vec4(1h));
        _ = exp(vec4(f));
        _ = exp(vec4(h));
    }

    {
        _ = exp2(vec2(0, 1));
        _ = exp2(vec2(0.0, 1.0));
        _ = exp2(vec2(1f, 2f));
        _ = exp2(vec2(1h));
        _ = exp2(vec2(f));
        _ = exp2(vec2(h));
    }
    {
        _ = exp2(vec3(-1, 0, 1));
        _ = exp2(vec3(-1.0, 0.0, 1.0));
        _ = exp2(vec3(-1f, 1f, 2f));
        _ = exp2(vec3(1h));
        _ = exp2(vec3(f));
        _ = exp2(vec3(h));
    }
    {
        _ = exp2(vec4(-1, 0, 1, 2));
        _ = exp2(vec4(-1.0, 0.0, 1.0, 2.0));
        _ = exp2(vec4(-1f, 1f, 2f, 3f));
        _ = exp2(vec4(1h));
        _ = exp2(vec4(f));
        _ = exp2(vec4(h));
    }
}

// 16.5.23 & 16.5.24
// RUN: %metal-compile testExtractBits
@compute @workgroup_size(1)
fn testExtractBits()
{
    // signed
    // [].(I32, U32, U32) => I32,
    {
        let i = 0i;
        _ = extractBits(0, 1, 1);
        _ = extractBits(0i, 1, 1);
        _ = extractBits(0i, 1u, 1u);
        let r: i32 = extractBits(i, 1u, 1u);
    }
    // [N].(Vector[I32, N], U32, U32) => Vector[I32, N],
    {
        let vi = vec2(0i);
        _ = extractBits(vec2(0), 1, 1);
        _ = extractBits(vec2(0i), 1, 1);
        _ = extractBits(vec2(0i), 1u, 1u);
        let r: vec2i = extractBits(vi, 1u, 1u);
    }
    {
        let vi = vec3(0i);
        _ = extractBits(vec3(0), 1, 1);
        _ = extractBits(vec3(0i), 1, 1);
        _ = extractBits(vec3(0i), 1u, 1u);
        let r: vec3i = extractBits(vi, 1u, 1u);
    }
    {
        let vi = vec4(0i);
        _ = extractBits(vec4(0), 1, 1);
        _ = extractBits(vec4(0i), 1, 1);
        _ = extractBits(vec4(0i), 1u, 1u);
        let r: vec4i = extractBits(vi, 1u, 1u);
    }

    // unsigned
    // [].(U32, U32, U32) => U32,
    {
        let u = 0u;
        _ = extractBits(0, 1, 1);
        _ = extractBits(0u, 1, 1);
        _ = extractBits(0u, 1u, 1u);
        let r: u32 = extractBits(u, 1u, 1u);
    }

    // [N].(Vector[U32, N], U32, U32) => Vector[U32, N],
    {
        let vu = vec2(0u);
        _ = extractBits(vec2(0), 1, 1);
        _ = extractBits(vec2(0u), 1, 1);
        _ = extractBits(vec2(0u), 1u, 1u);
        let r: vec2u = extractBits(vu, 1u, 1u);
    }
    {
        let vu = vec3(0u);
        _ = extractBits(vec3(0), 1, 1);
        _ = extractBits(vec3(0u), 1, 1);
        _ = extractBits(vec3(0u), 1u, 1u);
        let r: vec3u = extractBits(vu, 1u, 1u);
    }
    {
        let vu = vec4(0u);
        _ = extractBits(vec4(0), 1, 1);
        _ = extractBits(vec4(0u), 1, 1);
        _ = extractBits(vec4(0u), 1u, 1u);
        let r: vec4u = extractBits(vu, 1u, 1u);
    }
}

// 16.5.25
// RUN: %metal-compile testFaceForward
@compute @workgroup_size(1)
fn testFaceForward()
{
    // [T < Float, N].(Vector[T, N], Vector[T, N], Vector[T, N]) => Vector[T, N],
    {
        let vf = vec2(2f);
        _ = faceForward(vec2(0), vec2(1),   vec2(2));
        _ = faceForward(vec2(0), vec2(1),   vec2(2.0));
        _ = faceForward(vec2(0), vec2(1.0), vec2(2f));
        let r: vec2f = faceForward(vf, vf, vf);
    }
    {
        let vf = vec3(2f);
        _ = faceForward(vec3(0), vec3(1),   vec3(2));
        _ = faceForward(vec3(0), vec3(1),   vec3(2.0));
        _ = faceForward(vec3(0), vec3(1.0), vec3(2f));
        let r: vec3f = faceForward(vf, vf, vf);
    }
    {
        let vf = vec4(2f);
        _ = faceForward(vec4(0), vec4(1),   vec4(2));
        _ = faceForward(vec4(0), vec4(1),   vec4(2.0));
        _ = faceForward(vec4(0), vec4(1.0), vec4(2f));
        let r: vec4f = faceForward(vf, vf, vf);
    }
}

// 16.5.26 & 16.5.27
// RUN: %metal-compile testFirstLeadingBit
@compute @workgroup_size(1)
fn testFirstLeadingBit()
{
    let i = 0i;
    let u = 0u;

    // signed
    // [].(I32) => I32,
    {
        _ = firstLeadingBit(0);
        _ = firstLeadingBit(0i);
        _ = firstLeadingBit(i);
    }
    // [N].(Vector[I32, N]) => Vector[I32, N],
    {
        _ = firstLeadingBit(vec2(0));
        _ = firstLeadingBit(vec2(0i));
        _ = firstLeadingBit(vec2(i));
    }
    {
        _ = firstLeadingBit(vec3(0));
        _ = firstLeadingBit(vec3(0i));
        _ = firstLeadingBit(vec3(i));
    }
    {
        _ = firstLeadingBit(vec4(0));
        _ = firstLeadingBit(vec4(0i));
        _ = firstLeadingBit(vec4(i));
    }

    // unsigned
    // [].(U32) => U32,
    {
        _ = firstLeadingBit(0);
        _ = firstLeadingBit(0u);
        _ = firstLeadingBit(u);
    }

    // [N].(Vector[U32, N]) => Vector[U32, N],
    {
        _ = firstLeadingBit(vec2(0));
        _ = firstLeadingBit(vec2(0u));
        _ = firstLeadingBit(vec2(u));
    }
    {
        _ = firstLeadingBit(vec3(0));
        _ = firstLeadingBit(vec3(0u));
        _ = firstLeadingBit(vec3(u));
    }
    {
        _ = firstLeadingBit(vec4(0));
        _ = firstLeadingBit(vec4(0u));
        _ = firstLeadingBit(vec4(u));
    }
}

// 16.5.28
// RUN: %metal-compile testFirstTrailingBit
@compute @workgroup_size(1)
fn testFirstTrailingBit()
{
    let i = 0i;
    let u = 0u;

    // [T < ConcreteInteger].(T) => T,
    {
        _ = firstTrailingBit(0);
        _ = firstTrailingBit(0i);
        _ = firstTrailingBit(0u);
        _ = firstTrailingBit(i);
        _ = firstTrailingBit(u);
    }

    // [T < ConcreteInteger, N].(Vector[T, N]) => Vector[T, N],
    {
        _ = firstTrailingBit(vec2(0));
        _ = firstTrailingBit(vec2(0i));
        _ = firstTrailingBit(vec2(0u));
        _ = firstTrailingBit(vec2(i));
        _ = firstTrailingBit(vec2(u));
    }
    {
        _ = firstTrailingBit(vec3(0));
        _ = firstTrailingBit(vec3(0i));
        _ = firstTrailingBit(vec3(0u));
        _ = firstTrailingBit(vec3(i));
        _ = firstTrailingBit(vec3(u));
    }
    {
        _ = firstTrailingBit(vec4(0));
        _ = firstTrailingBit(vec4(0i));
        _ = firstTrailingBit(vec4(0u));
        _ = firstTrailingBit(vec4(i));
        _ = firstTrailingBit(vec4(u));
    }
}

// 16.5.29
// RUN: %metal-compile testFloor
@compute @workgroup_size(1)
fn testFloor()
{
    let f = 0f;
    let h = 0h;

    // [T < Float].(T) => T,
    {
        _ = floor(0);
        _ = floor(0.0);
        _ = floor(1f);
        _ = floor(1h);
        _ = floor(f);
        _ = floor(h);
    }

    // [T < Float, N].(Vector[T, N]) => Vector[T, N],
    {
        _ = floor(vec2(0, 1));
        _ = floor(vec2(0.0, 1.0));
        _ = floor(vec2(1f, 2f));
        _ = floor(vec2(1h));
        _ = floor(vec2(f));
        _ = floor(vec2(h));
    }
    {
        _ = floor(vec3(-1, 0, 1));
        _ = floor(vec3(-1.0, 0.0, 1.0));
        _ = floor(vec3(-1f, 1f, 2f));
        _ = floor(vec3(1h));
        _ = floor(vec3(f));
        _ = floor(vec3(h));
    }
    {
        _ = floor(vec4(-1, 0, 1, 2));
        _ = floor(vec4(-1.0, 0.0, 1.0, 2.0));
        _ = floor(vec4(-1f, 1f, 2f, 3f));
        _ = floor(vec4(1h));
        _ = floor(vec4(f));
        _ = floor(vec4(h));
    }
}

// 16.5.30
// RUN: %metal-compile testFma
@compute @workgroup_size(1)
fn testFma()
{
    let f = 0f;
    let h = 0f;

    // [T < Float].(T, T, T) => T,
    {
        _ = fma(-1, 0, 1);
        _ = fma(-1, 0, 1.0);
        _ = fma(-1, 1, 2f);
        _ = fma(-1, 1, 2h);
        _ = fma(f, f, f);
        _ = fma(h, h, h);
    }
    // [T < Float, N].(Vector[T, N], Vector[T, N], Vector[T, N]) => Vector[T, N],
    {
        _ = fma(vec2(0), vec2(0), vec2(0));
        _ = fma(vec2(0), vec2(0), vec2(0.0));
        _ = fma(vec2(0), vec2(0), vec2(0f));
        _ = fma(vec2(0), vec2(0), vec2(0h));
        _ = fma(vec2(f), vec2(f), vec2(f));
        _ = fma(vec2(h), vec2(h), vec2(h));
    }
    {
        _ = fma(vec3(0), vec3(0), vec3(0));
        _ = fma(vec3(0), vec3(0), vec3(0.0));
        _ = fma(vec3(0), vec3(0), vec3(0f));
        _ = fma(vec3(0), vec3(0), vec3(0h));
        _ = fma(vec3(f), vec3(f), vec3(f));
        _ = fma(vec3(h), vec3(h), vec3(h));
    }
    {
        _ = fma(vec4(0), vec4(0), vec4(0));
        _ = fma(vec4(0), vec4(0), vec4(0.0));
        _ = fma(vec4(0), vec4(0), vec4(0f));
        _ = fma(vec4(0), vec4(0), vec4(0h));
        _ = fma(vec4(f), vec4(f), vec4(f));
        _ = fma(vec4(h), vec4(h), vec4(h));
    }
}

// 16.5.31
// RUN: %metal-compile testFract
@compute @workgroup_size(1)
fn testFract()
{
    let f = 0f;
    let h = 0h;

    // [T < Float].(T) => T,
    {
        _ = fract(0);
        _ = fract(0.0);
        _ = fract(1f);
        _ = fract(1h);
        _ = fract(f);
        _ = fract(h);
    }

    // [T < Float, N].(Vector[T, N]) => Vector[T, N],
    {
        _ = fract(vec2(0));
        _ = fract(vec2(0.0));
        _ = fract(vec2(1f));
        _ = fract(vec2(1h));
        _ = fract(vec2(f));
        _ = fract(vec2(h));
    }
    {
        _ = fract(vec3(-1));
        _ = fract(vec3(-1.0));
        _ = fract(vec3(-1f));
        _ = fract(vec3(-1h));
        _ = fract(vec3(f));
        _ = fract(vec3(h));
    }
    {
        _ = fract(vec4(-1));
        _ = fract(vec4(-1.0));
        _ = fract(vec4(-1f));
        _ = fract(vec4(-1h));
        _ = fract(vec4(f));
        _ = fract(vec4(h));
    }
}

// 16.5.32
// RUN: %metal-compile testFrexp
@compute @workgroup_size(1)
fn testFrexp()
{
    {
      let x: f32 = 1.5;
      let y: f16 = 1.5;
      let r1 = frexp(x);
      let r2 = frexp(y);
      let r3 = frexp(1.5);
      let r4 = frexp(1.5f);
      let r5 = frexp(1.5h);
    }

    {
      let x: vec2<f32> = vec2(1.5);
      let y: vec2<f16> = vec2(1.5);
      let r1 = frexp(x);
      let r2 = frexp(y);
      let r3 = frexp(vec2(1.5));
      let r4 = frexp(vec2(1.5f));
      let r5 = frexp(vec2(1.5h));
    }

    {
      let x: vec3<f32> = vec3(1.5);
      let y: vec3<f16> = vec3(1.5);
      let r1 = frexp(x);
      let r2 = frexp(y);
      let r3 = frexp(vec3(1.5));
      let r4 = frexp(vec3(1.5f));
      let r5 = frexp(vec3(1.5h));
    }

    {
      let x: vec4<f32> = vec4(1.5);
      let y: vec4<f16> = vec4(1.5);
      let r1 = frexp(x);
      let r2 = frexp(y);
      let r3 = frexp(vec4(1.5));
      let r4 = frexp(vec4(1.5f));
    }
}

// 16.5.33
// RUN: %metal-compile testInsertBits
@compute @workgroup_size(1)
fn testInsertBits()
{
    // [T < ConcreteInteger].(T, T, U32, U32) => T,
    {
        let i = 0i;
        let u = 0u;
        let r1: i32 = insertBits(i, i, 0, 0);
        let r2: u32 = insertBits(u, u, 0, 0);
        _ = insertBits(0, 0, 0, 0);
        _ = insertBits(0, 0i, 0, 0);
        _ = insertBits(0, 0u, 0, 0);
    }

    // [T < ConcreteInteger, N].(Vector[T, N], Vector[T, N], U32, U32) => Vector[T, N],
    {
        let vi = vec2(0i);
        let vu = vec2(0u);
        _ = insertBits(vec2(0), vec2(0), 0, 0);
        _ = insertBits(vec2(0), vec2(0i), 0, 0);
        _ = insertBits(vec2(0), vec2(0u), 0, 0);
        let r1: vec2i = insertBits(vi, vi, 0, 0);
        let r2: vec2u = insertBits(vu, vu, 0, 0);
    }
    {
        let vi = vec3(0i);
        let vu = vec3(0u);
        _ = insertBits(vec3(0), vec3(0), 0, 0);
        _ = insertBits(vec3(0), vec3(0i), 0, 0);
        _ = insertBits(vec3(0), vec3(0u), 0, 0);
        let r1: vec3i = insertBits(vi, vi, 0, 0);
        let r2: vec3u = insertBits(vu, vu, 0, 0);
    }
    {
        let vi = vec4(0i);
        let vu = vec4(0u);
        _ = insertBits(vec4(0), vec4(0), 0, 0);
        _ = insertBits(vec4(0), vec4(0i), 0, 0);
        _ = insertBits(vec4(0), vec4(0u), 0, 0);
        let r1: vec4i = insertBits(vi, vi, 0, 0);
        let r2: vec4u = insertBits(vu, vu, 0, 0);
    }
}

// 16.5.34
// RUN: %metal-compile testInverseSqrt
@compute @workgroup_size(1)
fn testInverseSqrt()
{
    // [T < Float].(T) => T,
    let x = 2.f;
    {
        _ = inverseSqrt(2);
        _ = inverseSqrt(2.0);
        _ = inverseSqrt(2f);
        _ = inverseSqrt(x);
    }

    // [T < Float, N].(Vector[T, N]) => Vector[T, N],
    {
        _ = inverseSqrt(vec2(2));
        _ = inverseSqrt(vec2(2.0));
        _ = inverseSqrt(vec2(2f));
        _ = inverseSqrt(vec2(x));
    }
    {
        _ = inverseSqrt(vec3(2));
        _ = inverseSqrt(vec3(2.0));
        _ = inverseSqrt(vec3(2f));
        _ = inverseSqrt(vec3(x));
    }
    {
        _ = inverseSqrt(vec4(2));
        _ = inverseSqrt(vec4(2.0));
        _ = inverseSqrt(vec4(2f));
        _ = inverseSqrt(vec4(x));
    }
}

// 16.5.35
// RUN: %metal-compile testLdexp
@compute @workgroup_size(1)
fn testLdexp()
{
    let f = 0f;
    let h = 0h;

    // [T < ConcreteFloat].(T, I32) => T,
    {
        _ = ldexp(0f, 1);
        _ = ldexp(0h, 1);
        _ = ldexp(f, 1);
        _ = ldexp(h, 1);
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

        _ = ldexp(vec2(0h), vec2(1));
        _ = ldexp(vec3(0h), vec3(1));
        _ = ldexp(vec4(0h), vec4(1));

        _ = ldexp(vec2(f), vec2(1));
        _ = ldexp(vec3(f), vec3(1));
        _ = ldexp(vec4(f), vec4(1));

        _ = ldexp(vec2(h), vec2(1));
        _ = ldexp(vec3(h), vec3(1));
        _ = ldexp(vec4(h), vec4(1));
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

// 16.5.36
// RUN: %metal-compile testLength
@compute @workgroup_size(1)
fn testLength()
{
    let f = 0f;
    let h = 0h;

    // [T < Float].(T) => T,
    {
        _ = length(0);
        _ = length(0.0);
        _ = length(1f);
        _ = length(1h);
        _ = length(f);
        _ = length(h);
    }

    // [T < Float, N].(Vector[T, N]) => Vector[T, N],
    {
        _ = length(vec2(0));
        _ = length(vec2(0.0));
        _ = length(vec2(1f));
        _ = length(vec2(1h));
        _ = length(vec2(f));
        _ = length(vec2(h));
    }
    {
        _ = length(vec3(-1));
        _ = length(vec3(-1.0));
        _ = length(vec3(-1f));
        _ = length(vec3(-1h));
        _ = length(vec3(f));
        _ = length(vec3(h));
    }
    {
        _ = length(vec4(-1));
        _ = length(vec4(-1.0));
        _ = length(vec4(-1f));
        _ = length(vec4(-1h));
        _ = length(vec4(f));
        _ = length(vec4(h));
    }
}

// 16.5.37
// RUN: %metal-compile testLog
@compute @workgroup_size(1)
fn testLog()
{
    let f = 0f;
    let h = 0h;

    // [T < Float].(T) => T,
    {
        _ = log(2);
        _ = log(1.0);
        _ = log(1f);
        _ = log(1h);
        _ = log(f);
        _ = log(h);
    }

    // [T < Float, N].(Vector[T, N]) => Vector[T, N],
    {
        _ = log(vec2(2));
        _ = log(vec2(2.0));
        _ = log(vec2(2f));
        _ = log(vec2(2h));
        _ = log(vec2(f));
        _ = log(vec2(h));
    }
    {
        _ = log(vec3(2));
        _ = log(vec3(2.0));
        _ = log(vec3(2f));
        _ = log(vec3(2h));
        _ = log(vec3(f));
        _ = log(vec3(h));
    }
    {
        _ = log(vec4(2));
        _ = log(vec4(2.0));
        _ = log(vec4(2f));
        _ = log(vec4(2h));
        _ = log(vec4(f));
        _ = log(vec4(h));
    }
}

// 16.5.38
// RUN: %metal-compile testLog2
@compute @workgroup_size(1)
fn testLog2()
{
    let f = 0f;
    let h = 0h;

    // [T < Float].(T) => T,
    {
        _ = log2(2);
        _ = log2(2.0);
        _ = log2(2f);
        _ = log2(2h);
        _ = log2(f);
        _ = log2(h);
    }

    // [T < Float, N].(Vector[T, N]) => Vector[T, N],
    {
        _ = log2(vec2(2));
        _ = log2(vec2(2.0));
        _ = log2(vec2(2f));
        _ = log2(vec2(2h));
        _ = log2(vec2(f));
        _ = log2(vec2(h));
    }
    {
        _ = log2(vec3(2));
        _ = log2(vec3(2.0));
        _ = log2(vec3(2f));
        _ = log2(vec3(2h));
        _ = log2(vec3(f));
        _ = log2(vec3(h));
    }
    {
        _ = log2(vec4(2));
        _ = log2(vec4(2.0));
        _ = log2(vec4(2f));
        _ = log2(vec4(2h));
        _ = log2(vec4(f));
        _ = log2(vec4(h));
    }
}

// 16.5.39
// RUN: %metal-compile testMax
@compute @workgroup_size(1)
fn testMax()
{
    let i = 1i;
    let u = 1u;
    let f = 1f;
    let h = 1h;

    // [T < Number].(T, T) => T,
    {
        _ = max(-1, 0);
        _ = max(-1, 1i);
        _ = max(0, 1u);
        _ = max(-1, 0.0);
        _ = max(-1, 1f);
        _ = max(-1, 1h);
        _ = max(-1, i);
        _ = max(0, u);
        _ = max(-1, f);
        _ = max(-1, h);
    }
    // [T < Number, N].(Vector[T, N], Vector[T, N]) => Vector[T, N],
    {
        _ = max(vec2(0), vec2(0));
        _ = max(vec2(0), vec2(0i));
        _ = max(vec2(0), vec2(0u));
        _ = max(vec2(0), vec2(0.0));
        _ = max(vec2(0), vec2(0f));
        _ = max(vec2(0), vec2(0h));
        _ = max(vec2(0), vec2(i));
        _ = max(vec2(0), vec2(u));
        _ = max(vec2(0), vec2(f));
        _ = max(vec2(0), vec2(h));
    }
    {
        _ = max(vec3(0), vec3(0));
        _ = max(vec3(0), vec3(0i));
        _ = max(vec3(0), vec3(0u));
        _ = max(vec3(0), vec3(0.0));
        _ = max(vec3(0), vec3(0f));
        _ = max(vec3(0), vec3(0h));
        _ = max(vec3(0), vec3(i));
        _ = max(vec3(0), vec3(u));
        _ = max(vec3(0), vec3(f));
        _ = max(vec3(0), vec3(h));
    }
    {
        _ = max(vec4(0), vec4(0));
        _ = max(vec4(0), vec4(0i));
        _ = max(vec4(0), vec4(0u));
        _ = max(vec4(0), vec4(0.0));
        _ = max(vec4(0), vec4(0f));
        _ = max(vec4(0), vec4(0h));
        _ = max(vec4(0), vec4(i));
        _ = max(vec4(0), vec4(u));
        _ = max(vec4(0), vec4(f));
        _ = max(vec4(0), vec4(h));
    }
}

// 16.5.40
// RUN: %metal-compile testMin
@compute @workgroup_size(1)
fn testMin()
{
    let i = 1i;
    let u = 1u;
    let f = 1f;
    let h = 1h;

    // [T < Number].(T, T) => T,
    {
        _ = min(-1, 0);
        _ = min(-1, 1i);
        _ = min(0, 1u);
        _ = min(-1, 0.0);
        _ = min(-1, 1f);
        _ = min(-1, 1h);
    }
    // [T < Number, N].(Vector[T, N], Vector[T, N]) => Vector[T, N],
    {
        _ = min(vec2(0), vec2(0));
        _ = min(vec2(0), vec2(0i));
        _ = min(vec2(0), vec2(0u));
        _ = min(vec2(0), vec2(0.0));
        _ = min(vec2(0), vec2(0f));
        _ = min(vec2(0), vec2(0h));
        _ = min(vec2(0), vec2(i));
        _ = min(vec2(0), vec2(u));
        _ = min(vec2(0), vec2(f));
        _ = min(vec2(0), vec2(h));
    }
    {
        _ = min(vec3(0), vec3(0));
        _ = min(vec3(0), vec3(0i));
        _ = min(vec3(0), vec3(0u));
        _ = min(vec3(0), vec3(0.0));
        _ = min(vec3(0), vec3(0f));
        _ = min(vec3(0), vec3(0h));
        _ = min(vec3(0), vec3(i));
        _ = min(vec3(0), vec3(u));
        _ = min(vec3(0), vec3(f));
        _ = min(vec3(0), vec3(h));
    }
    {
        _ = min(vec4(0), vec4(0));
        _ = min(vec4(0), vec4(0i));
        _ = min(vec4(0), vec4(0u));
        _ = min(vec4(0), vec4(0.0));
        _ = min(vec4(0), vec4(0f));
        _ = min(vec4(0), vec4(0h));
        _ = min(vec4(0), vec4(i));
        _ = min(vec4(0), vec4(u));
        _ = min(vec4(0), vec4(f));
        _ = min(vec4(0), vec4(h));
    }
}

// 16.5.41
// RUN: %metal-compile testMix
@compute @workgroup_size(1)
fn testMix()
{
    let f = 0f;
    let h = 0h;

    // [T < Float].(T, T, T) => T,
    {
        _ = mix(-1, 0, 1);
        _ = mix(-1, 0, 1.0);
        _ = mix(-1, 1, 2f);
        _ = mix(-1, 1, 2h);
        _ = mix(-1, 1, f);
        _ = mix(-1, 1, h);
    }
    // [T < Float, N].(Vector[T, N], Vector[T, N], Vector[T, N]) => Vector[T, N],
    {
        _ = mix(vec2(0), vec2(0), vec2(0));
        _ = mix(vec2(0), vec2(0), vec2(0.0));
        _ = mix(vec2(0), vec2(0), vec2(0f));
        _ = mix(vec2(0), vec2(0), vec2(0h));
        _ = mix(vec2(0), vec2(0), vec2(f));
        _ = mix(vec2(0), vec2(0), vec2(h));
    }
    {
        _ = mix(vec3(0), vec3(0), vec3(0));
        _ = mix(vec3(0), vec3(0), vec3(0.0));
        _ = mix(vec3(0), vec3(0), vec3(0f));
        _ = mix(vec3(0), vec3(0), vec3(0h));
        _ = mix(vec3(0), vec3(0), vec3(f));
        _ = mix(vec3(0), vec3(0), vec3(h));
    }
    {
        _ = mix(vec4(0), vec4(0), vec4(0));
        _ = mix(vec4(0), vec4(0), vec4(0.0));
        _ = mix(vec4(0), vec4(0), vec4(0f));
        _ = mix(vec4(0), vec4(0), vec4(0h));
        _ = mix(vec4(0), vec4(0), vec4(f));
        _ = mix(vec4(0), vec4(0), vec4(h));
    }
    // [T < Float, N].(Vector[T, N], Vector[T, N], T) => Vector[T, N],
    {
        _ = mix(vec2(0), vec2(0), 0);
        _ = mix(vec2(0), vec2(0), 0.0);
        _ = mix(vec2(0), vec2(0), 0f);
        _ = mix(vec2(0), vec2(0), 0h);
        _ = mix(vec2(0), vec2(0), f);
        _ = mix(vec2(0), vec2(0), h);
    }
    {
        _ = mix(vec3(0), vec3(0), 0);
        _ = mix(vec3(0), vec3(0), 0.0);
        _ = mix(vec3(0), vec3(0), 0f);
        _ = mix(vec3(0), vec3(0), 0h);
        _ = mix(vec3(0), vec3(0), f);
        _ = mix(vec3(0), vec3(0), h);
    }
    {
        _ = mix(vec4(0), vec4(0), 0);
        _ = mix(vec4(0), vec4(0), 0.0);
        _ = mix(vec4(0), vec4(0), 0f);
        _ = mix(vec4(0), vec4(0), 0h);
        _ = mix(vec4(0), vec4(0), f);
        _ = mix(vec4(0), vec4(0), h);
    }
}

// 16.5.42
// RUN: %metal-compile testModf
@compute @workgroup_size(1)
fn testModf()
{
    {
      let x: f32 = 1.5;
      let y: f16 = 1.5;
      let r1 = modf(x);
      let r2 = modf(y);
      const r3 = modf(1.5);
      const r4 = modf(1.5f);
      const r5 = modf(1.5h);

      let r6: f32 = modf(x).fract;
      let r7: f16 = modf(y).whole;
      const r8: f32 = modf(1.5).fract;
      const r9: f16 = modf(1.5).whole;
      const r10: f32 = modf(1.5f).fract;
      const r11: f16 = modf(1.5h).whole;
    }

    {
      let x: vec2<f32> = vec2(1.5);
      let y: vec2<f16> = vec2(1.5);
      let r1 = modf(x);
      let r2 = modf(y);
      const r3 = modf(vec2(1.5));
      const r4 = modf(vec2(1.5f));
      const r5 = modf(vec2(1.5h));

      let r6: vec2<f32> = modf(x).fract;
      let r7: vec2<f16> = modf(y).whole;
      const r8: vec2<f32> = modf(vec2(1.5)).fract;
      const r9: vec2<f16> = modf(vec2(1.5)).whole;
      const r10: vec2<f32> = modf(vec2(1.5f)).fract;
      const r11: vec2<f16> = modf(vec2(1.5h)).whole;
    }

    {
      let x: vec3<f32> = vec3(1.5);
      let y: vec3<f16> = vec3(1.5);
      let r1 = modf(x);
      let r2 = modf(y);
      const r3 = modf(vec3(1.5));
      const r4 = modf(vec3(1.5f));
      const r5 = modf(vec3(1.5h));

      let r6: vec3<f32> = modf(x).fract;
      let r7: vec3<f16> = modf(y).whole;
      const r8: vec3<f32> = modf(vec3(1.5)).fract;
      const r9: vec3<f16> = modf(vec3(1.5)).whole;
      const r10: vec3<f32> = modf(vec3(1.5f)).fract;
      const r11: vec3<f16> = modf(vec3(1.5h)).whole;
    }

    {
      let x: vec4<f32> = vec4(1.5);
      let y: vec4<f16> = vec4(1.5);
      let r1 = modf(x);
      let r2 = modf(y);
      const r3 = modf(vec4(1.5));
      const r4 = modf(vec4(1.5f));
      const r5 = modf(vec4(1.5h));

      let r6: vec4<f32> = modf(x).fract;
      let r7: vec4<f16> = modf(y).whole;
      const r8: vec4<f32> = modf(vec4(1.5)).fract;
      const r9: vec4<f16> = modf(vec4(1.5)).whole;
      const r10: vec4<f32> = modf(vec4(1.5f)).fract;
      const r11: vec4<f16> = modf(vec4(1.5h)).whole;
    }
}

// 16.5.43
// RUN: %metal-compile testNormalize
@compute @workgroup_size(1)
fn testNormalize()
{
    let f = 0f;
    let h = 0h;

    // [T < Float, N].(Vector[T, N]) => Vector[T, N],
    {
        _ = normalize(vec2(1));
        _ = normalize(vec2(1.0));
        _ = normalize(vec2(1f));
        _ = normalize(vec2(1h));
        _ = normalize(vec2(f));
        _ = normalize(vec2(h));
    }
    {
        _ = normalize(vec3(-1));
        _ = normalize(vec3(-1.0));
        _ = normalize(vec3(-1f));
        _ = normalize(vec3(-1h));
        _ = normalize(vec3(f));
        _ = normalize(vec3(h));
    }
    {
        _ = normalize(vec4(-1));
        _ = normalize(vec4(-1.0));
        _ = normalize(vec4(-1f));
        _ = normalize(vec4(-1h));
        _ = normalize(vec4(f));
        _ = normalize(vec4(h));
    }
}

// 16.5.44
// RUN: %metal-compile testPow
@compute @workgroup_size(1)
fn testPow()
{
    let f = 0f;
    let h = 0h;

    // [T < Float].(T, T) => T,
    {
        _ = pow(0, 1);
        _ = pow(0, 1.0);
        _ = pow(0, 1f);
        _ = pow(0, 1h);
        _ = pow(0, f);
        _ = pow(0, h);
        _ = pow(0.0, 1.0);
        _ = pow(1.0, 2f);
        _ = pow(1.0, 2h);
        _ = pow(1.0, f);
        _ = pow(1.0, h);
        _ = pow(1f, 2f);
        _ = pow(1h, 2h);
        _ = pow(1f, f);
        _ = pow(1h, h);
    }
    // [T < Float, N].(Vector[T, N], Vector[T, N]) => Vector[T, N],
    {
        _ = pow(vec2(0),   vec2(1)  );
        _ = pow(vec2(0),   vec2(0.0));
        _ = pow(vec2(0),   vec2(1f) );
        _ = pow(vec2(0),   vec2(1h) );
        _ = pow(vec2(0),   vec2(f) );
        _ = pow(vec2(0),   vec2(h) );
        _ = pow(vec2(0.0), vec2(0.0));
        _ = pow(vec2(0.0), vec2(0f) );
        _ = pow(vec2(0.0), vec2(0h) );
        _ = pow(vec2(0.0), vec2(f) );
        _ = pow(vec2(0.0), vec2(h) );
        _ = pow(vec2(1f),  vec2(1f) );
        _ = pow(vec2(1h),  vec2(1h) );
        _ = pow(vec2(1f),  vec2(f) );
        _ = pow(vec2(1h),  vec2(h) );
    }
    {
        _ = pow(vec3(0),   vec3(1)  );
        _ = pow(vec3(0),   vec3(0.0));
        _ = pow(vec3(0),   vec3(1f) );
        _ = pow(vec3(0),   vec3(1h) );
        _ = pow(vec3(0),   vec3(f) );
        _ = pow(vec3(0),   vec3(h) );
        _ = pow(vec3(0.0), vec3(0.0));
        _ = pow(vec3(0.0), vec3(0f) );
        _ = pow(vec3(0.0), vec3(0h) );
        _ = pow(vec3(0.0), vec3(f) );
        _ = pow(vec3(0.0), vec3(h) );
        _ = pow(vec3(1f),  vec3(1f) );
        _ = pow(vec3(1h),  vec3(1h) );
        _ = pow(vec3(1f),  vec3(f) );
        _ = pow(vec3(1h),  vec3(h) );
    }
    {
        _ = pow(vec4(0),   vec4(1)  );
        _ = pow(vec4(0),   vec4(0.0));
        _ = pow(vec4(0),   vec4(1f) );
        _ = pow(vec4(0),   vec4(1h) );
        _ = pow(vec4(0),   vec4(f) );
        _ = pow(vec4(0),   vec4(h) );
        _ = pow(vec4(0.0), vec4(0.0));
        _ = pow(vec4(0.0), vec4(0f) );
        _ = pow(vec4(0.0), vec4(0h) );
        _ = pow(vec4(0.0), vec4(f) );
        _ = pow(vec4(0.0), vec4(h) );
        _ = pow(vec4(1f),  vec4(1f) );
        _ = pow(vec4(1h),  vec4(1h) );
        _ = pow(vec4(1f),  vec4(f) );
        _ = pow(vec4(1h),  vec4(h) );
    }
}

// 16.5.45
// RUN: %metal-compile testQuantizeToF16
@compute @workgroup_size(1)
fn testQuantizeToF16()
{
    // [].(F32) => F32,
    {
        let x = 0f;
        _ = quantizeToF16(x);
        _ = quantizeToF16(0);
        _ = quantizeToF16(0.0);
        _ = quantizeToF16(0f);
    }

    // [N].(Vector[F32, N]) => Vector[F32, N],
    {
        let x = vec2(0f);
        _ = quantizeToF16(x);
        _ = quantizeToF16(vec2(0));
        _ = quantizeToF16(vec2(0.0));
        _ = quantizeToF16(vec2(0f));
    }
    {
        let x = vec3(0f);
        _ = quantizeToF16(x);
        _ = quantizeToF16(vec3(0));
        _ = quantizeToF16(vec3(0.0));
        _ = quantizeToF16(vec3(0f));
    }
    {
        let x = vec4(0f);
        _ = quantizeToF16(x);
        _ = quantizeToF16(vec4(0));
        _ = quantizeToF16(vec4(0.0));
        _ = quantizeToF16(vec4(0f));
    }
}

// 16.5.46
// RUN: %metal-compile testRadians
@compute @workgroup_size(1)
fn testRadians()
{
    let f = 0f;
    let h = 0h;

    // [T < Float].(T) => T,
    {
        _ = radians(0);
        _ = radians(0.0);
        _ = radians(1f);
        _ = radians(1h);
        _ = radians(f);
        _ = radians(h);
    }

    // [T < Float, N].(Vector[T, N]) => Vector[T, N],
    {
        _ = radians(vec2(0));
        _ = radians(vec2(0.0));
        _ = radians(vec2(1f));
        _ = radians(vec2(1h));
        _ = radians(vec2(f));
        _ = radians(vec2(h));
    }
    {
        _ = radians(vec3(-1));
        _ = radians(vec3(-1.0));
        _ = radians(vec3(-1f));
        _ = radians(vec3(-1h));
        _ = radians(vec3(f));
        _ = radians(vec3(h));
    }
    {
        _ = radians(vec4(-1));
        _ = radians(vec4(-1.0));
        _ = radians(vec4(-1f));
        _ = radians(vec4(-1h));
        _ = radians(vec4(f));
        _ = radians(vec4(h));
    }
}

// 16.5.47
// RUN: %metal-compile testReflect
@compute @workgroup_size(1)
fn testReflect()
{
    let f = 0f;
    let h = 0h;
    // [T < Float, N].(Vector[T, N], Vector[T, N]) => Vector[T, N],
    {
        _ = reflect(vec2(0),   vec2(1)  );
        _ = reflect(vec2(0),   vec2(0.0));
        _ = reflect(vec2(0),   vec2(1f) );
        _ = reflect(vec2(0),   vec2(1h) );
        _ = reflect(vec2(0),   vec2(f) );
        _ = reflect(vec2(0),   vec2(h) );
        _ = reflect(vec2(0.0), vec2(0.0));
        _ = reflect(vec2(0.0), vec2(0f) );
        _ = reflect(vec2(0.0), vec2(0h) );
        _ = reflect(vec2(0.0), vec2(f) );
        _ = reflect(vec2(0.0), vec2(h) );
        _ = reflect(vec2(1f),  vec2(1f) );
        _ = reflect(vec2(1h),  vec2(1h) );
        _ = reflect(vec2(1f),  vec2(f) );
        _ = reflect(vec2(1h),  vec2(h) );
    }
    {
        _ = reflect(vec3(0),   vec3(1)  );
        _ = reflect(vec3(0),   vec3(0.0));
        _ = reflect(vec3(0),   vec3(1f) );
        _ = reflect(vec3(0),   vec3(1h) );
        _ = reflect(vec3(0),   vec3(f) );
        _ = reflect(vec3(0),   vec3(h) );
        _ = reflect(vec3(0.0), vec3(0.0));
        _ = reflect(vec3(0.0), vec3(0f) );
        _ = reflect(vec3(0.0), vec3(0h) );
        _ = reflect(vec3(0.0), vec3(f) );
        _ = reflect(vec3(0.0), vec3(h) );
        _ = reflect(vec3(1f),  vec3(1f) );
        _ = reflect(vec3(1h),  vec3(1h) );
        _ = reflect(vec3(1f),  vec3(f) );
        _ = reflect(vec3(1h),  vec3(h) );
    }
    {
        _ = reflect(vec4(0),   vec4(1)  );
        _ = reflect(vec4(0),   vec4(0.0));
        _ = reflect(vec4(0),   vec4(1f) );
        _ = reflect(vec4(0),   vec4(1h) );
        _ = reflect(vec4(0),   vec4(f) );
        _ = reflect(vec4(0),   vec4(h) );
        _ = reflect(vec4(0.0), vec4(0.0));
        _ = reflect(vec4(0.0), vec4(0f) );
        _ = reflect(vec4(0.0), vec4(0h) );
        _ = reflect(vec4(0.0), vec4(f) );
        _ = reflect(vec4(0.0), vec4(h) );
        _ = reflect(vec4(1f),  vec4(1f) );
        _ = reflect(vec4(1h),  vec4(1h) );
        _ = reflect(vec4(1f),  vec4(f) );
        _ = reflect(vec4(1h),  vec4(h) );
    }
}

// 16.5.48
// RUN: %metal-compile testRefract
@compute @workgroup_size(1)
fn testRefract()
{
    let f = 0f;
    let h = 0h;

    // [T < Float, N].(Vector[T, N], Vector[T, N], T) => Vector[T, N],
    {
        _ = refract(vec2(0), vec2(0), 1);
        _ = refract(vec2(0), vec2(0), 0.0);
        _ = refract(vec2(0), vec2(0), 1f);
        _ = refract(vec2(0), vec2(0), 1h);
        _ = refract(vec2(0), vec2(0), f);
        _ = refract(vec2(0), vec2(0), h);
    }
    {
        _ = refract(vec3(0), vec3(0), 1);
        _ = refract(vec3(0), vec3(0), 0.0);
        _ = refract(vec3(0), vec3(0), 1f);
        _ = refract(vec3(0), vec3(0), 1h);
        _ = refract(vec3(0), vec3(0), f);
        _ = refract(vec3(0), vec3(0), h);
    }
    {
        _ = refract(vec4(0), vec4(0), 1);
        _ = refract(vec4(0), vec4(0), 0.0);
        _ = refract(vec4(0), vec4(0), 1f);
        _ = refract(vec4(0), vec4(0), 1h);
        _ = refract(vec4(0), vec4(0), f);
        _ = refract(vec4(0), vec4(0), h);
    }
}

// 16.5.49
// RUN: %metal-compile testReverseBits
@compute @workgroup_size(1)
fn testReverseBits()
{
    // [T < ConcreteInteger].(T) => T,
    {
        let i = 0i;
        let u = 0u;
        _ = reverseBits(0);
        _ = reverseBits(0i);
        _ = reverseBits(0u);
        let r1: i32 = reverseBits(i);
        let r2: u32 = reverseBits(u);
    }
    // [T < ConcreteInteger, N].(Vector[T, N]) => Vector[T, N],
    {
        let vi = vec2(0i);
        let vu = vec2(0u);
        _ = reverseBits(vec2(0));
        _ = reverseBits(vec2(0i));
        _ = reverseBits(vec2(0u));
        let r1: vec2i = reverseBits(vi);
        let r2: vec2u = reverseBits(vu);
    }
    {
        let vi = vec3(0i);
        let vu = vec3(0u);
        _ = reverseBits(vec3(0));
        _ = reverseBits(vec3(0i));
        _ = reverseBits(vec3(0u));
        let r1: vec3i = reverseBits(vi);
        let r2: vec3u = reverseBits(vu);
    }
    {
        let vi = vec4(0i);
        let vu = vec4(0u);
        _ = reverseBits(vec4(0));
        _ = reverseBits(vec4(0i));
        _ = reverseBits(vec4(0u));
        let r1: vec4i = reverseBits(vi);
        let r2: vec4u = reverseBits(vu);
    }
}

// 16.5.50
// RUN: %metal-compile testRound
@compute @workgroup_size(1)
fn testRound()
{
    let f = 0f;
    let h = 0h;

    // [T < Float].(T) => T,
    {
        _ = round(0);
        _ = round(0.0);
        _ = round(1f);
        _ = round(1h);
        _ = round(f);
        _ = round(h);
    }

    // [T < Float, N].(Vector[T, N]) => Vector[T, N],
    {
        _ = round(vec2(0));
        _ = round(vec2(0.0));
        _ = round(vec2(1f));
        _ = round(vec2(1h));
        _ = round(vec2(f));
        _ = round(vec2(h));
    }
    {
        _ = round(vec3(-1));
        _ = round(vec3(-1.0));
        _ = round(vec3(-1f));
        _ = round(vec3(-1h));
        _ = round(vec3(f));
        _ = round(vec3(h));
    }
    {
        _ = round(vec4(-1));
        _ = round(vec4(-1.0));
        _ = round(vec4(-1f));
        _ = round(vec4(-1h));
        _ = round(vec4(f));
        _ = round(vec4(h));
    }
}

// 16.5.51
// RUN: %metal-compile testSaturate
@compute @workgroup_size(1)
fn testSaturate()
{
    let f = 0f;
    let h = 0h;

    // [T < Float].(T) => T,
    {
        _ = saturate(0);
        _ = saturate(0.0);
        _ = saturate(1f);
        _ = saturate(1h);
        _ = saturate(f);
        _ = saturate(h);
    }

    // [T < Float, N].(Vector[T, N]) => Vector[T, N],
    {
        _ = saturate(vec2(0));
        _ = saturate(vec2(0.0));
        _ = saturate(vec2(1f));
        _ = saturate(vec2(1h));
        _ = saturate(vec2(f));
        _ = saturate(vec2(h));
    }
    {
        _ = saturate(vec3(-1));
        _ = saturate(vec3(-1.0));
        _ = saturate(vec3(-1f));
        _ = saturate(vec3(-1h));
        _ = saturate(vec3(f));
        _ = saturate(vec3(h));
    }
    {
        _ = saturate(vec4(-1));
        _ = saturate(vec4(-1.0));
        _ = saturate(vec4(-1f));
        _ = saturate(vec4(-1h));
        _ = saturate(vec4(f));
        _ = saturate(vec4(h));
    }
}

// 16.5.52
// RUN: %metal-compile testSign
@compute @workgroup_size(1)
fn testSign()
{
    let i = 0i;
    let f = 0f;
    let h = 0h;

    // [T < SignedNumber].(T) => T,
    {
        _ = sign(0);
        _ = sign(0i);
        _ = sign(0.0);
        _ = sign(1f);
        _ = sign(1h);
        _ = sign(i);
        _ = sign(f);
        _ = sign(h);
    }

    // [T < SignedNumber, N].(Vector[T, N]) => Vector[T, N],
    {
        _ = sign(vec2(0));
        _ = sign(vec2(0i));
        _ = sign(vec2(0.0));
        _ = sign(vec2(1f));
        _ = sign(vec2(1h));
        _ = sign(vec2(i));
        _ = sign(vec2(f));
        _ = sign(vec2(h));
    }
    {
        _ = sign(vec3(-1));
        _ = sign(vec3(-1i));
        _ = sign(vec3(-1.0));
        _ = sign(vec3(-1f));
        _ = sign(vec3(-1h));
        _ = sign(vec3(i));
        _ = sign(vec3(f));
        _ = sign(vec3(h));
    }
    {
        _ = sign(vec4(-1));
        _ = sign(vec4(-1i));
        _ = sign(vec4(-1.0));
        _ = sign(vec4(-1f));
        _ = sign(vec4(-1h));
        _ = sign(vec4(i));
        _ = sign(vec4(f));
        _ = sign(vec4(h));
    }
}

// 16.5.53. sin
// 16.5.54. sinh
// Tested in testTrigonometric and testTrigonometricHyperbolic

// 16.5.55
// RUN: %metal-compile testSmoothstep
@compute @workgroup_size(1)
fn testSmoothstep()
{
    let f = 0f;
    let h = 0h;

    // [T < Float].(T, T, T) => T,
    {
        _ = smoothstep(-1, 0, 1);
        _ = smoothstep(-1, 0, 1.0);
        _ = smoothstep(-1, 1, 2f);
        _ = smoothstep(-1, 1, 2h);
        _ = smoothstep(-1, 1, f);
        _ = smoothstep(-1, 1, h);
    }
    // [T < Float, N].(Vector[T, N], Vector[T, N], Vector[T, N]) => Vector[T, N],
    {
        _ = smoothstep(vec2(2), vec2(1), vec2(1));
        _ = smoothstep(vec2(2), vec2(1), vec2(1.0));
        _ = smoothstep(vec2(2), vec2(1), vec2(1f));
        _ = smoothstep(vec2(2), vec2(1), vec2(1h));
        _ = smoothstep(vec2(2), vec2(1), vec2(f));
        _ = smoothstep(vec2(2), vec2(1), vec2(h));
    }
    {
        _ = smoothstep(vec3(2), vec3(1), vec3(1));
        _ = smoothstep(vec3(2), vec3(1), vec3(1.0));
        _ = smoothstep(vec3(2), vec3(1), vec3(1f));
        _ = smoothstep(vec3(2), vec3(1), vec3(1h));
        _ = smoothstep(vec3(2), vec3(1), vec3(f));
        _ = smoothstep(vec3(2), vec3(1), vec3(h));
    }
    {
        _ = smoothstep(vec4(2), vec4(1), vec4(1));
        _ = smoothstep(vec4(2), vec4(1), vec4(1.0));
        _ = smoothstep(vec4(2), vec4(1), vec4(1f));
        _ = smoothstep(vec4(2), vec4(1), vec4(1h));
        _ = smoothstep(vec4(2), vec4(1), vec4(f));
        _ = smoothstep(vec4(2), vec4(1), vec4(h));
    }
}

// 16.5.56
// RUN: %metal-compile testSqrt
@compute @workgroup_size(1)
fn testSqrt()
{
    let f = 0f;
    let h = 0h;

    // [T < Float].(T) => T,
    {
        _ = sqrt(0);
        _ = sqrt(0.0);
        _ = sqrt(1f);
        _ = sqrt(1h);
        _ = sqrt(f);
        _ = sqrt(h);
    }

    // [T < Float, N].(Vector[T, N]) => Vector[T, N],
    {
        _ = sqrt(vec2(0));
        _ = sqrt(vec2(0.0));
        _ = sqrt(vec2(1f));
        _ = sqrt(vec2(1h));
        _ = sqrt(vec2(f));
        _ = sqrt(vec2(h));
    }
    {
        _ = sqrt(vec3(1));
        _ = sqrt(vec3(1.0));
        _ = sqrt(vec3(1f));
        _ = sqrt(vec3(1h));
        _ = sqrt(vec3(f));
        _ = sqrt(vec3(h));
    }
    {
        _ = sqrt(vec4(1));
        _ = sqrt(vec4(1.0));
        _ = sqrt(vec4(1f));
        _ = sqrt(vec4(1h));
        _ = sqrt(vec4(f));
        _ = sqrt(vec4(h));
    }
}

// 16.5.57
// RUN: %metal-compile testStep
@compute @workgroup_size(1)
fn testStep()
{
    let f = 0f;
    let h = 0h;

    // [T < Float].(T, T) => T,
    {
        _ = step(0, 1);
        _ = step(0, 1.0);
        _ = step(1, 2f);
        _ = step(1, 2h);
        _ = step(1, f);
        _ = step(1, h);
    }
    // [T < Float, N].(Vector[T, N], Vector[T, N], Vector[T, N]) => Vector[T, N],
    {
        _ = step(vec2(0), vec2(0));
        _ = step(vec2(0), vec2(0.0));
        _ = step(vec2(0), vec2(0f));
        _ = step(vec2(0), vec2(0h));
        _ = step(vec2(0), vec2(f));
        _ = step(vec2(0), vec2(h));
    }
    {
        _ = step(vec3(0), vec3(0));
        _ = step(vec3(0), vec3(0.0));
        _ = step(vec3(0), vec3(0f));
        _ = step(vec3(0), vec3(0h));
        _ = step(vec3(0), vec3(f));
        _ = step(vec3(0), vec3(h));
    }
    {
        _ = step(vec4(0), vec4(0));
        _ = step(vec4(0), vec4(0.0));
        _ = step(vec4(0), vec4(0f));
        _ = step(vec4(0), vec4(0h));
        _ = step(vec4(0), vec4(f));
        _ = step(vec4(0), vec4(h));
    }
}

// 16.5.58. tan
// 16.5.59. tanh
// Tested in testTrigonometric and testTrigonometricHyperbolic

// 16.5.60
// RUN: %metal-compile testTranspose
@compute @workgroup_size(1)
fn testTranspose()
{
    let f = 0f;
    let h = 0h;
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
    {
        _ = transpose(mat2x2(0, 0, 0, f));
        _ = transpose(mat2x3(0, 0, 0, 0, 0, f));
        _ = transpose(mat2x4(0, 0, 0, 0, 0, 0, 0, f));
        _ = transpose(mat3x2(0, 0, 0, 0, 0, f));
        _ = transpose(mat3x3(0, 0, 0, 0, 0, 0, 0, 0, f));
        _ = transpose(mat3x4(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, f));
        _ = transpose(mat4x2(0, 0, 0, 0, 0, 0, 0, f));
        _ = transpose(mat4x3(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, f));
        _ = transpose(mat4x4(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, f));
    }
    {
        _ = transpose(mat2x2(0, 0, 0, h));
        _ = transpose(mat2x3(0, 0, 0, 0, 0, h));
        _ = transpose(mat2x4(0, 0, 0, 0, 0, 0, 0, h));
        _ = transpose(mat3x2(0, 0, 0, 0, 0, h));
        _ = transpose(mat3x3(0, 0, 0, 0, 0, 0, 0, 0, h));
        _ = transpose(mat3x4(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, h));
        _ = transpose(mat4x2(0, 0, 0, 0, 0, 0, 0, h));
        _ = transpose(mat4x3(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, h));
        _ = transpose(mat4x4(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, h));
    }
}

// 16.5.61
// RUN: %metal-compile testTrunc
@compute @workgroup_size(1)
fn testTrunc()
{
    let f = 0f;
    let h = 0h;
    // [T < Float].(T) => T,
    {
        _ = trunc(0);
        _ = trunc(0.0);
        _ = trunc(1f);
        _ = trunc(1h);
        _ = trunc(f);
        _ = trunc(h);
    }

    // [T < Float, N].(Vector[T, N]) => Vector[T, N],
    {
        _ = trunc(vec2(0));
        _ = trunc(vec2(0.0));
        _ = trunc(vec2(1f));
        _ = trunc(vec2(1h));
        _ = trunc(vec2(f));
        _ = trunc(vec2(h));
    }
    {
        _ = trunc(vec3(-1));
        _ = trunc(vec3(-1.0));
        _ = trunc(vec3(-1f));
        _ = trunc(vec3(-1h));
        _ = trunc(vec3(f));
        _ = trunc(vec3(h));
    }
    {
        _ = trunc(vec4(-1));
        _ = trunc(vec4(-1.0));
        _ = trunc(vec4(-1f));
        _ = trunc(vec4(-1h));
        _ = trunc(vec4(f));
        _ = trunc(vec4(h));
    }
}

// 16.6. Derivative Built-in Functions (https://www.w3.org/TR/WGSL/#derivative-builtin-functions)
// RUN: %metal-compile testDerivativeFunctions
@fragment
fn testDerivativeFunctions()
{
    // All have the same signatures:
    //   [].(f32) => f32,
    //   [N].(vec[N][f32]) => vec[N][f32],

    // 16.6.1 dpdx
    _ = dpdx(2.0);
    _ = dpdx(vec2(2.0));
    _ = dpdx(vec3(2.0));
    _ = dpdx(vec4(2.0));

    // 16.6.2 dpdxCoarse
    _ = dpdxCoarse(2.0);
    _ = dpdxCoarse(vec2(2.0));
    _ = dpdxCoarse(vec3(2.0));
    _ = dpdxCoarse(vec4(2.0));

    // 16.6.3 dpdxFine
    _ = dpdxFine(2.0);
    _ = dpdxFine(vec2(2.0));
    _ = dpdxFine(vec3(2.0));
    _ = dpdxFine(vec4(2.0));

    // 16.6.4 dpdy
    _ = dpdy(2.0);
    _ = dpdy(vec2(2.0));
    _ = dpdy(vec3(2.0));
    _ = dpdy(vec4(2.0));

    // 16.6.5 dpdyCoarse
    _ = dpdyCoarse(2.0);
    _ = dpdyCoarse(vec2(2.0));
    _ = dpdyCoarse(vec3(2.0));
    _ = dpdyCoarse(vec4(2.0));

    // 16.6.6 dpdyFine
    _ = dpdyFine(2.0);
    _ = dpdyFine(vec2(2.0));
    _ = dpdyFine(vec3(2.0));
    _ = dpdyFine(vec4(2.0));

    // 16.6.7 fwidth
    _ = fwidth(2.0);
    _ = fwidth(vec2(2.0));
    _ = fwidth(vec3(2.0));
    _ = fwidth(vec4(2.0));

    // 16.6.8 fwidthCoarse
    _ = fwidthCoarse(2.0);
    _ = fwidthCoarse(vec2(2.0));
    _ = fwidthCoarse(vec3(2.0));
    _ = fwidthCoarse(vec4(2.0));

    // 16.6.9 fwidthFine
    _ = fwidthFine(2.0);
    _ = fwidthFine(vec2(2.0));
    _ = fwidthFine(vec3(2.0));
    _ = fwidthFine(vec4(2.0));
}

// 16.7. Texture Built-in Functions (https://gpuweb.github.io/gpuweb/wgsl/#texture-builtin-functions)

@group(0) @binding( 0) var s: sampler;
@group(0) @binding(31) var sc: sampler_comparison;

@group(0) @binding( 1) var t1d: texture_1d<f32>;
@group(0) @binding( 2) var t1di: texture_1d<i32>;
@group(0) @binding( 3) var t1du: texture_1d<u32>;

@group(0) @binding( 4) var t2d: texture_2d<f32>;
@group(0) @binding( 5) var t2di: texture_2d<i32>;
@group(0) @binding( 6) var t2du: texture_2d<u32>;

@group(0) @binding( 7) var t2da: texture_2d_array<f32>;
@group(0) @binding( 8) var t2dai: texture_2d_array<i32>;
@group(0) @binding( 9) var t2dau: texture_2d_array<u32>;

@group(0) @binding(10) var t3d: texture_3d<f32>;
@group(0) @binding(11) var t3di: texture_3d<i32>;
@group(0) @binding(12) var t3du: texture_3d<u32>;

@group(0) @binding(13) var tm2d: texture_multisampled_2d<f32>;
@group(0) @binding(14) var tm2di: texture_multisampled_2d<i32>;
@group(0) @binding(15) var tm2du: texture_multisampled_2d<u32>;

@group(0) @binding(16) var tc: texture_cube<f32>;
@group(0) @binding(17) var tca: texture_cube_array<f32>;

@group(0) @binding(18) var ts1d: texture_storage_1d<rgba8unorm, write>;
@group(0) @binding(19) var ts2d: texture_storage_2d<rgba16uint, write>;
@group(0) @binding(20) var ts2da: texture_storage_2d_array<r32sint, write>;
@group(0) @binding(21) var ts3d: texture_storage_3d<rgba32float, write>;
@group(0) @binding(22) var te: texture_external;

@group(0) @binding(23) var td2d: texture_depth_2d;
@group(0) @binding(24) var td2da: texture_depth_2d_array;
@group(0) @binding(25) var tdc: texture_depth_cube;
@group(0) @binding(26) var tdca: texture_depth_cube_array;
@group(0) @binding(27) var tdms2d: texture_depth_multisampled_2d;

// 16.7.1
// RUN: %metal-compile testTextureDimensions
@compute @workgroup_size(1)
fn testTextureDimensions()
{
    // [S < Concrete32BitNumber].(Texture[S, Texture1d]) => U32,
    _ = textureDimensions(t1d);
    // [F, AM].(texture_storage_1d[F, AM]) => u32,
    _ = textureDimensions(ts1d);

    // [S < Concrete32BitNumber, T < ConcreteInteger].(Texture[S, Texture1d], T) => U32,
    _ = textureDimensions(t1d, 0);

    // [S < Concrete32BitNumber].(Texture[S, Texture2d]) => Vector[U32, 2],
    _ = textureDimensions(t2d);
    // [S < Concrete32BitNumber].(Texture[S, Texture2dArray]) => Vector[U32, 2],
    _ = textureDimensions(t2da);
    // [S < Concrete32BitNumber].(Texture[S, TextureCube]) => Vector[U32, 2],
    _ = textureDimensions(tc);
    // [S < Concrete32BitNumber].(Texture[S, TextureCubeArray]) => Vector[U32, 2],
    _ = textureDimensions(tca);
    // [S < Concrete32BitNumber].(Texture[S, TextureMultisampled2d]) => Vector[U32, 2],
    _ = textureDimensions(tm2d);

    // [].(texture_depth_2d) => Vector[U32, 2],
    _ = textureDimensions(td2d);
    // [].(texture_depth_2d_array) => Vector[U32, 2],
    _ = textureDimensions(td2da);
    // [].(texture_depth_cube) => Vector[U32, 2],
    _ = textureDimensions(tdc);
    // [].(texture_depth_cube_array) => Vector[U32, 2],
    _ = textureDimensions(tdca);
    // [].(texture_depth_multisampled_2d) => Vector[U32, 2],
    _ = textureDimensions(tdms2d);

    // [F, AM].(texture_storage_2d[F, AM]) => vec2[u32],
    _ = textureDimensions(ts2d);
    // [F, AM].(texture_storage_2d_array[F, AM]) => vec2[u32],
    _ = textureDimensions(ts2da);

    // [].(TextureExternal) => Vector[U32, 2],
    _ = textureDimensions(te);

    // [S < Concrete32BitNumber, T < ConcreteInteger].(Texture[S, Texture2d], T) => Vector[U32, 2],
    _ = textureDimensions(t2d, 0);
    // [S < Concrete32BitNumber, T < ConcreteInteger].(Texture[S, Texture2dArray], T) => Vector[U32, 2],
    _ = textureDimensions(t2da, 0);
    // [S < Concrete32BitNumber, T < ConcreteInteger].(Texture[S, TextureCube], T) => Vector[U32, 2],
    _ = textureDimensions(tc, 0);
    // [S < Concrete32BitNumber, T < ConcreteInteger].(Texture[S, TextureCubeArray], T) => Vector[U32, 2],
    _ = textureDimensions(tca, 0);
    // [T < ConcreteInteger].(texture_depth_2d, T) => Vector[U32, 2],
    _ = textureDimensions(td2d, 0);
    // [T < ConcreteInteger].(texture_depth_2d_array, T) => Vector[U32, 2],
    _ = textureDimensions(td2da, 0);
    // [T < ConcreteInteger].(texture_depth_cube, T) => Vector[U32, 2],
    _ = textureDimensions(tdc, 0);
    // [T < ConcreteInteger].(texture_depth_cube_array, T) => Vector[U32, 2],
    _ = textureDimensions(tdca, 0);

    // [S < Concrete32BitNumber].(Texture[S, Texture3d]) => Vector[U32, 3],
    _ = textureDimensions(t3d);
    // [F, AM].(texture_storage_3d[F, AM]) => vec3[u32],
    _ = textureDimensions(ts3d);

    // [S < Concrete32BitNumber, T < ConcreteInteger].(Texture[S, Texture3d], T) => Vector[U32, 3],
    _ = textureDimensions(t3d, 0);
}

// 16.7.2
// RUN: %metal-compile testTextureGather
@compute @workgroup_size(1)
fn testTextureGather()
{
    // [T < ConcreteInteger, S < Concrete32BitNumber].(T, Texture[S, Texture2d], Sampler, Vector[F32, 2]) => Vector[S, 4],
    _ = textureGather(0, t2d, s, vec2f(0));

    // [T < ConcreteInteger, S < Concrete32BitNumber].(T, Texture[S, Texture2d], Sampler, Vector[F32, 2], Vector[I32, 2]) => Vector[S, 4],
    _ = textureGather(0, t2d, s, vec2f(0), vec2i(0));

    // [T < ConcreteInteger, S < Concrete32BitNumber, U < ConcreteInteger].(T, Texture[S, Texture2dArray], Sampler, Vector[F32, 2], U) => Vector[S, 4],
    _ = textureGather(0, t2da, s, vec2f(0), 0);

    // [T < ConcreteInteger, S < Concrete32BitNumber, U < ConcreteInteger].(T, Texture[S, Texture2dArray], Sampler, Vector[F32, 2], U, Vector[I32, 2]) => Vector[S, 4],
    _ = textureGather(0, t2da, s, vec2f(0), 0, vec2i(0));

    // [T < ConcreteInteger, S < Concrete32BitNumber].(T, Texture[S, TextureCube], Sampler, Vector[F32, 3]) => Vector[S, 4],
    _ = textureGather(0, tc, s, vec3f(0));

    // [T < ConcreteInteger, S < Concrete32BitNumber, U < ConcreteInteger].(T, Texture[S, TextureCubeArray], Sampler, Vector[F32, 3], U) => Vector[S, 4],
    _ = textureGather(0, tca, s, vec3f(0), 0);

    // [].(texture_depth_2d, Sampler, Vector[F32, 2]) => Vector[F32, 4],
    _ = textureGather(td2d, s, vec2f(0));

    // [].(texture_depth_2d, Sampler, Vector[F32, 2], Vector[I32, 2]) => Vector[F32, 4],
    _ = textureGather(td2d, s, vec2f(0), vec2i(0));

    // [].(texture_depth_cube, Sampler, Vector[F32, 3]) => Vector[F32, 4],
    _ = textureGather(tdc, s, vec3f(0));

    // [U < ConcreteInteger].(texture_depth_2d_array, Sampler, Vector[F32, 2], U) => Vector[F32, 4],
    _ = textureGather(td2da, s, vec2f(0), 0);

    // [U < ConcreteInteger].(texture_depth_2d_array, Sampler, Vector[F32, 2], U, Vector[I32, 2]) => Vector[F32, 4],
    _ = textureGather(td2da, s, vec2f(0), 0, vec2i(0));

    // [U < ConcreteInteger].(texture_depth_cube_array, Sampler, Vector[F32, 3], U) => Vector[F32, 4],
    _ = textureGather(tdca, s, vec3f(0), 0);
}

// 16.7.3 textureGatherCompare
// RUN: %metal-compile testTextureGatherCompare
@compute @workgroup_size(1)
fn testTextureGatherCompare()
{
    // [].(texture_depth_2d, sampler_comparison, vec2[f32], f32) => vec4[f32],
    _ = textureGatherCompare(td2d, sc, vec2f(0), 0f);

    // [].(texture_depth_2d, sampler_comparison, vec2[f32], f32, vec2[i32]) => vec4[f32],
    _ = textureGatherCompare(td2d, sc, vec2f(0), 0f, vec2i(0));

    // [T < ConcreteInteger].(texture_depth_2d_array, sampler_comparison, vec2[f32], T, f32) => vec4[f32],
    _ = textureGatherCompare(td2da, sc, vec2f(0), 0i, 0f);

    // [T < ConcreteInteger].(texture_depth_2d_array, sampler_comparison, vec2[f32], T, f32, vec2[i32]) => vec4[f32],
    _ = textureGatherCompare(td2da, sc, vec2f(0), 0i, 0f, vec2i(0));

    // [].(texture_depth_cube, sampler_comparison, vec3[f32], f32) => vec4[f32],
    _ = textureGatherCompare(tdc, sc, vec3f(0), 0f);

    // [T < ConcreteInteger].(texture_depth_cube_array, sampler_comparison, vec3[f32], T, f32) => vec4[f32],
    _ = textureGatherCompare(tdca, sc, vec3f(0), 0i, 0f);
}

// 16.7.4
// RUN: %metal-compile testTextureLoad
@compute @workgroup_size(1)
fn testTextureLoad()
{
    let x: i32 = 0;
    // [T < ConcreteInteger, U < ConcreteInteger, S < Concrete32BitNumber].(Texture[S, Texture1d], T, U) => Vector[S, 4],
    {
        {
            _ = textureLoad(t1d, 0, 0);
            _ = textureLoad(t1d, 0i, 0u);
            _ = textureLoad(t1d, 0u, 0i);
            _ = textureLoad(t1d, 0u, x);
        }
        {
            _ = textureLoad(t1di, 0, 0);
            _ = textureLoad(t1di, 0i, 0u);
            _ = textureLoad(t1di, 0u, 0i);
            _ = textureLoad(t1di, 0u, x);
        }
        {
            _ = textureLoad(t1du, 0, 0);
            _ = textureLoad(t1du, 0i, 0u);
            _ = textureLoad(t1du, 0u, 0i);
            _ = textureLoad(t1du, 0u, x);
        }
    }

    // [T < ConcreteInteger, U < ConcreteInteger, S < Concrete32BitNumber].(Texture[S, Texture2d], Vector[T, 2], U) => Vector[S, 4],
    {
        {
            _ = textureLoad(t2d, vec2(0), 0);
            _ = textureLoad(t2d, vec2(0i), 0u);
            _ = textureLoad(t2d, vec2(0u), 0i);
            _ = textureLoad(t2d, vec2(0u), x);
        }
        {
            _ = textureLoad(t2di, vec2(0), 0);
            _ = textureLoad(t2di, vec2(0i), 0u);
            _ = textureLoad(t2di, vec2(0u), 0i);
            _ = textureLoad(t2di, vec2(0u), x);
        }
        {
            _ = textureLoad(t2du, vec2(0), 0);
            _ = textureLoad(t2du, vec2(0i), 0u);
            _ = textureLoad(t2du, vec2(0u), 0i);
            _ = textureLoad(t2du, vec2(0u), x);
        }
    }

    // [T < ConcreteInteger, V < ConcreteInteger, U < ConcreteInteger, S < Concrete32BitNumber].(Texture[S, Texture2dArray], Vector[T, 2], V, U) => Vector[S, 4],
    {
        {
            _ = textureLoad(t2da, vec2(0), 0, 0);
            _ = textureLoad(t2da, vec2(0i), 0u, 0i);
            _ = textureLoad(t2da, vec2(0u), 0i, 0u);
            _ = textureLoad(t2da, vec2(0u), x, x);
        }
        {
            _ = textureLoad(t2dai, vec2(0), 0, 0);
            _ = textureLoad(t2dai, vec2(0i), 0u, 0i);
            _ = textureLoad(t2dai, vec2(0u), 0i, 0u);
            _ = textureLoad(t2dau, vec2(0u), x, x);
        }
        {
            _ = textureLoad(t2dau, vec2(0), 0, 0);
            _ = textureLoad(t2dau, vec2(0i), 0u, 0i);
            _ = textureLoad(t2dau, vec2(0u), 0i, 0u);
            _ = textureLoad(t2dau, vec2(0u), x, x);
        }
    }

    // [T < ConcreteInteger, U < ConcreteInteger, S < Concrete32BitNumber].(Texture[S, Texture3d], Vector[T, 3], U) => Vector[S, 4],
    {
        {
            _ = textureLoad(t3d, vec3(0), 0);
            _ = textureLoad(t3d, vec3(0i), 0u);
            _ = textureLoad(t3d, vec3(0u), 0i);
        }
        {
            _ = textureLoad(t3di, vec3(0), 0);
            _ = textureLoad(t3di, vec3(0i), 0u);
            _ = textureLoad(t3di, vec3(0u), 0i);
        }
        {
            _ = textureLoad(t3du, vec3(0), 0);
            _ = textureLoad(t3du, vec3(0i), 0u);
            _ = textureLoad(t3du, vec3(0u), 0i);
        }
    }

    // [T < ConcreteInteger, U < ConcreteInteger, S < Concrete32BitNumber].(Texture[S, TextureMultisampled2d], Vector[T, 2], U) => Vector[S, 4],
    {
        {
            _ = textureLoad(tm2d, vec2(0), 0);
            _ = textureLoad(tm2d, vec2(0i), 0u);
            _ = textureLoad(tm2d, vec2(0u), 0i);
            _ = textureLoad(tm2d, vec2(0u), x);
        }
        {
            _ = textureLoad(tm2di, vec2(0), 0);
            _ = textureLoad(tm2di, vec2(0i), 0u);
            _ = textureLoad(tm2di, vec2(0u), 0i);
            _ = textureLoad(tm2di, vec2(0u), x);
        }
        {
            _ = textureLoad(tm2du, vec2(0), 0);
            _ = textureLoad(tm2du, vec2(0i), 0u);
            _ = textureLoad(tm2du, vec2(0u), 0i);
            _ = textureLoad(tm2du, vec2(0u), x);
        }
    }

    // [T < ConcreteInteger].(TextureExternal, Vector[T, 2]) => Vector[F32, 4],
    {
        _ = textureLoad(te, vec2(0));
        _ = textureLoad(te, vec2(0i));
        _ = textureLoad(te, vec2(0u));
    }

    // [T < ConcreteInteger, U < ConcreteInteger].(texture_depth_2d, Vector[T, 2], U) => F32,
    {
        _ = textureLoad(td2d, vec2(0), 0);
        _ = textureLoad(td2d, vec2(0i), 0i);
        _ = textureLoad(td2d, vec2(0u), 0u);
        _ = textureLoad(td2d, vec2(0u), x);
    }

    // [T < ConcreteInteger, S < ConcreteInteger, U < ConcreteInteger].(texture_depth_2d_array, Vector[T, 2], S, U) => F32,
    {
        _ = textureLoad(td2da, vec2(0), 0, 0);
        _ = textureLoad(td2da, vec2(0i), 0i, 0i);
        _ = textureLoad(td2da, vec2(0u), 0u, 0u);
        _ = textureLoad(td2da, vec2(0u), x, x);
    }

    // [T < ConcreteInteger, U < ConcreteInteger].(texture_depth_multisampled_2d, Vector[T, 2], U) => F32,
    {
        _ = textureLoad(tdms2d, vec2(0), 0);
        _ = textureLoad(tdms2d, vec2(0i), 0i);
        _ = textureLoad(tdms2d, vec2(0u), 0u);
        _ = textureLoad(tdms2d, vec2(0u), x);
    }
}

// 16.7.5
// RUN: %metal-compile testTextureNumLayers
@compute @workgroup_size(1)
fn testTextureNumLayers()
{
    // [S < Concrete32BitNumber].(Texture[S, Texture2dArray]) => U32,
    _ = textureNumLayers(t2da);

    // [S < Concrete32BitNumber].(Texture[S, TextureCubeArray]) => U32,
    _ = textureNumLayers(tca);

    // [].(texture_depth_2d_array) => U32,
    _ = textureNumLayers(td2da);

    // [].(texture_depth_cube_array) => U32,
    _ = textureNumLayers(tdca);

    // [F, AM].(texture_storage_2d_array[F, AM]) => u32,
    _ = textureNumLayers(ts2da);
}

// 16.7.6
// RUN: %metal-compile testTextureNumLevels
@compute @workgroup_size(1)
fn testTextureNumLevels()
{
    // [S < Concrete32BitNumber].(Texture[S, Texture1d]) => U32,
    _ = textureNumLevels(t1d);

    // [S < Concrete32BitNumber].(Texture[S, Texture2d]) => U32,
    _ = textureNumLevels(t2d);

    // [S < Concrete32BitNumber].(Texture[S, Texture2dArray]) => U32,
    _ = textureNumLevels(t2da);

    // [S < Concrete32BitNumber].(Texture[S, Texture3d]) => U32,
    _ = textureNumLevels(t3d);

    // [S < Concrete32BitNumber].(Texture[S, TextureCube]) => U32,
    _ = textureNumLevels(tc);

    // [S < Concrete32BitNumber].(Texture[S, TextureCubeArray]) => U32,
    _ = textureNumLevels(tca);

    // [].(texture_depth_2d) => U32,
    _ = textureNumLevels(td2d);

    // [].(texture_depth_2d_array) => U32,
    _ = textureNumLevels(td2da);

    // [].(texture_depth_cube) => U32,
    _ = textureNumLevels(tdc);

    // [].(texture_depth_cube_array) => U32,
    _ = textureNumLevels(tdca);
}

// 16.7.7
// RUN: %metal-compile testTextureNumSamples
@compute @workgroup_size(1)
fn testTextureNumSamples()
{
    // [S < Concrete32BitNumber].(Texture[S, TextureMultisampled2d]) => U32,
    _ = textureNumSamples(tm2d);

    // [].(texture_depth_multisampled_2d) => U32,
    _ = textureNumSamples(tdms2d);
}

// 16.7.8
// RUN: %metal-compile testTextureSample
@fragment
fn testTextureSample()
{
    // [].(Texture[F32, Texture1d], Sampler, F32) => Vector[F32, 4],
    _ = textureSample(t1d, s, 1);

    // [].(Texture[F32, Texture2d], Sampler, Vector[F32, 2]) => Vector[F32, 4],
    _ = textureSample(t2d, s, vec2<f32>(0, 0));

    // [].(Texture[F32, Texture2d], Sampler, Vector[F32, 2], Vector[I32, 2]) => Vector[F32, 4],
    _ = textureSample(t2d, s, vec2<f32>(0, 0), vec2<i32>(1, 1));

    // [T < ConcreteInteger].(Texture[F32, Texture2dArray], Sampler, Vector[F32, 2], T) => Vector[F32, 4],
    _ = textureSample(t2da, s, vec2<f32>(0, 0), 0);

    // [T < ConcreteInteger].(Texture[F32, Texture2dArray], Sampler, Vector[F32, 2], T, Vector[I32, 2]) => Vector[F32, 4],
    _ = textureSample(t2da, s, vec2<f32>(0, 0), 0, vec2<i32>(1, 1));

    // [].(Texture[F32, Texture3d], Sampler, Vector[F32, 3]) => Vector[F32, 4],
    _ = textureSample(t3d, s, vec3<f32>(0, 0, 0));

    // [].(Texture[F32, TextureCube], Sampler, Vector[F32, 3]) => Vector[F32, 4],
    _ = textureSample(tc, s, vec3<f32>(0, 0, 0));

    // [].(Texture[F32, Texture3d], Sampler, Vector[F32, 3], Vector[I32, 3]) => Vector[F32, 4],
    _ = textureSample(t3d, s, vec3<f32>(0, 0, 0), vec3<i32>(0, 0, 0));

    // [T < ConcreteInteger].(Texture[F32, TextureCubeArray], Sampler, Vector[F32, 3], T) => Vector[F32, 4],
    _ = textureSample(tca, s, vec3<f32>(0, 0, 0), 0);

    // [].(texture_depth_2d, Sampler, Vector[F32, 2]) => F32,
    _ = textureSample(td2d, s, vec2f(0));

    // [].(texture_depth_2d, Sampler, Vector[F32, 2], Vector[I32, 2]) => F32,
    _ = textureSample(td2d, s, vec2f(0), vec2i(0));

    // [T < ConcreteInteger].(texture_depth_2d_array, Sampler, Vector[F32, 2], T) => F32,
    _ = textureSample(td2da, s, vec2f(0), 0);

    // [T < ConcreteInteger].(texture_depth_2d_array, Sampler, Vector[F32, 2], T, Vector[I32, 2]) => F32,
    _ = textureSample(td2da, s, vec2f(0), 0, vec2i(0));

    // [].(texture_depth_cube, Sampler, Vector[F32, 3]) => F32,
    _ = textureSample(tdc, s, vec3f(0));

    // [T < ConcreteInteger].(texture_depth_cube_array, Sampler, Vector[F32, 3], T) => F32,
    _ = textureSample(tdca, s, vec3f(0), 0);
}


// 16.7.9
// RUN: %metal-compile testTextureSampleBias
@fragment
fn testTextureSampleBias()
{
    // [].(Texture[F32, Texture2d], Sampler, Vector[F32, 2], F32) => Vector[F32, 4],
    _ = textureSampleBias(t2d, s, vec2f(0), 0);

    // [].(Texture[F32, Texture2d], Sampler, Vector[F32, 2], F32, Vector[I32, 2]) => Vector[F32, 4],
    _ = textureSampleBias(t2d, s, vec2f(0), 0, vec2i(0));

    // [T < ConcreteInteger].(Texture[F32, Texture2dArray], Sampler, Vector[F32, 2], T, F32) => Vector[F32, 4],
    _ = textureSampleBias(t2da, s, vec2f(0), 0, 0);

    // [T < ConcreteInteger].(Texture[F32, Texture2dArray], Sampler, Vector[F32, 2], T, F32, Vector[I32, 2]) => Vector[F32, 4],
    _ = textureSampleBias(t2da, s, vec2f(0), 0, 0, vec2i(0));

    // [].(Texture[F32, Texture3d], Sampler, Vector[F32, 3], F32) => Vector[F32, 4],
    _ = textureSampleBias(t3d, s, vec3f(0), 0);

    // [].(Texture[F32, TextureCube], Sampler, Vector[F32, 3], F32) => Vector[F32, 4],
    _ = textureSampleBias(tc, s, vec3f(0), 0);

    // [].(Texture[F32, Texture3d], Sampler, Vector[F32, 3], F32, Vector[I32, 3]) => Vector[F32, 4],
    _ = textureSampleBias(t3d, s, vec3f(0), 0, vec3i(0));

    // [T < ConcreteInteger].(Texture[F32, TextureCubeArray], Sampler, Vector[F32, 3], T, F32) => Vector[F32, 4],
    _ = textureSampleBias(tca, s, vec3f(0), 0, 0);
}

// 16.7.10
// RUN: %metal-compile testTextureSampleCompare
@fragment
fn testTextureSampleCompare()
{
    // [].(texture_depth_2d, sampler_comparison, vec2[f32], f32) => f32,
    _ = textureSampleCompare(td2d, sc, vec2f(0), 0);

    // [].(texture_depth_2d, sampler_comparison, vec2[f32], f32, vec2[i32]) => f32,
    _ = textureSampleCompare(td2d, sc, vec2f(0), 0, vec2i(0));

    // [T < ConcreteInteger].(texture_depth_2d_array, sampler_comparison, vec2[f32], T, f32) => f32,
    _ = textureSampleCompare(td2da, sc, vec2f(0), 0i, 0f);

    // [T < ConcreteInteger].(texture_depth_2d_array, sampler_comparison, vec2[f32], T, f32, vec2[i32]) => f32,
    _ = textureSampleCompare(td2da, sc, vec2f(0), 0i, 0f, vec2i(0));

    // [].(texture_depth_cube, sampler_comparison, vec3[f32], f32) => f32,
    _ = textureSampleCompare(tdc, sc, vec3f(0), 0);

    // [T < ConcreteInteger].(texture_depth_cube_array, sampler_comparison, vec3[f32], T, f32) => f32,
    _ = textureSampleCompare(tdca, sc, vec3f(0), 0i, 0f);
}

// 16.7.11
// RUN: %metal-compile testTextureSampleCompareLevel
@compute @workgroup_size(1)
fn testTextureSampleCompareLevel()
{
    // [].(texture_depth_2d, sampler_comparison, vec2[f32], f32) => f32,
    _ = textureSampleCompareLevel(td2d, sc, vec2f(0), 0);

    // [].(texture_depth_2d, sampler_comparison, vec2[f32], f32, vec2[i32]) => f32,
    _ = textureSampleCompareLevel(td2d, sc, vec2f(0), 0, vec2i(0));

    // [T < ConcreteInteger].(texture_depth_2d_array, sampler_comparison, vec2[f32], T, f32) => f32,
    _ = textureSampleCompareLevel(td2da, sc, vec2f(0), 0i, 0f);

    // [T < ConcreteInteger].(texture_depth_2d_array, sampler_comparison, vec2[f32], T, f32, vec2[i32]) => f32,
    _ = textureSampleCompareLevel(td2da, sc, vec2f(0), 0i, 0f, vec2i(0));

    // [].(texture_depth_cube, sampler_comparison, vec3[f32], f32) => f32,
    _ = textureSampleCompareLevel(tdc, sc, vec3f(0), 0);

    // [T < ConcreteInteger].(texture_depth_cube_array, sampler_comparison, vec3[f32], T, f32) => f32,
    _ = textureSampleCompareLevel(tdca, sc, vec3f(0), 0i, 0f);
}

// 16.7.12
// RUN: %metal-compile testTextureSampleGrad
@compute @workgroup_size(1)
fn testTextureSampleGrad()
{
    // [].(Texture[F32, Texture2d], Sampler, Vector[F32, 2], Vector[F32, 2], Vector[F32, 2]) => Vector[F32, 4],
    _ = textureSampleGrad(t2d, s, vec2f(0), vec2f(0), vec2f(0));

    // [].(Texture[F32, Texture2d], Sampler, Vector[F32, 2], Vector[F32, 2], Vector[F32, 2], Vector[I32, 2]) => Vector[F32, 4],
    _ = textureSampleGrad(t2d, s, vec2f(0), vec2f(0), vec2f(0), vec2i(0));

    // [T < ConcreteInteger].(Texture[F32, Texture2dArray], Sampler, Vector[F32, 2], T, Vector[F32, 2], Vector[F32, 2]) => Vector[F32, 4],
    _ = textureSampleGrad(t2da, s, vec2f(0), 0, vec2f(0), vec2f(0));

    // [T < ConcreteInteger].(Texture[F32, Texture2dArray], Sampler, Vector[F32, 2], T, Vector[F32, 2], Vector[F32, 2], Vector[I32, 2]) => Vector[F32, 4],
    _ = textureSampleGrad(t2da, s, vec2f(0), 0, vec2f(0), vec2f(0), vec2i(0));

    // [].(Texture[F32, Texture3d], Sampler, Vector[F32, 3], Vector[F32, 2], Vector[F32, 2]) => Vector[F32, 4],
    _ = textureSampleGrad(t3d, s, vec3f(0), vec3f(0), vec3f(0));

    // [].(Texture[F32, TextureCube], Sampler, Vector[F32, 3], Vector[F32, 2], Vector[F32, 2]) => Vector[F32, 4],
    _ = textureSampleGrad(tc, s, vec3f(0), vec3f(0), vec3f(0));

    // [].(Texture[F32, Texture3d], Sampler, Vector[F32, 3], Vector[F32, 2], Vector[F32, 2], Vector[I32, 3]) => Vector[F32, 4],
    _ = textureSampleGrad(t3d, s, vec3f(0), vec3f(0), vec3f(0), vec3i(0));

    // [T < ConcreteInteger].(Texture[F32, TextureCubeArray], Sampler, Vector[F32, 3], T, Vector[F32, 2], Vector[F32, 2]) => Vector[F32, 4],
    _ = textureSampleGrad(tca, s, vec3f(0), 0, vec3f(0), vec3f(0));
}

// 16.7.13
// RUN: %metal-compile testTextureSampleLevel
@compute @workgroup_size(1)
fn testTextureSampleLevel()
{
    // [].(Texture[F32, Texture2d], Sampler, Vector[F32, 2], F32) => Vector[F32, 4],
    _ = textureSampleLevel(t2d, s, vec2f(0), 0);

    // [].(Texture[F32, Texture2d], Sampler, Vector[F32, 2], F32, Vector[I32, 2]) => Vector[F32, 4],
    _ = textureSampleLevel(t2d, s, vec2f(0), 0, vec2i(0));

    // [T < ConcreteInteger].(Texture[F32, Texture2dArray], Sampler, Vector[F32, 2], T, F32) => Vector[F32, 4],
    _ = textureSampleLevel(t2da, s, vec2f(0), 0i, 0);

    // [T < ConcreteInteger].(Texture[F32, Texture2dArray], Sampler, Vector[F32, 2], T, F32, Vector[I32, 2]) => Vector[F32, 4],
    _ = textureSampleLevel(t2da, s, vec2f(0), 0i, 0, vec2i(0));

    // [].(Texture[F32, Texture3d], Sampler, Vector[F32, 3], F32) => Vector[F32, 4],
    _ = textureSampleLevel(t3d, s, vec3f(0), 0);

    // [].(Texture[F32, TextureCube], Sampler, Vector[F32, 3], F32) => Vector[F32, 4],
    _ = textureSampleLevel(tc, s, vec3f(0), 0);

    // [].(Texture[F32, Texture3d], Sampler, Vector[F32, 3], F32, Vector[I32, 3]) => Vector[F32, 4],
    _ = textureSampleLevel(t3d, s, vec3f(0), 0, vec3i(0));

    // [T < ConcreteInteger].(Texture[F32, TextureCubeArray], Sampler, Vector[F32, 3], T, F32) => Vector[F32, 4],
    _ = textureSampleLevel(tca, s, vec3f(0), 0i, 0f);

    // [T < ConcreteInteger].(texture_depth_2d, Sampler, Vector[F32, 2], T) => F32,
    _ = textureSampleLevel(td2d, s, vec2f(0), 0i);

    // [T < ConcreteInteger].(texture_depth_2d, Sampler, Vector[F32, 2], T, Vector[I32, 2]) => F32,
    _ = textureSampleLevel(td2d, s, vec2f(0), 0i, vec2i(0));

    // [S < ConcreteInteger, T < ConcreteInteger].(texture_depth_2d_array, Sampler, Vector[F32, 2], S, T) => F32,
    _ = textureSampleLevel(td2da, s, vec2f(0), 0i, 0u);

    // [S < ConcreteInteger, T < ConcreteInteger].(texture_depth_2d_array, Sampler, Vector[F32, 2], S, T, Vector[I32, 2]) => F32,
    _ = textureSampleLevel(td2da, s, vec2f(0), 0i, 0u, vec2i(0));

    // [T < ConcreteInteger].(texture_depth_cube, Sampler, Vector[F32, 3], T) => F32,
    _ = textureSampleLevel(tdc, s, vec3f(0), 0u);

    // [S < ConcreteInteger, T < ConcreteInteger].(texture_depth_cube_array, Sampler, Vector[F32, 3], S, T) => F32,
    _ = textureSampleLevel(tdca, s, vec3f(0), 0i, 0u);
}

// 16.7.14
// RUN: %metal-compile testTextureSampleBaseClampToEdge
@compute @workgroup_size(1)
fn testTextureSampleBaseClampToEdge()
{
    // [].(TextureExternal, Sampler, Vector[F32, 2]) => Vector[F32, 4],
    _ = textureSampleBaseClampToEdge(te, s, vec2f(0));

    // [].(Texture[F32, Texture2d], Sampler, Vector[F32, 2]) => Vector[F32, 4],
    _ = textureSampleBaseClampToEdge(t2d, s, vec2f(0));
}

// 16.7.15
// RUN: %metal-compile testTextureStore
@compute @workgroup_size(1)
fn testTextureStore()
{
    // [F, T < ConcreteInteger].(texture_storage_1d[F, write], T, vec4[ChannelFormat[F]]) => void,
    textureStore(ts1d, 0, vec4f(0));

    // [F, T < ConcreteInteger].(texture_storage_2d[F, write], vec2[T], vec4[ChannelFormat[F]]) => void,
    textureStore(ts2d, vec2(0), vec4u(0));

    // [F, T < ConcreteInteger, S < ConcreteInteger].(texture_storage_2d_array[F, write], vec2[T], S, vec4[ChannelFormat[F]]) => void,
    textureStore(ts2da, vec2(0), 0, vec4i(0));

    // [F, T < ConcreteInteger].(texture_storage_3d[F, write], vec3[T], vec4[ChannelFormat[F]]) => void,
    textureStore(ts3d, vec3(0), vec4f(0));
}

// 16.8. Atomic Built-in Functions (https://www.w3.org/TR/WGSL/#atomic-builtin-functions)
var<workgroup> x: atomic<i32>;
@group(7) @binding(10) var<storage, read_write> y: atomic<i32>;

// RUN: %metal-compile testAtomicFunctions
@compute @workgroup_size(1)
fn testAtomicFunctions()
{
    testAtomicLoad();
    testAtomicStore();
    testAtomicReadWriteModify();
}

// 16.8.1
fn testAtomicLoad()
{
    // [AS, T].(ptr[AS, atomic[T], read_write]) => T,
    _ = atomicLoad(&x);
    _ = atomicLoad(&y);
}

// 16.8.2
fn testAtomicStore()
{
    /*[AS, T].(ptr[AS, atomic[T], read_write], T) => void,*/
    atomicStore(&x, 42);
    atomicStore(&y, 42);
}

// 16.8.3. Atomic Read-modify-write (this spec entry contains several functions)
fn testAtomicReadWriteModify()
{
    // [AS, T].(ptr[AS, atomic[T], read_write], T) => T,
    _ = atomicAdd(&x, 42);
    _ = atomicSub(&x, 42);
    _ = atomicMax(&x, 42);
    _ = atomicMin(&x, 42);
    _ = atomicAnd(&x, 42);
    _ = atomicOr(&x, 42);
    _ = atomicXor(&x, 42);
    _ = atomicExchange(&x, 42);
    _ = atomicCompareExchangeWeak(&x, 42, 13);

    _ = atomicAdd(&y, 42);
    _ = atomicSub(&y, 42);
    _ = atomicMax(&y, 42);
    _ = atomicMin(&y, 42);
    _ = atomicAnd(&y, 42);
    _ = atomicOr(&y, 42);
    _ = atomicXor(&y, 42);
    _ = atomicExchange(&y, 42);
    _ = atomicCompareExchangeWeak(&y, 42, 13);
}

// 16.9. Data Packing Built-in Functions (https://www.w3.org/TR/WGSL/#pack-builtin-functions)
// RUN: %metal-compile testDataPackingFunctions
@compute @workgroup_size(1)
fn testDataPackingFunctions()
{
    { let x = pack4x8snorm(vec4f(0)); }
    { let x = pack4x8unorm(vec4f(0)); }
    { let x = pack4xI8(vec4i(0)); }
    { let x = pack4xU8(vec4u(0)); }
    { let x = pack4xI8Clamp(vec4i(0)); }
    { let x = pack4xU8Clamp(vec4u(0)); }
    { let x = pack2x16snorm(vec2f(0)); }
    { let x = pack2x16unorm(vec2f(0)); }
    { let x = pack2x16float(vec2f(0)); }

    let v2f = vec2f(0);
    let v4f = vec4f(0);
    let v4i = vec4i(0);
    let v4u = vec4u(0);
    { let x = pack4x8snorm(v4f); }
    { let x = pack4x8unorm(v4f); }
    { let x = pack4xI8(v4i); }
    { let x = pack4xU8(v4u); }
    { let x = pack4xI8Clamp(v4i); }
    { let x = pack4xU8Clamp(v4u); }
    { let x = pack2x16snorm(v2f); }
    { let x = pack2x16unorm(v2f); }
    { let x = pack2x16float(v2f); }
}

// 16.10. Data Unpacking Built-in Functions (https://www.w3.org/TR/WGSL/#unpack-builtin-functions)
// RUN: %metal-compile testDataUnpackingFunction
@compute @workgroup_size(1)
fn testDataUnpackingFunction()
{
    { let x = unpack4x8snorm(0); }
    { let x = unpack4x8unorm(0); }
    { let x = unpack4xI8(0); }
    { let x = unpack4xU8(0); }
    { let x = unpack2x16snorm(0); }
    { let x = unpack2x16unorm(0); }
    { let x = unpack2x16float(0); }

    let u = 0u;
    { let x = unpack4x8snorm(u); }
    { let x = unpack4x8unorm(u); }
    { let x = unpack4xI8(u); }
    { let x = unpack4xU8(u); }
    { let x = unpack2x16snorm(u); }
    { let x = unpack2x16unorm(u); }
    { let x = unpack2x16float(u); }
}

// 16.11. Synchronization Built-in Functions (https://www.w3.org/TR/WGSL/#sync-builtin-functions)

// 16.11.1.
// RUN: %metal-compile testStorageBarrier
@compute @workgroup_size(1)
fn testStorageBarrier()
{
    // [].() => void,
    storageBarrier();
}

// 16.11.2.
// RUN: %metal-compile testWorkgroupBarrier
@compute @workgroup_size(1)
fn testWorkgroupBarrier()
{
    // fn workgroupBarrier()
    workgroupBarrier();
}

// 16.11.3.
var<workgroup> testWorkgroupUniformLoadHelper: i32;
// RUN: %metal-compile testWorkgroupUniformLoad
@compute @workgroup_size(1)
fn testWorkgroupUniformLoad()
{
    // [T].(ptr[workgroup, T]) => void,
    _ = workgroupUniformLoad(&testWorkgroupUniformLoadHelper);
}
