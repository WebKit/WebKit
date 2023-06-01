// RUN: %metal main 2>&1 | %check

var<private> x: i32;
var<private> y: i32;

// CHECK: void f\(int parameter\d\)
fn f() {
    //CHECK: parameter\d
    _ = y;
}

@compute @workgroup_size(1)
fn main() {
    // CHECK: int local\d;
    // CHECK: int local\d;
    _ = x;
    // CHECK: f\(local\d\)
    _ = f();
}
