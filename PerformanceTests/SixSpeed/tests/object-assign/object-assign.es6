const obj = {
  a: 1,
  b: true,
  c: function () {},
  d: null,
  e: 'e'
};

const fn = function (src) {
  return Object.assign({}, src);
};

const r = fn(obj);
assertEqual(r.a, obj.a);
assertEqual(r.b, obj.b);
assertEqual(r.c, obj.c);
assertEqual(r.d, obj.d);
assertEqual(r.e, obj.e);

test(function () {
  fn(obj);
});
