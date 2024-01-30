// RUN: %not %wgslc | %check

fn f1(x: f32) -> f32 { return x; }

fn f2() {
    // CHECK-L: unresolved call target 'f0'
    _ = f0();

    // CHECK-L: type in function call does not match parameter type: expected 'f32', found 'i32'
    _ = f1(0i);

    // CHECK-L: cannot call value of type 'i32'
    let f3: i32 = 0;
    _ = f3();

    // CHECK-L: cannot initialize var of type 'i32' with value of type 'f32'
    let x: i32 = f1(0);

    // CHECK-L: funtion call has too few arguments: expected 1, found 0
    _ = f1();

    // CHECK-L: funtion call has too many arguments: expected 1, found 2
    _ = f1(0, 0);
}

fn f3() { }
fn f4() {
    // CHECK-L: cannot initialize variable with expression of type 'void'
    let x = f3();
}
