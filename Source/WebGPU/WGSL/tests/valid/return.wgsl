// RUN: %metal-compile main

fn testScalarPromotion() -> f32
{
    return 0;
}

fn testVectorPromotion() -> vec3f
{
    return vec3(0);
}

@compute
@workgroup_size(1, 1, 1)
fn main() {
    _ = testScalarPromotion();
    _ = testVectorPromotion();
}
