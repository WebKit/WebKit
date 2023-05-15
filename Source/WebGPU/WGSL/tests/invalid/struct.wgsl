// RUN: %not %wgslc | %check

struct S {
    x: f32,
    y: i32,
}

fn testStructConstructor()
{
    // CHECK-L: struct initializer has too few inputs: expected 2, found 1
    _ = S(0);

    // CHECK-L: struct initializer has too many inputs: expected 2, found 3
    _ = S(0, 0, 0);

    // CHECK-L: type in struct initializer does not match struct member type: expected 'f32', found 'i32'
    _ = S(0i, 0);

    // CHECK-L: type in struct initializer does not match struct member type: expected 'i32', found 'f32'
    _ = S(0f, 0f);
}
