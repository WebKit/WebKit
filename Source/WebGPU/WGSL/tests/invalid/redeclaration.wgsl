// RUN: %not %wgslc | %check

struct f {
}

// CHECK-L: redeclaration of 'f'
override f = 1;

// CHECK-L: redeclaration of 'f'
fn f() {
    let x = 1;
    // CHECK-L: redeclaration of 'x'
    var x = 2;
    // CHECK-L: redeclaration of 'x'
    const x = 2;
}
