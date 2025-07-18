// RUN: %not %wgslc | %check

struct S { x: i32 }

var<private> s : S;

// CHECK-L: cannot use value 's' as type
fn f() -> s
{
    // CHECK-NEXT-L: unresolved type 'undefined'
    var x : undefined;

    let s = S(42);
    // CHECK-NEXT-L: cannot call value of type 'S'
    _ = s(42);

    // CHECK-NEXT-L: cannot use type 'S' as value
    _ = S;

    // CHECK-NEXT-L: unresolved identifier 'undefined'
    _ = undefined;
}
