fn testExplicitTypeArguments() {
  // CHECK-L: no matching overload for initializer vec2<i32>(<AbstractFloat>, <AbstractFloat>)
  let v0 = vec2<i32>(0.0, 0.0);
}
