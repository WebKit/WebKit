// RUN: %not %wgslc | %check

fn noReturn()
{
    // CHECK-L: return statement type does not match its function return type, returned 'i32', expected 'void'
    return 0i;
}

fn typeMisMatch() -> f32
{
    // CHECK-L: return statement type does not match its function return type, returned 'i32', expected 'f32'
    return 0i;
}

fn conversionFailure() -> i32
{
    // CHECK-L: value 500000000000 cannot be represented as 'i32'
    return 500000000000;
}
