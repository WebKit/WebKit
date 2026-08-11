// author: Simon Zünd

Object.defineProperty(Object.prototype, '2', {
  value: 'foo',
  configurable: false,
  enumerable: true,
  writable: true,
});

const array = [undefined, 'c', /*hole*/, 'b', undefined, /*hole*/, 'a', 'd'];

debug('.sort():');
array.sort();
log(array);
