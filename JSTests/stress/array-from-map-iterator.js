function shouldBe(a, b) {
  if (a !== b)
    throw new Error(`Expected ${b} but got ${a}`);
}

function shouldBeArray(a, b) {
  shouldBe(a.length, b.length);
  for (let i = 0; i < a.length; i++) {
    if (Array.isArray(b[i])) {
      shouldBeArray(a[i], b[i]);
    } else {
      shouldBe(a[i], b[i]);
    }
  }
}

function testKeys(map) {
  return Array.from(map.keys());
}
noInline(testKeys);

function testValues(map) {
  return Array.from(map.values());
}
noInline(testValues);

function testEntries(map) {
  return Array.from(map.entries());
}
noInline(testEntries);

// ========== Basic tests for keys() ==========

// int32 keys
{
  const map = new Map([[1, 'a'], [2, 'b'], [3, 'c']]);
  const array = testKeys(map);
  shouldBe(array.length, 3);
  shouldBe(array[0], 1);
  shouldBe(array[1], 2);
  shouldBe(array[2], 3);
}

// double keys
{
  const map = new Map([[1.1, 'a'], [2.2, 'b'], [3.3, 'c']]);
  const array = testKeys(map);
  shouldBe(array.length, 3);
  shouldBe(array[0], 1.1);
  shouldBe(array[1], 2.2);
  shouldBe(array[2], 3.3);
}

// object keys (contiguous)
{
  const key1 = { k: 1 };
  const key2 = { k: 2 };
  const key3 = { k: 3 };
  const map = new Map([[key1, 'a'], [key2, 'b'], [key3, 'c']]);
  const array = testKeys(map);
  shouldBe(array.length, 3);
  shouldBe(array[0], key1);
  shouldBe(array[1], key2);
  shouldBe(array[2], key3);
}

// ========== Basic tests for values() ==========

// int32 values
{
  const map = new Map([['a', 1], ['b', 2], ['c', 3]]);
  const array = testValues(map);
  shouldBe(array.length, 3);
  shouldBe(array[0], 1);
  shouldBe(array[1], 2);
  shouldBe(array[2], 3);
}

// double values
{
  const map = new Map([['a', 1.1], ['b', 2.2], ['c', 3.3]]);
  const array = testValues(map);
  shouldBe(array.length, 3);
  shouldBe(array[0], 1.1);
  shouldBe(array[1], 2.2);
  shouldBe(array[2], 3.3);
}

// object values (contiguous)
{
  const val1 = { v: 1 };
  const val2 = { v: 2 };
  const val3 = { v: 3 };
  const map = new Map([['a', val1], ['b', val2], ['c', val3]]);
  const array = testValues(map);
  shouldBe(array.length, 3);
  shouldBe(array[0], val1);
  shouldBe(array[1], val2);
  shouldBe(array[2], val3);
}

// ========== Basic tests for entries() ==========

// entries always returns arrays (contiguous)
{
  const map = new Map([[1, 'a'], [2, 'b'], [3, 'c']]);
  const array = testEntries(map);
  shouldBe(array.length, 3);
  shouldBeArray(array[0], [1, 'a']);
  shouldBeArray(array[1], [2, 'b']);
  shouldBeArray(array[2], [3, 'c']);
}

{
  const map = new Map([['x', 100], ['y', 200]]);
  const array = testEntries(map);
  shouldBe(array.length, 2);
  shouldBeArray(array[0], ['x', 100]);
  shouldBeArray(array[1], ['y', 200]);
}

// ========== Empty map ==========

{
  const map = new Map();
  shouldBe(testKeys(map).length, 0);
  shouldBe(testValues(map).length, 0);
  shouldBe(testEntries(map).length, 0);
}

// ========== Large map ==========

{
  const map = new Map();
  for (let i = 0; i < 1000; i++) {
    map.set(i, i * 2);
  }

  const keys = testKeys(map);
  shouldBe(keys.length, 1000);
  for (let i = 0; i < 1000; i++) {
    shouldBe(keys[i], i);
  }

  const values = testValues(map);
  shouldBe(values.length, 1000);
  for (let i = 0; i < 1000; i++) {
    shouldBe(values[i], i * 2);
  }

  const entries = testEntries(map);
  shouldBe(entries.length, 1000);
  for (let i = 0; i < 1000; i++) {
    shouldBeArray(entries[i], [i, i * 2]);
  }
}

