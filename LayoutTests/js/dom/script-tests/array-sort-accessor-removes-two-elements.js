// author: Simon Zünd

let array = [undefined, 'c', /*hole*/, 'b', undefined, /*hole*/, 'a', 'd'];

Object.defineProperty(array, '2', {
  get() { array.pop(); array.pop(); return this.foo; },
  set(v) { this.foo = v; }
});

debug('.sort():');
array.sort();
log(array);

array = [undefined, 'c', /*hole*/, 'b', undefined, /*hole*/, 'a', 'd'];

Object.defineProperty(array, '2', {
  get() { return this.foo; },
  set(v) { array.pop(); array.pop(); this.foo = v; }
});

debug('.sort(comparator):');
array.sort((a, b) => a - b);
log(array);
