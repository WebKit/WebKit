// RUN: %not %wgslc | %check

fn testIndexAccess() {
  // CHECK-L: index must be of type 'i32' or 'u32', found: 'f32'
  let x1 = vec2(0)[1f];
}
