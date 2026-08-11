// author: Simon Zünd

const array = [undefined, 'c', /*hole*/, 'b', undefined, /*hole*/, 'a', 'd'];

const proxy = new Proxy(array, {
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

debug('.sort():');
Array.prototype.sort.call(proxy);
