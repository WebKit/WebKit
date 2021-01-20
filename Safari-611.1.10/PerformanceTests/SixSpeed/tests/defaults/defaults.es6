function fn(arg = 1, other = 3) {
  return other;
}


assertEqual(fn(), 3);
assertEqual(fn(1, 2), 2);

test(function() {
  fn();
  fn(2);
  fn(2, 4);
});