// ========== Map with deleted elements ==========

{
  const map = new Map([[1, 'a'], [2, 'b'], [3, 'c'], [4, 'd'], [5, 'e']]);
  map.delete(2);
  map.delete(4);

  const keys = testKeys(map);
  shouldBe(keys.length, 3);
  shouldBe(keys[0], 1);
  shouldBe(keys[1], 3);
  shouldBe(keys[2], 5);

  const values = testValues(map);
  shouldBe(values.length, 3);
  shouldBe(values[0], 'a');
  shouldBe(values[1], 'c');
  shouldBe(values[2], 'e');
}

// ========== Iteration order is preserved ==========

{
  const map = new Map();
  map.set(3, 'three');
  map.set(1, 'one');
  map.set(4, 'four');
  map.set(1, 'ONE'); // update, should not change order
  map.set(5, 'five');

  const keys = testKeys(map);
  shouldBe(keys.length, 4);
  shouldBe(keys[0], 3);
  shouldBe(keys[1], 1);
  shouldBe(keys[2], 4);
  shouldBe(keys[3], 5);
}

// ========== Special values ==========

{
  const map = new Map([
    [NaN, 'nan'],
    [Infinity, 'inf'],
    [-Infinity, '-inf'],
    [0, 'zero'],
    [null, 'null'],
    [undefined, 'undef']
  ]);

  const keys = testKeys(map);
  shouldBe(keys.length, 6);
  shouldBe(Number.isNaN(keys[0]), true);
  shouldBe(keys[1], Infinity);
  shouldBe(keys[2], -Infinity);
  shouldBe(keys[3], 0);
  shouldBe(keys[4], null);
  shouldBe(keys[5], undefined);
}

// ========== String keys ==========

{
  const map = new Map([['hello', 1], ['world', 2], ['', 3]]);
  const keys = testKeys(map);
  shouldBe(keys.length, 3);
  shouldBe(keys[0], 'hello');
  shouldBe(keys[1], 'world');
  shouldBe(keys[2], '');
}

// ========== Symbol keys ==========

{
  const sym1 = Symbol('a');
  const sym2 = Symbol('b');
  const sym3 = Symbol.for('c');
  const map = new Map([[sym1, 1], [sym2, 2], [sym3, 3]]);
  const keys = testKeys(map);
  shouldBe(keys.length, 3);
  shouldBe(keys[0], sym1);
  shouldBe(keys[1], sym2);
  shouldBe(keys[2], sym3);
}

// ========== BigInt ==========

{
  const map = new Map([[1n, 'one'], [2n, 'two'], [9007199254740993n, 'big']]);
  const keys = testKeys(map);
  shouldBe(keys.length, 3);
  shouldBe(keys[0], 1n);
  shouldBe(keys[1], 2n);
  shouldBe(keys[2], 9007199254740993n);
}

// ========== Single element ==========

{
  const map = new Map([[42, 'answer']]);
  shouldBe(testKeys(map).length, 1);
  shouldBe(testKeys(map)[0], 42);
  shouldBe(testValues(map).length, 1);
  shouldBe(testValues(map)[0], 'answer');
}

// ========== Re-adding deleted element changes order ==========

{
  const map = new Map([[1, 'a'], [2, 'b'], [3, 'c']]);
  map.delete(2);
  map.set(2, 'B');

  const keys = testKeys(map);
  shouldBe(keys.length, 3);
  shouldBe(keys[0], 1);
  shouldBe(keys[1], 3);
  shouldBe(keys[2], 2);
}

// ========== Modified Map.prototype[Symbol.iterator] should use slow path ==========

