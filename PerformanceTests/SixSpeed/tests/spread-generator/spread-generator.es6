function *generate() {
  yield 1;
  yield 2;
  yield 3;
}

function fn() {
  return Math.max(... generate());
}
assertEqual(fn(), 3);
test(fn);
