// author: Simon Zünd

const array = [];

Object.defineProperty(array, '0', {
  get() { debug('get [0]'); return this.foo; },
  set(v) { debug(`set [0] with ${v}`); this.foo = v; }
});

debug(".sort(comparator) 0-length array:");
array.sort((a, b) => a - b);
log(array);

debug(".sort() 1-length array:");
array.push('bar');
array.sort();
log(array);