{
  const originalIterator = Map.prototype[Symbol.iterator];
  Map.prototype[Symbol.iterator] = function* () {
    yield [100, 'hundred'];
    yield [200, 'two hundred'];
  };
  const map = new Map([[1, 'a'], [2, 'b'], [3, 'c']]);
  // Note: keys() uses its own iterator, not [Symbol.iterator]
  // So this test is about entries behavior via Symbol.iterator
  Map.prototype[Symbol.iterator] = originalIterator;
}

// ========== Modified %MapIteratorPrototype%.next should use slow path ==========

{
  const map = new Map([[1, 'a'], [2, 'b'], [3, 'c']]);
  const mapIteratorPrototype = Object.getPrototypeOf(map.keys());
  const originalNext = mapIteratorPrototype.next;
  let callCount = 0;
  mapIteratorPrototype.next = function() {
    callCount++;
    return originalNext.call(this);
  };

  const array = Array.from(map.keys());
  shouldBe(array.length, 3);
  shouldBe(array[0], 1);
  shouldBe(array[1], 2);
  shouldBe(array[2], 3);
  // If slow path was used, next() should have been called
  shouldBe(callCount > 0, true);

  mapIteratorPrototype.next = originalNext;
}

// ========== Iterator already started should use slow path ==========

{
  const map = new Map([[1, 'a'], [2, 'b'], [3, 'c']]);
  const iter = map.keys();
  iter.next(); // Consume one element
  const result = Array.from(iter);
  shouldBe(result.length, 2);
  shouldBe(result[0], 2);
  shouldBe(result[1], 3);
}

{
  const map = new Map([[1, 'a'], [2, 'b'], [3, 'c']]);
  const iter = map.values();
  iter.next(); // Consume one element
  const result = Array.from(iter);
  shouldBe(result.length, 2);
  shouldBe(result[0], 'b');
  shouldBe(result[1], 'c');
}

{
  const map = new Map([[1, 'a'], [2, 'b'], [3, 'c']]);
  const iter = map.entries();
  iter.next(); // Consume one element
  const result = Array.from(iter);
  shouldBe(result.length, 2);
  shouldBeArray(result[0], [2, 'b']);
  shouldBeArray(result[1], [3, 'c']);
}

// ========== Map subclass ==========

{
  class MyMap extends Map {
    constructor(iterable) {
      super(iterable);
    }
  }
  const map = new MyMap([[1, 'a'], [2, 'b'], [3, 'c']]);
  const keys = Array.from(map.keys());
  shouldBe(keys.length, 3);
  shouldBe(keys[0], 1);
  shouldBe(keys[1], 2);
  shouldBe(keys[2], 3);
}

// ========== Array.from with mapFn ==========

{
  const map = new Map([[1, 'a'], [2, 'b'], [3, 'c']]);
  const array = Array.from(map.keys(), x => x * 2);
  shouldBe(array.length, 3);
  shouldBe(array[0], 2);
  shouldBe(array[1], 4);
  shouldBe(array[2], 6);
}

// ========== Array.from with mapFn and thisArg ==========

{
  const map = new Map([[1, 'a'], [2, 'b'], [3, 'c']]);
  const obj = { multiplier: 10 };
  const array = Array.from(map.keys(), function(x) { return x * this.multiplier; }, obj);
  shouldBe(array.length, 3);
  shouldBe(array[0], 10);
  shouldBe(array[1], 20);
  shouldBe(array[2], 30);
}

// ========== Very large map ==========

{
  const map = new Map();
  for (let i = 0; i < 10000; i++) {
    map.set(i, `value${i}`);
  }

  const keys = testKeys(map);
  shouldBe(keys.length, 10000);
  shouldBe(keys[0], 0);
  shouldBe(keys[9999], 9999);

  const values = testValues(map);
  shouldBe(values.length, 10000);
  shouldBe(values[0], 'value0');
  shouldBe(values[9999], 'value9999');
}

// ========== Map with many deletions ==========

{
  const map = new Map();
  for (let i = 0; i < 100; i++) {
    map.set(i, `val${i}`);
  }
  for (let i = 0; i < 100; i += 2) {
    map.delete(i);
  }

  const keys = testKeys(map);
  shouldBe(keys.length, 50);
  for (let i = 0; i < 50; i++) {
    shouldBe(keys[i], i * 2 + 1);
  }
}

