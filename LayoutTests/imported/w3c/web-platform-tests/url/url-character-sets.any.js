function checkHostCodePoint(codePoint, forbidden) {
  let caught = false;
  try {
    new URL("https://%" + codePoint.toString(16));
  } catch (e) {
    caught = true;
  }
  assert_equals(caught, forbidden, "percent encoded");
  caught = false;
  try {
    new URL("https://" + String.fromCodePoint(codePoint));
  } catch (e) {
    caught = true;
  }
  assert_equals(caught, forbidden, "not percent encoded");
}

function isForbiddenHostCodePoint(codePoint) {
  return codePoint <= 0x20
    || codePoint == 0x23
    || codePoint == 0x25
    || codePoint == 0x2F
    || codePoint == 0x3A
    || codePoint == 0x3C
    || codePoint == 0x3E
    || codePoint == 0x3F
    || codePoint == 0x40
    || codePoint == 0x5B
    || codePoint == 0x5C
    || codePoint == 0x5D
    || codePoint == 0x5E
    || codePoint == 0x7C
    || codePoint == 0x7F;
}

for (let codePoint = 0; codePoint <= 0x7f; codePoint++) {
  test(() => {
    checkHostCodePoint(codePoint, isForbiddenHostCodePoint(codePoint));
  }, "https host code point 0x" + codePoint.toString(16) + " (" + String.fromCodePoint(codePoint) + ")")
}
