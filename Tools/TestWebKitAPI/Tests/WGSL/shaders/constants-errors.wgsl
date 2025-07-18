// RUN: %not %wgslc | %check


fn testOutOfBounds()
{
    {
        const x = array(0);
        // CHECK-L: index -1 is out of bounds [0..0]
        _ = x[-1];
        // CHECK-NOT-L: index 0 is out of bounds [0..0]
        _ = x[0];
        // CHECK-L: index 1 is out of bounds [0..0]
        _ = x[1];
    }

    {
        const x = mat2x2(0, 0, 0, 0);
        // CHECK-L: index -1 is out of bounds [0..1]
        _ = x[-1];
        // CHECK-NOT-L: index 0 is out of bounds [0..1]
        _ = x[0];
        // CHECK-NOT-L: index 1 is out of bounds [0..1]
        _ = x[1];
        // CHECK-L: index 2 is out of bounds [0..1]
        _ = x[2];
    }

    {
        const x = vec2(0);
        // CHECK-L: index -1 is out of bounds [0..1]
        _ = x[-1];
        // CHECK-NOT-L: index 0 is out of bounds [0..1]
        _ = x[0];
        // CHECK-NOT-L: index 1 is out of bounds [0..1]
        _ = x[1];
        // CHECK-L: index 2 is out of bounds [0..1]
        _ = x[2];
    }
}

fn testInvalidExplicitU32Conversion()
{
    // CHECK-L: value 37359285590000 cannot be represented as 'u32'
    let x = u32(37359285590000);
}

fn main()
{
    testOutOfBounds();
    testInvalidExplicitU32Conversion();
}
