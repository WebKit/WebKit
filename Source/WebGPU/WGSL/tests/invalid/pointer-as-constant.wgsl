// RUN: %not %wgslc | %check

var<storage> x: i32;

// CHECK-L: cannot use runtime value in constant expression
@id(*&x) var<storage> y: i32;
