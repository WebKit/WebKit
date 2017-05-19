function fn() {
  return [1, ... [1, 2, 3]];
}
assertEqual(fn()[3], 3);
test(fn);
