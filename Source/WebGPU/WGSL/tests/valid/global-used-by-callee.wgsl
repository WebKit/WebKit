// RUN: %metal-compile main
// RUN: %metal main 2>&1 | %check

var<private> x: i32;
var<private> y: i32;

// CHECK: int function\d\(int parameter\d\)
fn f() -> i32
{
    //CHECK: parameter\d
    _ = y;
    return 0;
}

// CHECK: int function\d\(\)
fn g() -> i32
{
    let y = 42;
    return 0;
}

// CHECK: int function\d\(int parameter\d\)
fn h() -> i32
{
    _ = y;
    return 0;
}

// CHECK: int function\d\(int parameter\d\)
fn i() -> i32
{
    // CHECK: function\d\(parameter\d\)
    _ = f();
    return 0;
}

// CHECK: float function\d\(float parameter\d, int parameter\d\)
fn j(x: f32) -> f32
{
    // CHECK: function\d\(parameter\d\)
    _ = f();
    return x;
}

@compute @workgroup_size(1)
fn main()
{
    // CHECK: int local\d;
    // CHECK: int local\d;
    _ = x;

    // CHECK: function\d\(local\d\)
    _ = f();

    // CHECK: function\d\(\)
    _ = g();

    // CHECK: function\d\(local\d\)
    _ = h();

    // CHECK: function\d\(local\d\)
    _ = i();

    // CHECK: function\d\(function\d\(42, local\d\), local\d\)
    _ = j(j(42));
}
