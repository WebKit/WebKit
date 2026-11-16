function shouldBe(a, b) {
  if (a !== b)
    throw new Error(`Expected ${b} but got ${a}`);
}

function test(set) {
  return Array.from(set);
}
noInline(test);


// contiguous
{
  const value1 = { value: 1 };
  const value2 = { value: 2 };
  const value3 = { value: 3 };
  const value4 = { value: 4 };
  const value5 = { value: 5 };
  const set = new Set([value1, value2, value3, value4, value5]);
  const array = test(set);
  shouldBe(array.length, 5);
  shouldBe(array[0], value1);
  shouldBe(array[1], value2);
  shouldBe(array[2], value3);
  shouldBe(array[3], value4);
  shouldBe(array[4], value5);
}

// double
{
  const set = new Set([1.1, 2.1, 3.1, 4.1, 5.1]);
  const array = test(set);
  shouldBe(array.length, 5);
  shouldBe(array[0], 1.1);
  shouldBe(array[1], 2.1);
  shouldBe(array[2], 3.1);
  shouldBe(array[3], 4.1);
  shouldBe(array[4], 5.1);
}

// int32
{
  const set = new Set([1, 2, 3, 4, 5]);
  const array = test(set);
  shouldBe(array.length, 5);
  shouldBe(array[0], 1);
  shouldBe(array[1], 2);
  shouldBe(array[2], 3);
  shouldBe(array[3], 4);
  shouldBe(array[4], 5);
}

// empty set
{
  const set = new Set();
  const array = test(set);
  shouldBe(array.length, 0);
}

// mixed types (should use contiguous)
{
  const set = new Set([1, "two", 3.0, { four: 4 }]);
  const array = test(set);
  shouldBe(array.length, 4);
  shouldBe(array[0], 1);
  shouldBe(array[1], "two");
  shouldBe(array[2], 3.0);
  shouldBe(array[3].four, 4);
}

// large set
{
  const largeSet = new Set();
  for (let i = 0; i < 1000; i++) {
    largeSet.add(i);
  }
  const array = test(largeSet);
  shouldBe(array.length, 1000);
  for (let i = 0; i < 1000; i++) {
    shouldBe(array[i], i);
  }
}

// set with deleted elements
{
  const set = new Set([1, 2, 3, 4, 5]);
  set.delete(2);
  set.delete(4);
  const array = test(set);
  shouldBe(array.length, 3);
  shouldBe(array[0], 1);
  shouldBe(array[1], 3);
  shouldBe(array[2], 5);
}

// ensure iteration order is preserved
{
  const set = new Set();
  set.add(3);
  set.add(1);
  set.add(4);
  set.add(1); // duplicate, should be ignored
  set.add(5);
  set.add(9);
  const array = test(set);
  shouldBe(array.length, 5);
  shouldBe(array[0], 3);
  shouldBe(array[1], 1);
  shouldBe(array[2], 4);
  shouldBe(array[3], 5);
  shouldBe(array[4], 9);
}

// special values: NaN, Infinity, -Infinity, 0, null, undefined
// Note: Set treats -0 and +0 as the same value per ECMAScript spec
{
  const set = new Set([NaN, Infinity, -Infinity, 0, null, undefined]);
  const array = test(set);
  shouldBe(array.length, 6);
  shouldBe(Number.isNaN(array[0]), true);
  shouldBe(array[1], Infinity);
  shouldBe(array[2], -Infinity);
  shouldBe(array[3], 0);
  shouldBe(array[4], null);
  shouldBe(array[5], undefined);
}

// strings
{
  const set = new Set(["hello", "world", "", "foo"]);
  const array = test(set);
  shouldBe(array.length, 4);
  shouldBe(array[0], "hello");
  shouldBe(array[1], "world");
  shouldBe(array[2], "");
  shouldBe(array[3], "foo");
}

// symbols
{
  const sym1 = Symbol("a");
  const sym2 = Symbol("b");
  const sym3 = Symbol.for("c");
  const set = new Set([sym1, sym2, sym3]);
  const array = test(set);
  shouldBe(array.length, 3);
  shouldBe(array[0], sym1);
  shouldBe(array[1], sym2);
  shouldBe(array[2], sym3);
}

// BigInt
{
  const set = new Set([1n, 2n, 9007199254740993n]);
  const array = test(set);
  shouldBe(array.length, 3);
  shouldBe(array[0], 1n);
  shouldBe(array[1], 2n);
  shouldBe(array[2], 9007199254740993n);
}

// single element
{
  const set = new Set([42]);
  const array = test(set);
  shouldBe(array.length, 1);
  shouldBe(array[0], 42);
}

// re-adding deleted element changes order
{
  const set = new Set([1, 2, 3]);
  set.delete(2);
  set.add(2);
  const array = test(set);
  shouldBe(array.length, 3);
  shouldBe(array[0], 1);
  shouldBe(array[1], 3);
  shouldBe(array[2], 2);
}

// modified Set.prototype[Symbol.iterator] should use slow path
{
  const originalIterator = Set.prototype[Symbol.iterator];
  Set.prototype[Symbol.iterator] = function* () {
    yield 100;
    yield 200;
  };
  const set = new Set([1, 2, 3]);
  const array = Array.from(set);
  shouldBe(array.length, 2);
  shouldBe(array[0], 100);
  shouldBe(array[1], 200);
  Set.prototype[Symbol.iterator] = originalIterator;
}

// modified Set.prototype.values does NOT affect Array.from
// (Array.from uses Symbol.iterator, not values())
{
  const originalValues = Set.prototype.values;
  Set.prototype.values = function* () {
    yield 999;
  };
  const set = new Set([1, 2, 3]);
  const array = Array.from(set);
  // values() modification doesn't affect Array.from
  shouldBe(array.length, 3);
  shouldBe(array[0], 1);
  shouldBe(array[1], 2);
  shouldBe(array[2], 3);
  Set.prototype.values = originalValues;
}

