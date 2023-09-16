// RUN: %not %wgslc | %check

// CHECK-L: encountered a dependency cycle: a -> a
const a = a * 2;
