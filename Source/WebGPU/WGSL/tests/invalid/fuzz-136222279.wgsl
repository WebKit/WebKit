// RUN: %not %wgslc | %check

struct S {
    // CHECK-L: @size value must be at least the byte-size of the type of the member
    @size(2) x: i32,
}

struct T {
    // Test that we don't crash when referring to a struct that contains errors
    @align(32) x: S,
}
