var<private> a: array<i32, 100000>;
@compute @workgroup_size(1)
fn main()
{
    _ = a;
}
