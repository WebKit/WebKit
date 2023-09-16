// RUN: %metal-compile main

const x = y * 2;
const y = 3;

struct T {
    s: S,
};

struct S {
    x: i32,
}

@compute @workgroup_size(1)
fn main()
{
    _ = helper();
}

fn helper() -> i32
{
    _ = T(S(y));
    return 0;
}
