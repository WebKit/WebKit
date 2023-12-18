// RUN: %metal-compile main

fn testI32()
{
    var x: vec2<i32>;
    var y: vec2<i32>;

    let a = x / y;
    let b = x / y[0];
    let c = x[0] / y;
    let d = x[0] / y[0];
}

fn testU32()
{
    var x: vec2<u32>;
    var y: vec2<u32>;

    let a = x / y;
    let b = x / y[0];
    let c = x[0] / y;
    let d = x[0] / y[0];
}

fn testF32()
{
    var x: vec2<f32>;
    var y: vec2<f32>;

    let a = x / y;
    let b = x / y[0];
    let c = x[0] / y;
    let d = x[0] / y[0];
}

fn testI32Compound()
{
    var x: vec2<i32>;
    var y: vec2<i32>;

    x /= y;
    x /= y[0];
    x[0] /= y[0];
}

fn testU32Compound()
{
    var x: vec2<u32>;
    var y: vec2<u32>;

    x /= y;
    x /= y[0];
    x[0] /= y[0];
}

fn testF32Compound()
{
    var x: vec2<f32>;
    var y: vec2<f32>;

    x /= y;
    x /= y[0];
    x[0] /= y[0];
}

@compute @workgroup_size(1)
fn main() {
    testI32();
    testU32();
    testF32();

    testI32Compound();
    testU32Compound();
    testF32Compound();
}
