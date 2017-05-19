var data = {
  a: 'foo',
  b: {c: 'd'},
  arr: [1, 2, 3]
};

function fn() {
  var {a, b:{c:b}, arr:[, c]} = data;
  return c;
}

assertEqual(fn(), 2);
test(fn);
