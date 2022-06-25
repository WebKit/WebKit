// author: Simon Zünd

const array = [undefined, 'c', /*hole*/, 'b', undefined, /*hole*/, 'a', 'd'];

const proxy = new Proxy({}, {
  get: (target, name) => {
    debug("get ['" + name + "']");
    return target[name];
  },

  set: (target, name, value) => {
    debug("set ['" + name + "'] = " + value);
    target[name] = value;
    return true;
  },

  has: (target, name) => {
    debug("has ['" + name + "']");
    return name in target;
  },

  deleteProperty: (target, name) => {
    debug("delete ['" + name + "']");
    return delete target[name];
  }
});

array.__proto__ = proxy;

debug('.sort(comparator):');
Array.prototype.sort.call(array, (a, b) => a - b);
