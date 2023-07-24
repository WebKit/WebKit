// RUN: %not %wgslc | %check

fn testForStatement() {
  // CHECK-L: for-loop condition must be bool, got ref<function, <AbstractInt>, read_write>
  for (var i = 0; i; i++) {
  }
}
