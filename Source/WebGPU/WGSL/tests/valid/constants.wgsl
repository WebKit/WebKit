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
