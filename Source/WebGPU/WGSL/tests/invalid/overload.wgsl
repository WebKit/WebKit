fn testExplicitTypeArguments() {
  // CHECK-L: no matching overload for initializer vec2<i32>(<AbstractFloat>, <AbstractFloat>)
  let v0 = vec2<i32>(0.0, 0.0);
}

struct S {
  x: i32,
};

fn testConstraints() {
    var x : S;

    // CHECK-L: no matching overload for operator + (S, S)
    let x2 = x + x;

    // CHECK-L:  no matching overload for initializer vec2(S, S)
    let x3 = vec2(x, x);

    // CHECK-L: no matching overload for initializer mat2x2(u32, u32, u32, u32)
    let x4 = mat2x2(0u, 0u, 0u, 0u);

    // CHECK-L: no matching overload for initializer vec2<S>(S, S)
    let x5 = vec2<S>(x, x);

    // CHECK-L:  no matching overload for initializer vec2<S>(vec2<<AbstractInt>>)
    let x6 = vec2<S>(vec2(0, 0));
}
