// RUN: %not %wgslc | %check

fn testWhileStatement() {
    // CHECK-L: while condition must be bool, got ref<function, i32, read_write>
    var i = 0;
    while (i) {
    }
}
