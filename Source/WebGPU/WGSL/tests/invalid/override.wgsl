// RUN: %not %metal main 2>&1 | %check

override ll: u32 = 0;
// CHECK-L: array count must be greater than 0
var<workgroup> oob: array<u32, ll>;

@compute @workgroup_size(1)
fn main() {
    let x = oob[0];
}
