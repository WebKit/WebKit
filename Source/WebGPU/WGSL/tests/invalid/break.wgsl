// RUN: %not %wgslc | %check

// CHECK-L: break statement must be in a loop or switch case
fn f() { if floor { break; } }
