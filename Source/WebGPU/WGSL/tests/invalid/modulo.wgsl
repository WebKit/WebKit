// RUN: %not %wgslc | %check

fn testAbstractInt()
{
    // CHECK-L: modulo by zero
    _ = 42 % 0;
    // CHECK-L: modulo by zero
    _ = 42 % vec2(1, 0);
    // CHECK-L: modulo by zero
    _ = vec2(42) % 0;
    // CHECK-L: modulo by zero
    _ = vec2(42) % vec2(1, 0);

    let x = 42;
    // CHECK-L: modulo by zero
    _ = x % 0;
    // CHECK-L: modulo by zero
    _ = x % vec2(1, 0);
    // CHECK-L: modulo by zero
    _ = vec2(x) % 0;
    // CHECK-L: modulo by zero
    _ = vec2(x) % vec2(1, 0);

    // CHECK-NOT-L: invalid modulo overflow
    _ = (-2147483647 - 1) % -1;
    // CHECK-NOT-L: invalid modulo overflow
    _ = vec2(-2147483647 - 1, 1) % vec2(-1, 1);

    // CHECK-L: invalid modulo overflow
    _ = (-9223372036854775807 - 1) % -1;
    // CHECK-L: invalid modulo overflow
    _ = vec2(-9223372036854775807 - 1, 1) % vec2(-1, 1);
}

fn testI32()
{
    // CHECK-L: modulo by zero
    _ = 42i % 0i;
    // CHECK-L: modulo by zero
    _ = 42i % vec2(1i, 0i);
    // CHECK-L: modulo by zero
    _ = vec2(42i) % 0i;
    // CHECK-L: modulo by zero
    _ = vec2(42i) % vec2(1i, 0i);

    let x = 42i;
    // CHECK-L: modulo by zero
    _ = x % 0i;
    // CHECK-L: modulo by zero
    _ = x % vec2(1i, 0i);
    // CHECK-L: modulo by zero
    _ = vec2(x) % 0i;
    // CHECK-L: modulo by zero
    _ = vec2(x) % vec2(1i, 0i);

    // CHECK-L: invalid modulo overflow
    _ = (-2147483647i - 1i) % -1i;
    // CHECK-L: invalid modulo overflow
    _ = vec2(-2147483647i - 1i, 1i) % vec2(-1i, 1i);
}

fn testU32()
{
    // CHECK-L: modulo by zero
    _ = 42u % 0u;
    // CHECK-L: modulo by zero
    _ = 42u % vec2(1u, 0u);
    // CHECK-L: modulo by zero
    _ = vec2(42u) % 0u;
    // CHECK-L: modulo by zero
    _ = vec2(42u) % vec2(1u, 0u);

    let x = 42u;
    // CHECK-L: modulo by zero
    _ = x % 0u;
    // CHECK-L: modulo by zero
    _ = x % vec2(1u, 0u);
    // CHECK-L: modulo by zero
    _ = vec2(x) % 0u;
    // CHECK-L: modulo by zero
    _ = vec2(x) % vec2(1u, 0u);
}

fn testAbstractFloat()
{
    // CHECK-L: value NaN cannot be represented as '<AbstractFloat>'
    _ = 42.0 % 0.0;
    // CHECK-L: value vec2(0, NaN) cannot be represented as 'vec2<<AbstractFloat>>'
    _ = 42.0 % vec2(1.0, 0.0);
    // CHECK-L: value vec2(NaN, NaN) cannot be represented as 'vec2<<AbstractFloat>>'
    _ = vec2(42.0) % 0.0;
    // CHECK-L: value vec2(0, NaN) cannot be represented as 'vec2<<AbstractFloat>>'
    _ = vec2(42.0) % vec2(1.0, 0.0);

    // CHECK-NOT-L: modulo by zero
    let x = 42.0;
    // CHECK-NOT-L: modulo by zero
    _ = x % 0.0;
    // CHECK-NOT-L: modulo by zero
    _ = x % vec2(1.0, 0.0);
    // CHECK-NOT-L: modulo by zero
    _ = vec2(x) % 0.0;
    // CHECK-NOT-L: modulo by zero
    _ = vec2(x) % vec2(1.0, 0.0);

    // CHECK-NOT-L: invalid modulo overflow
    _ = (-2147483647.0 - 1.0) % -1.0;
    // CHECK-NOT-L: invalid modulo overflow
    _ = vec2(-2147483647.0 - 1.0, 1.0) % vec2(-1.0, 1.0);

    // CHECK-NOT-L: invalid modulo overflow
    _ = (-9223372036854775807.0 - 1.0) % -1.0;
    // CHECK-NOT-L: invalid modulo overflow
    _ = vec2(-9223372036854775807.0 - 1.0, 1.0) % vec2(-1.0, 1.0);

    // CHECK-NOT-L: invalid modulo overflow
    _ = -340282346638528859811704183484516925440.0 - 1.0 % -1.0;
    // CHECK-NOT-L: invalid modulo overflow
    _ = vec2(-340282346638528859811704183484516925440.0 - 1.0, 1.0) % vec2(-1.0, 1.0);
}

