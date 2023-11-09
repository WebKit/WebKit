// RUN: %not %wgslc | %check

fn testExplicitTypeArguments() {
  // CHECK-L: no matching overload for initializer vec2<i32>(<AbstractFloat>, <AbstractFloat>)
  let v0 = vec2<i32>(0.0, 0.0);
}

struct S {
  x: i32,
};

fn testConstraints() {
    let x = S(0);

    // CHECK-L: no matching overload for operator +(S, S)
    let x2 = x + x;

    // CHECK-L:  no matching overload for initializer vec2(S, S)
    let x3 = vec2(x, x);

    // CHECK-L: no matching overload for initializer mat2x2(u32, u32, u32, u32)
    let x4 = mat2x2(0u, 0u, 0u, 0u);

    // CHECK-L: no matching overload for initializer vec2<S>(S, S)
    let x5 = vec2<S>(x, x);

    // CHECK-L:  no matching overload for initializer vec2<S>(vec2<<AbstractInt>>)
    let x6 = vec2<S>(vec2(0, 0));

    // CHECK-L: no matching overload for operator -(u32)
    let x7 = -1u;

    // CHECK-L: no matching overload for operator -(vec2<u32>)
    let x8 = -vec2(1u, 1u);

    // CHECK-L: cannot initialize var of type 'u32' with value of type 'i32'
    const x: u32 = 1i << 1;
}

fn testBottomOverload() {
  // CHECK-L: no matching overload for initializer vec2<f32>(i32)
  let m = vec2<f32>(0i);

  // CHECK-NOT-L: no matching overload for operator
  let x = m + 2;
}
