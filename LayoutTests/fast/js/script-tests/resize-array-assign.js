description("Tests that assignments into arrays behave properly after the array is resized.")

function resize(X) {
  X[9999] = 0;
  return 1;
}

function getZerothEntry(X) {
  return X[0];
}

var A = new Array();
A[0] = 0;
A[0] = resize(A);

shouldBe(getZerothEntry(A).toString(), "1");

var successfullyParsed = true;
