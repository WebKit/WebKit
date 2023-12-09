// RUN: %not %wgslc | %check

struct f {
}

// CHECK-L: redeclaration of 'f'
override f = 1;

fn f() { }
