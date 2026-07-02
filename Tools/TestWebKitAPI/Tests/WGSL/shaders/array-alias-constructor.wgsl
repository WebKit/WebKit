alias A = array<i32, 4>;

@compute @workgroup_size(1)
fn main()
{
    let a: A = A();
}
