"use strict";

const a = 1;
let b = 2;

assertEqual(a+b, 3);

test(function() {
  return a + b;
});
