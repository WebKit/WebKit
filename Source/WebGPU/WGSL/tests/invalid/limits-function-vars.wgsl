// RUN: %not %metal main 2>&1 | %check

@compute @workgroup_size(1)
fn main()
{
  // CHECK-L: The combined byte size of all variables in this function exceeds 8192 bytes
  var a: array<i32, 100000>;
}
