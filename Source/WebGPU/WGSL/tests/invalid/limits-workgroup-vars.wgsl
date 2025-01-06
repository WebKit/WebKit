// RUN: %not %metal main 2>&1 | %check

// CHECK-L: The combined byte size of all variables in the workgroup address space exceeds 16384 bytes
var<workgroup> a: array<i32, 100000>;
@compute @workgroup_size(1)
fn main()
{
    _ = a;
}
