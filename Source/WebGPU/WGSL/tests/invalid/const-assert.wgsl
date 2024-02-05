// RUN: %not %wgslc | %check

// CHECK-L: const assertion condition must be a bool, got '<AbstractInt>'
const_assert(1);

// CHECK-L: unresolved identifier 'undefined'
const_assert(undefined);

// CHECK-L: cannot use type 'vec2' as value
const_assert(vec2);

// CHECK-L: cannot use type 'i32' as value
const_assert(i32);

// CHECK-L: unresolved identifier 'sqrt'
const_assert(sqrt);

// CHECK-L: const assertion failed
const_assert(1 > 2);

const x = 1;
const y = 2;

// CHECK-L: const assertion failed
const_assert(x > y);

var<private> z = 3;

// CHECK-L: const assertion requires a const-expression
const_assert(x > z);

fn f()
{
    // CHECK-L: const assertion condition must be a bool, got '<AbstractInt>'
    const_assert(1);

    // CHECK-L: unresolved identifier 'undefined'
    const_assert(undefined);

    // CHECK-L: cannot use type 'vec2' as value
    const_assert(vec2);

    // CHECK-L: cannot use type 'i32' as value
    const_assert(i32);

    // CHECK-L: unresolved identifier 'sqrt'
    const_assert(sqrt);

    // CHECK-L: const assertion failed
    const_assert(1 > 2);

    const x = 1;
    const y = 2;

    // CHECK-L: const assertion failed
    const_assert(x > y);

    let z = 3;

    // CHECK-L: const assertion requires a const-expression
    const_assert(x > z);
}

// CHECK-L: cannot use function 'f' as value
const_assert(f);