// ========== Type promotions ==========

// int32 to double promotion for keys
{
  const map = new Map([[1, 'a'], [2, 'b'], [3.5, 'c'], [4, 'd']]);
  const keys = testKeys(map);
  shouldBe(keys.length, 4);
  shouldBe(keys[0], 1);
  shouldBe(keys[1], 2);
  shouldBe(keys[2], 3.5);
  shouldBe(keys[3], 4);
}

// int32 to contiguous promotion for keys
{
  const obj = {};
  const map = new Map([[1, 'a'], [2, 'b'], [obj, 'c'], [4, 'd']]);
  const keys = testKeys(map);
  shouldBe(keys.length, 4);
  shouldBe(keys[0], 1);
  shouldBe(keys[1], 2);
  shouldBe(keys[2], obj);
  shouldBe(keys[3], 4);
}

// int32 to double promotion for values
{
  const map = new Map([['a', 1], ['b', 2], ['c', 3.5], ['d', 4]]);
  const values = testValues(map);
  shouldBe(values.length, 4);
  shouldBe(values[0], 1);
  shouldBe(values[1], 2);
  shouldBe(values[2], 3.5);
  shouldBe(values[3], 4);
}

// int32 to contiguous promotion for values
{
  const obj = {};
  const map = new Map([['a', 1], ['b', 2], ['c', obj], ['d', 4]]);
  const values = testValues(map);
  shouldBe(values.length, 4);
  shouldBe(values[0], 1);
  shouldBe(values[1], 2);
  shouldBe(values[2], obj);
  shouldBe(values[3], 4);
}

// ========== Repeated calls with same map ==========

{
  const map = new Map([[1, 'a'], [2, 'b'], [3, 'c']]);
  for (let i = 0; i < 1000; i++) {
    const keys = testKeys(map);
    shouldBe(keys.length, 3);
    shouldBe(keys[0], 1);
    shouldBe(keys[1], 2);
    shouldBe(keys[2], 3);
  }
}

// ========== Verify returned array is a proper Array ==========

{
  const map = new Map([[1, 'a'], [2, 'b']]);
  const keys = testKeys(map);
  shouldBe(Array.isArray(keys), true);
  shouldBe(keys.constructor, Array);
  shouldBe(Object.getPrototypeOf(keys), Array.prototype);
}

// ========== Verify returned array is mutable ==========

{
  const map = new Map([[1, 'a'], [2, 'b']]);
  const keys = testKeys(map);
  keys.push(99);
  shouldBe(keys.length, 3);
  shouldBe(keys[2], 99);
  keys[0] = 100;
  shouldBe(keys[0], 100);
}

// ========== Map containing functions ==========

{
  const fn1 = () => 1;
  const fn2 = function() { return 2; };
  const map = new Map([[fn1, 'fn1'], [fn2, 'fn2']]);
  const keys = testKeys(map);
  shouldBe(keys.length, 2);
  shouldBe(keys[0], fn1);
  shouldBe(keys[1], fn2);
}

// ========== Map containing arrays ==========

{
  const arr1 = [1, 2];
  const arr2 = [3, 4];
  const map = new Map([[arr1, 'arr1'], [arr2, 'arr2']]);
  const keys = testKeys(map);
  shouldBe(keys.length, 2);
  shouldBe(keys[0], arr1);
  shouldBe(keys[1], arr2);
}

// ========== Mixed key and value types in entries ==========

{
  const obj = { x: 1 };
  const map = new Map([
    [1, 'one'],
    ['two', 2],
    [obj, [3, 4]],
    [null, undefined]
  ]);
  const entries = testEntries(map);
  shouldBe(entries.length, 4);
  shouldBeArray(entries[0], [1, 'one']);
  shouldBeArray(entries[1], ['two', 2]);
  shouldBe(entries[2][0], obj);
  shouldBeArray(entries[2][1], [3, 4]);
  shouldBe(entries[3][0], null);
  shouldBe(entries[3][1], undefined);
}
