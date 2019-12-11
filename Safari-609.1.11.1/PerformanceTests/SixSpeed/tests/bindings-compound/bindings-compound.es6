"use strict";

const b = 2;

function fn() {

  let a = 1;
  a += b;

  return a;
}

assertEqual(fn(), 3);
test(fn);
