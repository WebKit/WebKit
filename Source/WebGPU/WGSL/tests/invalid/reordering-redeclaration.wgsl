// RUN: %not %wgslc | %check

// CHECK-L: redeclaration of 'y'
const y = x * 2;
const y = 2;
