//@ requireOptions("--useShadowRealm=1")

function summarizeElement(element) {
  if (typeof element === "object") {
    if (element === null)
      return "null";
    if (Array.isArray(element))
      return summarizeArray(element, 0);
  }
  let result = JSON.stringify(element);
  if (result.length > 20) {
    let c = result[0];
    if (c === "{")
      c = "}";
    result = `${result.substr(0, 15)}...${c}`;
  }
  return result;
}

function summarizeArray(array, index, showIndex) {

  let prefix = index === 0 ? "" : `...${summarizeElement(array[index - 1])}, `;
  let suffix = index + 1 < array.length ? `, ${summarizeElement(array[index + 1])}...` : "";

  if (prefix.length > 20)
    prefix = prefix.substr(prefix.length - 20);
  if (suffix.length > 20)
    suffix = suffix.substr(0, 20);
  ender = showIndex ? ` (at index ${index})` : "";
  return `[${prefix}${JSON.stringify(array[index])}${suffix}]${ender}`;
}
function shouldBeArrayEqual(actual, expected) {
  if ((Array.isArray(expected) && !Array.isArray(actual)))
    throw new Error(`Expected ${JSON.stringify} but found ${JSON.stringify(actual)}`);

  var i;
  for (var i = 0; i < expected.length; ++i) {
    let e = expected[i];
    let a = actual[i];
    if (e === a)
      continue;
    if (Array.isArray(e))
      shouldBeArrayEqual(a, e);
    throw new Error(`Expected ${summarizeArray(expected, i, true)} but found ${summarizeArray(actual, i)}`)
  }
}

function shouldBe(actual, expected) {
    if (Array.isArray(actual) || Array.isArray(expected))
      return shouldBeArrayEqual(actual, expected);
    if (actual !== expected)
        throw new Error(`expected ${expected} but got ${actual}`);
}

function shouldHaveDescriptor(object, propertyName, descriptor) {
  let actual = Object.getOwnPropertyDescriptor(object, propertyName);
  let order = Object.keys(descriptor).sort();
  for (let key of order) {
    let value = descriptor[key];
    if (["undefined", "boolean", "number", "string", "symbol", "bigint"].includes(typeof value) || value === null) {
      if (actual[key] !== value)
        throw new Error(`Expected ${summarizeElement(actual)}'s '${key}' to be ${summarizeElement(value)}, but found ${summarizeElement(actual[key])}`);
    } else if (typeof value === "function") {
      if (actual[key] !== value && String(value) !== String(actual[key]))
        throw new Error(`Expected ${summarizeElement(actual)}'s '${key}' to be ${summarizeElement(value)}, but found ${summarizeElement(actual[key])}`);
    }
  }
}

function shouldThrow(op, errorConstructor, desc) {
    try {
        op();
    } catch (e) {
        if (!(e instanceof errorConstructor)) {
            throw new Error(`threw ${e}, but should have thrown ${errorConstructor.name}`);
        }
        return;
    }
    throw new Error(`Expected ${desc || 'operation'} to throw ${errorConstructor.name}, but no exception thrown`);
}

let realm = new ShadowRealm;

let f = realm.evaluate(`(() => {
  let test1 = function(a, b, c) {
  };
  return test1;
})()`);

shouldHaveDescriptor(f, "length", {
  value: 3,
  writable: false,
  enumerable: false,
  configurable: true,
});
shouldHaveDescriptor(f, "name", {
  value: "test1",
  writable: false,
  enumerable: false,
  configurable: true,
});

f = realm.evaluate(`(() => {
  function test2(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p) {
  }
  return test2;
})()`);

shouldHaveDescriptor(f, "length", {
  value: 16,
  writable: false,
  enumerable: false,
  configurable: true,
});
shouldHaveDescriptor(f, "name", {
  value: "test2",
  writable: false,
  enumerable: false,
  configurable: true,
});

f = realm.evaluate(`(() => {
  let f = function() {
  }
  Object.defineProperty(f, "length", {
    value: -Infinity,
    writable: true,
    configurable: true,
    enumerable: true
  });
  Object.defineProperty(f, "name", {
    value: "27",
    writable: true,
    configurable: true,
    enumerable: true
  });
  return f;
})()`);

shouldHaveDescriptor(f, "length", {
  value: 0,
  writable: false,
  enumerable: false,
  configurable: true,
});
shouldHaveDescriptor(f, "name", {
  value: "27",
  writable: false,
  enumerable: false,
  configurable: true,
});

f = realm.evaluate(`(() => {
  let f = function() {
  }
  Object.defineProperty(f, "length", {
    value: -9.74,
    writable: true,
    configurable: true,
    enumerable: true
  });
  Object.defineProperty(f, "name", {
    value: 57,
    writable: true,
    configurable: true,
    enumerable: true
  });
  return f;
})()`);

shouldHaveDescriptor(f, "length", {
  value: 0,
  writable: false,
  enumerable: false,
  configurable: true,
});
shouldHaveDescriptor(f, "name", {
  value: "",
  writable: false,
  enumerable: false,
  configurable: true,
});

let innerLog = realm.evaluate(`(() => {
  globalThis._log = [];
  globalThis.log = function log(item) {
    globalThis._log.push(item);
  };

  return function log() {
    let log = globalThis._log;
    globalThis._log = [];
    return JSON.stringify(log);
  };
})()`);

