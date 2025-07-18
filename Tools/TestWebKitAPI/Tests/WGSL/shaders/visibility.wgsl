// RUN: %not %wgslc | %check

fn f() {
    // CHECK-L: built-in cannot be used by fragment pipeline stage
    storageBarrier();
}

@fragment
fn main() {
    f();
}
