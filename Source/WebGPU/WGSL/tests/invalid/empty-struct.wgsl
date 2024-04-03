// RUN: %not %wgslc | %check

// CHECK-L: structures must have at least one member
struct S { }
