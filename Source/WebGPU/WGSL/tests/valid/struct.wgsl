// RUN: %metal-compile main

struct S {
    @size(16) x: f32,
    @align(16) y: i32,
}

struct T{ x:array<i32,4294063007u> }

fn testStructConstructor() -> i32
{
    _ = S(0, 0);
    _ = S(0.0, 0);
    _ = S(0f, 0i);
    return 0;
}

fn testPrimitiveStruct()
{
    var x = frexp(vec4<f16>());
}

@compute @workgroup_size(1)
fn main()
{
    _ = testStructConstructor();
    testPrimitiveStruct();
}

struct S1 {
@location(0) x: i32,
}

struct S2 {
@location(1) x: i32,
}

@fragment
fn fragment_main(s1: S1, s2: S2) {
}
