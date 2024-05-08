// RUN: %not %wgslc | %check

fn testIfStatement() {
  // CHECK-L: expected 'bool', found 'array<f32, 9>'
    if array<f32, 9>() {
        return;
    }
}
