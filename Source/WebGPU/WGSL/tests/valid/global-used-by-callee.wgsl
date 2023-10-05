// RUN: %metal-compile main
// RUN: %metal main 2>&1 | %check

var<private> x: i32;
var<private> y: i32;
override z = 42;

// CHECK: int function\d\(thread int& global\d\)
fn f() -> i32
{
    //CHECK: global\d
    _ = y;
    return 0;
}

// CHECK: int function\d\(\)
fn g() -> i32
{
    let y = 42;
    return y;
}

// CHECK: int function\d\(thread int& global\d, int global\d\)
fn h() -> i32
{
    _ = y;
    _ = z;
    return 0;
}

// CHECK: int function\d\(thread int& global\d\)
fn i() -> i32
{
    // CHECK: function\d\(global\d\)
    _ = f();
    return 0;
}

// CHECK: float function\d\(float parameter\d, thread int& global\d\)
fn j(x: f32) -> f32
{
    // CHECK: function\d\(global\d\)
    _ = f();
    return x;
}

@compute @workgroup_size(1)
fn main()
{
    // CHECK: int global\d
    // CHECK: int global\d
    _ = x;

    // CHECK: function\d\(global\d\)
    _ = f();

    // CHECK: function\d\(\)
    _ = g();

    // CHECK: function\d\(global\d, global\d\)
    _ = h();

    // CHECK: function\d\(global\d\)
    _ = i();

    // CHECK: function\d\(function\d\(42., global\d\), global\d\)
    _ = j(j(42));
}
