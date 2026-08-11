// author: Simon Zünd

Object.defineProperty(Object.prototype, '2', {
  value: 'foo',
  configurable: true,
  enumerable: true,
  writable: false,
});

const array = [undefined, 'c', /*hole*/, 'b', undefined, /*hole*/, 'a', 'd'];

shouldThrow('array.sort()', '"TypeError: Attempted to assign to readonly property."');
