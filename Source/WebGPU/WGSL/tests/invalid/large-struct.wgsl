// RUN: %not %metal main 2>&1 | %check

struct Data {
    @size(2147483647) a: bool,
    @size(2147483647) b: bool,
    @size(2147483647) c: bool,
}

@compute @workgroup_size(1)
fn main() {
    // CHECK-L:The combined byte size of all variables in this function exceeds 8192 bytes
    var d : Data;
}
