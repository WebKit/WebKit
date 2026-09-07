alias A = array<i32, 4>;

@group(0) @binding(0) var<storage, read_write> x: array<i32>;

fn f(a: i32) -> A
{
    return A(a, a + 1, a + 2, a + 3);
}

@compute @workgroup_size(1)
fn main()
{
    let a: A = A();
    let b: A = A(x[0], x[0] + 1, x[0] + 2, x[0] + 3);
    let c: A = f(x[0]);
    x[0] = a[0] + b[0] + c[0];
}
