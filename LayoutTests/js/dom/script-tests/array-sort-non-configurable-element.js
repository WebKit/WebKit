// author: Simon Zünd

const array = [undefined, 'c', /*hole*/, 'b', undefined, /*hole*/, 'a', 'd'];

Object.defineProperty(array, '6', {
  value: 'a',
  configurable: false,
  enumerable: true,
  writable: true,
});

Object.defineProperty(array, '7', {
  value: 'd',
  configurable: false,
  enumerable: true,
  writable: true,
});

shouldThrow('array.sort()', '"TypeError: Unable to delete property."');
