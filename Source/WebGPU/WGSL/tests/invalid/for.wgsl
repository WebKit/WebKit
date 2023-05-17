// RUN: %not %wgslc | %check

fn testForStatement() {
  // CHECK-L: for-loop condition must be bool, got <AbstractInt>
  for (var i = 0; i;) {
  }
}
