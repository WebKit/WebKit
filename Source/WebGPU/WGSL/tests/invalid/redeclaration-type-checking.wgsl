// RUN: %not %wgslc | %check

fn f() {
    let x = 1;
    // CHECK-L: redeclaration of 'x'
    var x = 2;
    // CHECK-L: redeclaration of 'x'
    const x = 2;
}
