function assert_identity_2d_matrix(actual) {
  assert_true(actual instanceof DOMMatrixReadOnly);
  assert_true(actual.is2D, "is2D");
  assert_true(actual.isIdentity, "isIdentity");
  assert_identity_matrix(actual);
}

function assert_identity_3d_matrix(actual) {
  assert_true(actual instanceof DOMMatrixReadOnly);
  assert_false(actual.is2D, "is2D");
  assert_true(actual.isIdentity, "isIdentity");
  assert_identity_matrix(actual);
}

function assert_identity_matrix(actual) {
  assert_array_equals(actual.toFloat64Array(), [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1]);
}

function toArray(actual) {
  var array = actual.toFloat64Array();
  // Do not care negative zero for testing accommodation.
  for (var i = 0; i < array.length; i++) {
    if (array[i] === -0)
      array[i] = 0;
  }
  return array;
}

function assert_2d_matrix_equals(actual, expected) {
  assert_true(actual instanceof DOMMatrixReadOnly);
  assert_true(Array.isArray(expected));
  assert_equals(6, expected.length, "expected.length");
  assert_true(actual.is2D, "is2D");
  assert_false(actual.isIdentity, "isIdentity");
  assert_array_equals(toArray(actual), [
    expected[0], expected[1], 0, 0,
    expected[2], expected[3], 0, 0,
    0, 0, 1, 0,
    expected[4], expected[5], 0, 1
  ]);
}

function assert_3d_matrix_equals(actual, expected) {
  assert_true(actual instanceof DOMMatrixReadOnly);
  assert_true(Array.isArray(expected) );
  assert_equals(16, expected.length, "expected.length");
  assert_false(actual.is2D, "is2D");
  assert_false(actual.isIdentity, "isIdentity");
  assert_array_equals(toArray(actual), [
    expected[0], expected[1], expected[2], expected[3],
    expected[4], expected[5], expected[6], expected[7],
    expected[8], expected[9], expected[10], expected[11],
    expected[12], expected[13], expected[14], expected[15],
  ]);
}

function assert_array_almost_equals(actual, expected) {
  for (var i = 0; i < actual.length; i++) {
    assert_equals(actual[i].toFixed(10), expected[i].toFixed(10));
  }
}

function assert_matrix_almost_equals(actual, expected) {
  assert_array_almost_equals(actual.toFloat64Array(), expected.toFloat64Array());
}

function assert_dom_point_equals(actual, expected) {
  assert_true(actual instanceof DOMPointReadOnly);
  if(Array.isArray(expected)) {
    assert_equals(expected.length, 4);
    assert_equals(actual.x, expected[0], "point equality: x differs");
    assert_equals(actual.y, expected[1], "point equality: y differs");
    assert_equals(actual.z, expected[2], "point equality: z differs");
    assert_equals(actual.w, expected[3], "point equality: w differs");
  } else if(expected instanceof DOMPointReadOnly) {
    assert_equals(actual.x, expected.x, "point equality: x differs");
    assert_equals(actual.y, expected.y, "point equality: y differs");
    assert_equals(actual.z, expected.z, "point equality: z differs");
    assert_equals(actual.w, expected.w, "point equality: w differs");
  } else {
    assert_unreached();
  }
}
 
function assert_dom_rect_equals(actual, expected) {
  assert_true(actual instanceof DOMRectReadOnly);
  if(Array.isArray(expected)) {
    assert_equals(expected.length, 8);
    assert_equals(actual.x, expected[0], "rect equality: x differs");
    assert_equals(actual.y, expected[1], "rect equality: y differs");
    assert_equals(actual.width, expected[2], "rect equality: width differs");
    assert_equals(actual.height, expected[3], "rect equality: height differs");
    assert_equals(actual.top, expected[4], "rect equality: top differs");
    assert_equals(actual.right, expected[5], "rect equality: right differs");
    assert_equals(actual.bottom, expected[6], "rect equality: bottom differs");
    assert_equals(actual.left, expected[7], "rect equality: left differs");
  } else if(expected instanceof DOMRectReadOnly) {
    assert_equals(actual.x, expected.x, "rect equality: x differs");
    assert_equals(actual.y, expected.y, "rect equality: y differs");
    assert_equals(actual.width, expected.width, "rect equality: width differs");
    assert_equals(actual.height, expected.height, "rect equality: height differs");
    assert_equals(actual.top, expected.top, "rect equality: top differs");
    assert_equals(actual.right, expected.right, "rect equality: right differs");
    assert_equals(actual.bottom, expected.bottom, "rect equality: bottom differs");
    assert_equals(actual.left, expected.left, "poirectnt equality: left differs");
  } else {
    assert_unreached();
  }
}