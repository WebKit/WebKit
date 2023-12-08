// RUN: %not %wgslc | %check

// CHECK-L: encountered a dependency cycle: a -> S -> a
const a = S(array(1));
struct S { x: array<i32, a.x[0]> };