function log() {
  let result = JSON.parse(innerLog());
  shouldBe(Array.isArray(result), true);
  return result;
}

f = realm.evaluate(`(() => {
  let target = (function() {
    log(\`called\`);
  });
  let p = new Proxy(target, {
    getOwnPropertyDescriptor(t, p) {
      log(\`getOwnPropertyDescriptor \${p}\`);
      return Reflect.getOwnProperty(...arguments);
    },
    get(t, p) {
      log(\`get \${p}\`);
      return Reflect.get(...arguments);
    },
    defineProperty(t, p, o) {
      log(\`defineProperty \${p}\`);
      return Reflect.defineProperty(...arguments);
    },
    getPrototypeOf(t) {
      log(\`getPrototypeOf\`);
      return Reflect.getPrototypeOf(...arguments);
    },
    setPrototypeOf(t, v) {
      log(\`setPrototypeOf\`);
      return Reflect.setPrototypeOf(...arguments);
    },
    isExtensible(t) {
      log(\`isExtensible\`);
      return Reflect.isExtensible(...arguments);
    },
    preventExtensions(t) {
      log(\`preventExtensions\`);
      return Reflect.preventExtensions(...arguments);
    },
    has(t, p) {
      log(\`has \${p}\`);
      return Reflect.has(...arguments);
    },
    set(t, p, v) {
      log(\`set \${p}\`);
      return Reflect.set(...arguments);
    },
    delete(t, p) {
      log(\`delete \${p}\`);
      return Reflect.delete(...arguments);
    },
    ownKeys(t) {
      log(\`ownKeys\`);
      return Reflect.ownKeys(...arguments);
    },
    apply() {
      log(\`apply\`);
      return Reflect.apply(...arguments);
    },
    construct() {
      log(\`construct\`);
      return Reflect.argumentList(...arguments);
    },
  });
  Object.defineProperty(target, "length", {
    get() {
      log("length getter");
      return Infinity;
    },
    set() {
      log("length setter");
      return false;
    },
    configurable: true,
    enumerable: true
  });
  Object.defineProperty(target, "name", {
    get() {
      log("name getter");
      return Infinity;
    },
    set() {
      log("name setter");
      return false;
    },
    configurable: true,
    enumerable: true
  });
  return p;
})()`);

f();
shouldHaveDescriptor(f, "length", {
  value: Infinity,
  writable: false,
  enumerable: false,
  configurable: true,
});
shouldHaveDescriptor(f, "name", {
  value: "",
  writable: false,
  enumerable: false,
  configurable: true,
});
shouldBe(log(), [
  "get length",
  "length getter",
  "get name",
  "name getter",
  "apply",
  "called"
]);
f();
shouldBe(log(), ["apply", "called"]);

f = realm.evaluate(`(() => {
  let target = (function() {
    log(\`called 2\`);
  });
  let p = new Proxy(target, {
    getOwnPropertyDescriptor(t, p) {
      log(\`getOwnPropertyDescriptor \${p}\`);
      return Reflect.getOwnProperty(...arguments);
    },
    get(t, p) {
      log(\`get \${p}\`);
      return Reflect.get(...arguments);
    },
    defineProperty(t, p, o) {
      log(\`defineProperty \${p}\`);
      return Reflect.defineProperty(...arguments);
    },
    getPrototypeOf(t) {
      log(\`getPrototypeOf\`);
      return Reflect.getPrototypeOf(...arguments);
    },
    setPrototypeOf(t, v) {
      log(\`setPrototypeOf\`);
      return Reflect.setPrototypeOf(...arguments);
    },
    isExtensible(t) {
      log(\`isExtensible\`);
      return Reflect.isExtensible(...arguments);
    },
    preventExtensions(t) {
      log(\`preventExtensions\`);
      return Reflect.preventExtensions(...arguments);
    },
    has(t, p) {
      log(\`has \${p}\`);
      return Reflect.has(...arguments);
    },
    set(t, p, v) {
      log(\`set \${p}\`);
      return Reflect.set(...arguments);
    },
    delete(t, p) {
      log(\`delete \${p}\`);
      return Reflect.delete(...arguments);
    },
    ownKeys(t) {
      log(\`ownKeys\`);
      return Reflect.ownKeys(...arguments);
    },
    apply() {
      log(\`apply\`);
      return Reflect.apply(...arguments);
    },
    construct() {
      log(\`construct\`);
      return Reflect.argumentList(...arguments);
    },
  });
  Object.defineProperty(target, "length", {
    get() { return },
    configurable: true,
    enumerable: true
  });
  Object.defineProperty(target, "name", {
    value: 57,
    configurable: true,
    enumerable: true
  });
  return p;
})()`);

f();
shouldHaveDescriptor(f, "length", {
  value: 0,
  writable: false,
  enumerable: false,
  configurable: true,
});
shouldHaveDescriptor(f, "name", {
  value: "",
  writable: false,
  enumerable: false,
  configurable: true,
});
shouldBe(log(), [
  "get length",
  "get name",
  "apply",
  "called 2"
]);
f();
shouldBe(log(), ["apply", "called 2"]);

shouldHaveDescriptor(f, "length", {
  writable: false,
  enumerable: false,
  configurable: true,
});
shouldHaveDescriptor(f, "name", {
  writable: false,
  enumerable: false,
  configurable: true,
});
