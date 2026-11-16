// author: Simon Zünd

const array = [undefined, 'c', /*hole*/, 'b', undefined, /*hole*/, 'a', 'd'];

const object = {};

Object.defineProperty(object, '2', {
  get() { debug('get [2]'); return this.foo; },
  set(v) { debug(`set [2] with ${v}`); this.foo = v; }
});

Object.defineProperty(object, '3', {
  get() { debug('get [3]'); return this.bar; },
  set(v) { debug(`set [3] with ${v}`); this.bar = v; }
});

array.__proto__ = object;

debug('.sort(comparator):');
Array.prototype.sort.call(array, (a, b) => a - b);
log(array);
