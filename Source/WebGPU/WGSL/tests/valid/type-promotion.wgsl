// RUN: %metal-compile main

fn f1(x: array<vec2<f32>, 1>) -> f32 { return x[0][0]; }
fn f2(x: array<vec2<i32>, 1>) -> i32 { return x[0][0]; }
fn f4(x: array<array<vec2<f32>, 1>, 1>) -> f32 { return x[0][0][0]; }

const global = array(vec2(0));

fn testCallee()
{
    // - Globals in a callee function
    _ = f1(global);
    _ = f2(global);
}

struct S { x: u32 };
fn testLiteral() -> i32
{
    // FIXME: this still fails
    // _ = S(abs(0));

    // We generate struct initialization with braces, so no narrowing allowed
    _ = S(abs(0u));
    return 0;
}

@compute @workgroup_size(1)
fn main() {
    // - Simple variable initialization promotion
    let x : array<vec2<f32>, 1> = array(vec2(0));

    // - Simple assignment promotion
    var y : array<vec2<f32>, 1>;
    y = array(vec2(0));

    // - Call with immediate value
    _ = f1(array(vec2(0)));

    // - Call with local constant
    const a = array(vec2(0));
    _ = f1(a);

    // - Two-level nested vector promotoin
    const b = array(array(vec2(0)));
    _ = f4(b);

    // - Constant promoted to two types
    const c = array(vec2(0));
    _ = f1(c);
    _ = f2(c);

    // - Global constants
    _ = f1(global);
    _ = f2(global);

    {
        // - Test within a nested block
        _ = f1(global);
        _ = f2(global);
    }

    _ = testCallee();
    _ = testLiteral();
}
