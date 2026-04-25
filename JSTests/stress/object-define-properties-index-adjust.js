function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

class InterfaceConstructor {
  two() {}
}

class Interface {}

Object.defineProperties(Interface.prototype, {
  one: {
    __proto__: null,
  },

  two: {
    value: InterfaceConstructor.prototype.two,
  },
});

var interface = new Interface();

shouldBe(typeof interface.two, 'function');
