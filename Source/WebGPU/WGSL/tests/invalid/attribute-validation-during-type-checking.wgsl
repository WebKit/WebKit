// RUN: %not %wgslc | %check

// CHECK-L: 'array<i32, 1>' cannot be used as the type of an 'override'
@id(0) override z: array<i32, 1>;

// CHECK-L: cannot call function from constant context
@id(u32(dpdx(0))) override y: i32;