// Set subclass
{
  class MySet extends Set {
    constructor(iterable) {
      super(iterable);
    }
  }
  const set = new MySet([1, 2, 3]);
  const array = Array.from(set);
  shouldBe(array.length, 3);
  shouldBe(array[0], 1);
  shouldBe(array[1], 2);
  shouldBe(array[2], 3);
}

// Set subclass with overridden iterator
{
  class MySet extends Set {
    *[Symbol.iterator]() {
      yield* [...super.values()].reverse();
    }
  }
  const set = new MySet([1, 2, 3]);
  const array = Array.from(set);
  shouldBe(array.length, 3);
  shouldBe(array[0], 3);
  shouldBe(array[1], 2);
  shouldBe(array[2], 1);
}

// Array.from with mapFn should work correctly
{
  const set = new Set([1, 2, 3]);
  const array = Array.from(set, x => x * 2);
  shouldBe(array.length, 3);
  shouldBe(array[0], 2);
  shouldBe(array[1], 4);
  shouldBe(array[2], 6);
}

// Array.from with mapFn and thisArg
{
  const set = new Set([1, 2, 3]);
  const obj = { multiplier: 10 };
  const array = Array.from(set, function(x) { return x * this.multiplier; }, obj);
  shouldBe(array.length, 3);
  shouldBe(array[0], 10);
  shouldBe(array[1], 20);
  shouldBe(array[2], 30);
}

// very large set
{
  const largeSet = new Set();
  for (let i = 0; i < 10000; i++) {
    largeSet.add(i);
  }
  const array = test(largeSet);
  shouldBe(array.length, 10000);
  shouldBe(array[0], 0);
  shouldBe(array[9999], 9999);
}

// set with many deletions (sparse internal storage)
{
  const set = new Set();
  for (let i = 0; i < 100; i++) {
    set.add(i);
  }
  for (let i = 0; i < 100; i += 2) {
    set.delete(i);
  }
  const array = test(set);
  shouldBe(array.length, 50);
  for (let i = 0; i < 50; i++) {
    shouldBe(array[i], i * 2 + 1);
  }
}

// int32 to double promotion
{
  const set = new Set([1, 2, 3.5, 4, 5]);
  const array = test(set);
  shouldBe(array.length, 5);
  shouldBe(array[0], 1);
  shouldBe(array[1], 2);
  shouldBe(array[2], 3.5);
  shouldBe(array[3], 4);
  shouldBe(array[4], 5);
}

// int32 to contiguous promotion
{
  const obj = {};
  const set = new Set([1, 2, obj, 4, 5]);
  const array = test(set);
  shouldBe(array.length, 5);
  shouldBe(array[0], 1);
  shouldBe(array[1], 2);
  shouldBe(array[2], obj);
  shouldBe(array[3], 4);
  shouldBe(array[4], 5);
}

// double to contiguous promotion
{
  const obj = {};
  const set = new Set([1.1, 2.2, obj, 4.4, 5.5]);
  const array = test(set);
  shouldBe(array.length, 5);
  shouldBe(array[0], 1.1);
  shouldBe(array[1], 2.2);
  shouldBe(array[2], obj);
  shouldBe(array[3], 4.4);
  shouldBe(array[4], 5.5);
}

// repeated calls with same set
{
  const set = new Set([1, 2, 3]);
  for (let i = 0; i < 1000; i++) {
    const array = test(set);
    shouldBe(array.length, 3);
    shouldBe(array[0], 1);
    shouldBe(array[1], 2);
    shouldBe(array[2], 3);
  }
}

// verify returned array is a proper Array
{
  const set = new Set([1, 2, 3]);
  const array = test(set);
  shouldBe(Array.isArray(array), true);
  shouldBe(array.constructor, Array);
  shouldBe(Object.getPrototypeOf(array), Array.prototype);
}

// verify returned array is mutable
{
  const set = new Set([1, 2, 3]);
  const array = test(set);
  array.push(4);
  shouldBe(array.length, 4);
  shouldBe(array[3], 4);
  array[0] = 100;
  shouldBe(array[0], 100);
}

// set containing functions
{
  const fn1 = () => 1;
  const fn2 = function() { return 2; };
  const fn3 = async () => 3;
  const set = new Set([fn1, fn2, fn3]);
  const array = test(set);
  shouldBe(array.length, 3);
  shouldBe(array[0], fn1);
  shouldBe(array[1], fn2);
  shouldBe(array[2], fn3);
}

// set containing arrays
{
  const arr1 = [1, 2];
  const arr2 = [3, 4];
  const set = new Set([arr1, arr2]);
  const array = test(set);
  shouldBe(array.length, 2);
  shouldBe(array[0], arr1);
  shouldBe(array[1], arr2);
}

// modified %SetIteratorPrototype%.next should use slow path
{
  const setIteratorPrototype = Object.getPrototypeOf(new Set()[Symbol.iterator]());
  const originalNext = setIteratorPrototype.next;
  let callCount = 0;
  setIteratorPrototype.next = function() {
    callCount++;
    return originalNext.call(this);
  };
  const set = new Set([1, 2, 3]);
  const array = Array.from(set);
  shouldBe(array.length, 3);
  shouldBe(array[0], 1);
  shouldBe(array[1], 2);
  shouldBe(array[2], 3);
  // If slow path was used, next() should have been called
  shouldBe(callCount > 0, true);
  setIteratorPrototype.next = originalNext;
}
