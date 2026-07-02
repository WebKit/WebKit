// Force definePropertiesSlow by using indexed keys on the properties object.
const properties = {
  0: { value: 0, writable: true, enumerable: true, configurable: true },
  1: { value: 1, writable: true, enumerable: true, configurable: true },
  2: { value: 2, writable: true, enumerable: true, configurable: true },
  3: { value: 3, writable: true, enumerable: true, configurable: true },
  4: { value: 4, writable: true, enumerable: true, configurable: true },
  5: { value: 5, writable: true, enumerable: true, configurable: true },
  6: { value: 6, writable: true, enumerable: true, configurable: true },
  7: { value: 7, writable: true, enumerable: true, configurable: true },
  8: { value: 8, writable: true, enumerable: true, configurable: true },
  9: { value: 9, writable: true, enumerable: true, configurable: true },
  a: { value: 'a', writable: true, enumerable: true, configurable: true },
  b: { value: 'b', writable: true, enumerable: true, configurable: true },
  c: { value: 'c', writable: true, enumerable: true, configurable: true },
};

var count = 50_000;

function test() {
  for (let i = 0; i < count; i++) {
    Object.defineProperties({}, properties);
  }
}

test();
