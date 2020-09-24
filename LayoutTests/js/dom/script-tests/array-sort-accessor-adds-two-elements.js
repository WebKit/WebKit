// author: Simon Zünd

let array = [undefined, 'c', /*hole*/, 'b', undefined, /*hole*/, 'a', 'd'];

Object.defineProperty(array, '2', {
  get() { array.push('foo'); array.push('bar'); return this.foo; },
  set(v) { this.foo = v; }
});

debug('.sort(comparator):');
array.sort((a, b) => a - b);
log(array);

array = [undefined, 'c', /*hole*/, 'b', undefined, /*hole*/, 'a', 'd'];

Object.defineProperty(array, '2', {
  get() { return this.foo; },
  set(v) {  array.push('foo'); array.push('bar'); this.foo = v; }
});

debug('.sort():');
array.sort();
log(array);