fn testF32()
{
    // CHECK-L: value NaN cannot be represented as 'f32'
    _ = 42f % 0f;
    // CHECK-L: value vec2(0f, NaN) cannot be represented as 'vec2<f32>'
    _ = 42f % vec2(1f, 0f);
    // CHECK-L: value vec2(NaN, NaN) cannot be represented as 'vec2<f32>'
    _ = vec2(42f) % 0f;
    // CHECK-L: value vec2(0f, NaN) cannot be represented as 'vec2<f32>'
    _ = vec2(42f) % vec2(1f, 0f);

    let x = 42f;
    // CHECK-NOT-L: modulo by zero
    _ = x % 0f;
    // CHECK-NOT-L: modulo by zero
    _ = x % vec2(1f, 0f);
    // CHECK-NOT-L: modulo by zero
    _ = vec2(x) % 0f;
    // CHECK-NOT-L: modulo by zero
    _ = vec2(x) % vec2(1f, 0f);

    // CHECK-NOT-L: invalid modulo overflow
    _ = (-2147483647f - 1f) % -1f;
    // CHECK-NOT-L: invalid modulo overflow
    _ = vec2(-2147483647f - 1f, 1f) % vec2(-1f, 1f);

    // CHECK-NOT-L: invalid modulo overflow
    _ = (-9223372036854775807.f - 1.f) % -1.f;
    // CHECK-NOT-L: invalid modulo overflow
    _ = vec2(-9223372036854775807.f - 1.f, 1.f) % vec2(-1.f, 1.f);

    // CHECK-NOT-L: invalid modulo overflow
    _ = (-340282346638528859811704183484516925439.f - 1f) % -1f;
    // CHECK-NOT-L: invalid modulo overflow
    _ = vec2(-340282346638528859811704183484516925439.f - 1f, 1f) % vec2(-1f, 1f);
}

fn testI32Compound()
{
    var y: vec2<i32>;
    // CHECK-L: modulo by zero
    y %= 0;
    // CHECK-L: modulo by zero
    y %= vec2(1, 0);
    // CHECK-L: modulo by zero
    y[0] %= 0;
    // CHECK-L: modulo by zero
    y[0] %= vec2(1, 0);
}

fn testU32Compound()
{
    var y: vec2<u32>;
    // CHECK-L: modulo by zero
    y %= 0;
    // CHECK-L: modulo by zero
    y %= vec2(1, 0);
    // CHECK-L: modulo by zero
    y[0] %= 0;
    // CHECK-L: modulo by zero
    y[0] %= vec2(1, 0);
}

fn testF32Compound()
{
    // FIXME: these depende on proper typing for compound statements
    // var y: vec2<f32>;
    // skip-CHECK-NOT-L: modulo by zero
    // y %= 0;
    // skip-CHECK-NOT-L: modulo by zero
    // y %= vec2(1, 0);
    // skip-CHECK-NOT-L: modulo by zero
    // y[0] %= 0;
    // skip-CHECK-NOT-L: modulo by zero
    // y[0] %= vec2(1, 0);
}

@compute @workgroup_size(1)
fn main() {
    testAbstractInt();
    testI32();
    testU32();
    testAbstractFloat();
    testF32();

    testI32Compound();
    testU32Compound();
    testF32Compound();
}
