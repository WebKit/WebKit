// RUN: %not %wgslc | %check

// CHECK-L: only pointers in <storage> address space may specify an access mode
fn f1(x: ptr<function, i32, read>) { }

// FIXME: these pointers are not allowed as parameter types, but validation is not yet implemented
// fn f2(x: ptr<storage, i32>) { }
// fn f3(x: ptr<workgroup, i32>) { }
// fn f4(x: ptr<uniform, i32>) { }
