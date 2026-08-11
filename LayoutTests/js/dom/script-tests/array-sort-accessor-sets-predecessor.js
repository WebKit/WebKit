// author: Simon Zünd

let array = [undefined, 'c', /*hole*/, 'b', undefined, /*hole*/, 'a', 'd'];

Object.defineProperty(array, '2', {
  get() { array[1] = 'foobar'; return this.foo; },
  set(v) { this.foo = v; }
});

debug('.sort(comparator):');
array.sort((a, b) => a - b);
log(array);

array = [undefined, 'c', /*hole*/, 'b', undefined, /*hole*/, 'a', 'd'];

Object.defineProperty(array, '2', {
  get() { return this.foo; },
  set(v) { array[1] = 'foobar'; this.foo = v; }
});

debug('.sort():');
array.sort();
log(array);
