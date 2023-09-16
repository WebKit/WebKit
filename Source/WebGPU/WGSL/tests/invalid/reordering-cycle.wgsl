// RUN: %not %wgslc | %check

// CHECK-L: encountered a dependency cycle: y -> x -> w -> z -> y
const y = x * 2;
const z = y;
const w = z;
const x = w;
