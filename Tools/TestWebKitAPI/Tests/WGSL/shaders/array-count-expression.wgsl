struct S { x: array<i32, 1<<2> }

@compute @workgroup_size(1)
fn main() {
    var s : S;
}
