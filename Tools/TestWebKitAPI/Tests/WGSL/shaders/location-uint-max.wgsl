struct S {
    @location(4294967294u) x: i32
}

@fragment
fn main(s: S)
{
}
