// author: Simon Zünd

const array = [undefined, 'c', /*hole*/, 'b', undefined, /*hole*/, 'a', 'd'];

Object.defineProperty(array, '1', {
  value: 'c',
  configurable: true,
  enumerable: true,
  writable: false,
});

Object.defineProperty(array, '3', {
  value: 'b',
  configurable: true,
  enumerable: true,
  writable: false,
});

shouldThrow('array.sort((a, b) => a - b)', '"TypeError: Attempted to assign to readonly property."');
