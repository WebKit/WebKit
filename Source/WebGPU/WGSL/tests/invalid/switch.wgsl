// RUN: %not %wgslc | %check

fn main() {
    let x: u32 = 42;
    switch 1u {
        // CHECK-L: the case selector values must have the same type as the selector expression: the selector expression has type 'u32' and case selector has type 'i32'
        case 1i, default,: { }
    }

    // CHECK-L: switch selector must be of type i32 or u32
    switch false {
        // CHECK-NOT-L: the case selector values must have the same type as the selector expression: the selector expression has type 'u32' and case selector has type 'i32'
        case 1i, default,: { }
    }
}
