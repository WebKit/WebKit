// RUN: %not %wgslc | %check

// CHECK-L: 'array' requires at least 1 template argument
var<private> a:array;

fn testArrayLengthMismatch() {
  // CHECK-L: array count must be greater than 0
  let x1 = array<i32, 0>();

  // CHECK-L: array constructor has too few elements: expected 2, found 1
  let x2 = array<i32, 2>(0);

  // CHECK-L: array constructor has too many elements: expected 1, found 2
  let x3 = array<i32, 1>(0, 0);

}

fn testRuntimeSizedArray() {
  // CHECK-L: cannot construct a runtime-sized array
  let x1 = array<i32>(0);
}

fn testArrayTypeMismatch() {
  // CHECK-L: '<AbstractFloat>' cannot be used to construct an array of 'i32'
  let x1 = array<i32, 1>(0.0);

}

fn testArrayInferenceError() {
  // CHECK-L: cannot infer array element type from constructor
  let x1 = array();

  // CHECK-L: cannot infer common array element type from constructor arguments
  let x2 = array(0, 0.0, 0u);
}

fn testBottomElementType() {
  // CHECK-L: unresolved type 'i2'
  let xl = array<i2, 1>(0.0);
}

fn testBottomElementCount() {
  // CHECK-L: unresolved identifier 'c'
  let xl = array<i32, c>(0.0);
}

override elementCount = 4;
fn testOverrideElementCount() {
  // CHECK-L: array must have constant size in order to be constructed
  let xl = array<i32, elementCount>(0.0);
}
