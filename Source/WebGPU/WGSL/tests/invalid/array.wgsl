// RUN: %not %wgslc | %check

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
