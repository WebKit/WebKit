// RUN: %metal main 2>&1 | %check

fn f1(x: array<vec2<f32>, 1>) -> f32 { return x[0][0]; }
fn f2(x: array<vec2<i32>, 1>) -> i32 { return x[0][0]; }
fn f4(x: array<array<vec2<f32>, 1>, 1>) -> f32 { return x[0][0][0]; }

const global = array(vec2(0));

fn testCallee()
{
    // - Globals in a callee function

    // CHECK: local\d+ = {
    // CHECK-L: vec<float, 2>(0)
    // CHECK-L: };
    // CHECK: f1\(local\d+\)
    _ = f1(global);

    // CHECK: local\d+ = {
    // CHECK-L: vec<int, 2>(0)
    // CHECK-L: };
    // CHECK: f2\(local\d+\)
    _ = f2(global);
}

@compute @workgroup_size(1)
fn main() {
    // - Simple assignment promotion

    // CHECK: local\d+ = {
    // CHECK-L: vec<float, 2>(0)
    // CHECK-L: };
    let x : array<vec2<f32>, 1> = array(vec2(0));

    // - Call with immediate value

    // CHECK-L: f1({
    // CHECK-L: vec<float, 2>(0)
    // CHECK-L: })
    _ = f1(array(vec2(0)));

    // - Call with local constant

    // CHECK: local\d+ = {
    // CHECK-L: vec<float, 2>(0)
    // CHECK-L: };
    const a = array(vec2(0));
    // CHECK: f1\(local\d+\)
    _ = f1(a);

    // - Two-level nested vector promotoin

    // CHECK: local\d+ = {
    // CHECK-L: {
    // CHECK-L: vec<float, 2>(0)
    // CHECK-L: }
    // CHECK-L: };
    const b = array(array(vec2(0)));
    // CHECK: f4\(local\d+\)
    _ = f4(b);

    // - Constant promoted to two types

    const c = array(vec2(0));

    // CHECK: local\d+ = {
    // CHECK-L: vec<float, 2>(0)
    // CHECK-L: };
    // CHECK: f1\(local\d+\)
    _ = f1(c);

    // CHECK: local\d+ = {
    // CHECK-L: vec<int, 2>(0)
    // CHECK-L: };
    // CHECK: f2\(local\d+\)
    _ = f2(c);

    // - Global constants

    // CHECK: local\d+ = {
    // CHECK-L: vec<float, 2>(0)
    // CHECK-L: };
    // CHECK: f1\(local\d+\)
    _ = f1(global);

    // CHECK: local\d+ = {
    // CHECK-L: vec<int, 2>(0)
    // CHECK-L: };
    // CHECK: f2\(local\d+\)
    _ = f2(global);

    {
        // - Test within a nested block
        // CHECK: local\d+ = {
        // CHECK-L: vec<float, 2>(0)
        // CHECK-L: };
        // CHECK: f1\(local\d+\)
        _ = f1(global);

        // CHECK: local\d+ = {
        // CHECK-L: vec<int, 2>(0)
        // CHECK-L: };
        // CHECK: f2\(local\d+\)
        _ = f2(global);
    }

    _ = testCallee();
}
