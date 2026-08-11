// author: Simon Zünd

const array = [undefined, 'c', /*hole*/, 'b', undefined, /*hole*/, 'a', 'd'];

Object.defineProperty(array, '2', {
  get() { debug('get [2]'); return this.foo; },
  set(v) { debug(`set [2] with ${v}`); this.foo = v; }
});

Object.defineProperty(array, '3', {
  get() { debug('get [3]'); return this.bar; },
  set(v) { debug(`set [3] with ${v}`); this.bar = v; }
});

debug('.sort(comparator):');
array.sort((a, b) => a - b);
log(array);
