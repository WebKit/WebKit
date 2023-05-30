// RUN: %wgslc

fn testLiteralConstants()
{
    {
        const x = 1;
        _ = countOneBits(x);
    }

    {
        const x = 1i;
        _ = countOneBits(x);
    }

    {
        const x = true;
        if x {
        }
    }

    {
        const x = 3.0;
        _ = sqrt(x);
    }

    {
        const x = 3f;
        _ = sqrt(x);
    }
}

fn testArrayConstants() -> i32
{
    if (false) {
        const a = array(0, 0, 0);
        return a[0];
    }
    return 0;
}
