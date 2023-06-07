// RUN: %metal-compile main

@group(0) @binding(0) var<storage> r: f32;

fn testI32(x: i32) -> i32 { return x; }

fn testResourceShadowedByLocal() -> i32
{
    var r : i32;
    _ = testI32(r);
    return 0;
}

fn testResourceShadowedByParameter(r: i32) -> i32
{
    _ = testI32(r);
    return 0;
}

fn testLocalConstShadowedByLocal() -> i32
{
    const x: f32 = 0.1;
    {
        let x: i32 = 1;
        _ = testI32(x);
    }
    return 0;
}

const x: f32 = 0.1123;
fn testGlobalConstShadowedByLocal() -> i32
{
    let x: i32 = 1;
    _ = testI32(x);
    return 0;
}

fn testGlobalConstShadowedByParameter(x: i32) -> i32
{
    _ = testI32(x);
    return 0;
}

fn testParameterShadowedByLocal(x: f32) -> i32
{
    {
        let x: i32 = 1;
        _ = testI32(x);
    }
    return 0;
}

fn testUserDefinedFunctionShadowedByLocal() -> i32
{
    let main: i32 = 1;
    _ = testI32(main);
    return 0;
}

fn testUserDefinedFunctionShadowedByLocalConst() -> i32
{
    const main: i32 = 1;
    _ = testI32(main);
    return 0;
}

fn testBuiltinShadowedByLocal() -> i32
{
    let textureLoad: i32 = 1;
    _ = testI32(textureLoad);
    return 0;
}

fn testBuiltinShadowedByLocalConst() -> i32
{
    const textureLoad: i32 = 1;
    _ = testI32(textureLoad);
    return 0;
}

@group(0) @binding(1) var<storage> transpose: i32;
fn testBuiltinShadowedByGlobal() -> i32
{
    _ = testI32(transpose);
    return 0;
}

const step: i32 = 42;
fn testBuiltinShadowedByGlobalConst() -> i32
{
    _ = testI32(step);
    return 0;
}

fn testBuiltinShadowedByParameter(textureLoad: i32) -> i32
{
    _ = testI32(textureLoad);
    return 0;
}

fn trunc(x: i32) -> i32 { return x; }

fn testBuiltinShadowedByUserDefinedFunction() -> i32
{
    _ = trunc(42);
    return 0;
}

struct max { x: i32 }
fn testBuiltinShadowedByStruct() -> i32
{
    _ = testI32(max(42).x);
    return 0;
}

@compute @workgroup_size(1)
fn main()
{
    _ = testResourceShadowedByLocal();
    _ = testResourceShadowedByParameter(42);

    _ = testLocalConstShadowedByLocal();

    _ = testGlobalConstShadowedByLocal();
    _ = testGlobalConstShadowedByParameter(42);

    _ = testParameterShadowedByLocal(0.1);

    _ = testUserDefinedFunctionShadowedByLocal();
    _ = testUserDefinedFunctionShadowedByLocalConst();

    _ = testBuiltinShadowedByLocal();
    _ = testBuiltinShadowedByLocalConst();
    _ = testBuiltinShadowedByGlobal();
    _ = testBuiltinShadowedByGlobalConst();
    _ = testBuiltinShadowedByParameter(42);
    _ = testBuiltinShadowedByUserDefinedFunction();
    _ = testBuiltinShadowedByStruct();

    // FIXME: test shadowing types once we support it
    // const i32 = 0;
    // const vec2 = 0;
}
