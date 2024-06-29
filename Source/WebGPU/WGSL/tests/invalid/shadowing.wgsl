// RUN: %not %wgslc | %check

fn testParameterScope(x: f32) -> i32
{
    // CHECK-L: redeclaration of 'x'
    let x: i32 = 1;
}
