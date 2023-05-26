// RUN: %wgslc

fn testImplicitConversion()
{
    // This is a trivial test case, but it exercises every path of the overload
    // resolution involving refernces:
    // - Unpacking the reference for validating the argument
    // - Matching against the previously inferred type (AbstractInt)
    // - Satisfying a constraint (Number)
    var x: i32 = 0;
    _ = 0 + x;
    _ = x + 0;
}
