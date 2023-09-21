// RUN: %metal-compile main

fn f1(x: ptr<function, i32>) -> i32
{
    return *x;
}

fn f2(x: ptr<private, i32>) -> i32
{
    return *x;
}

var<private> global: i32;

@compute @workgroup_size(1)
fn main()
{
    var local: i32;

    f1(&local);
    f2(&global);
}
