// RUN: %not %wgslc | %check

struct f {
  x: i32,
}

// CHECK-L: redeclaration of 'f'
override f = 1;

fn f() { }
