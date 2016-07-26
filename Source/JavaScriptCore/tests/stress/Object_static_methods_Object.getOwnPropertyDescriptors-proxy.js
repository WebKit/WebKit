function shouldBe(expected, actual, msg) {
    if (msg === void 0)
        msg = '';
    else
        msg = ' for ' + msg;
    if (actual !== expected)
        throw new Error('bad value' + msg + ': ' + actual + '. Expected ' + expected);
}

function shouldBeDataProperty(expected, value, name) {
    if (name === void 0)
        name = '<property descriptor>';
    shouldBe(value, expected.value, name + '.value');
    shouldBe(true, expected.enumerable, name + '.enumerable');
    shouldBe(true, expected.configurable, name + '.configurable');
    shouldBe(true, expected.writable, name + '.writable');
    shouldBe(undefined, expected.get, name + '.get');
    shouldBe(undefined, expected.set, name + '.set');
}

(function testPropertyFilteringAndOrder() {
    var log = [];
    var sym = Symbol('test');
    var O = {
        0: 0,
        [sym]: 3,
        'a': 2,
        1: 1
    };

    var P = new Proxy(O, {
        ownKeys(target) {
            log.push('ownKeys()');
            return Reflect.ownKeys(target);
        },
        getOwnPropertyDescriptor(target, name) {
            log.push(`getOwnPropertyDescriptor(${String(name)})`);
            return Reflect.getOwnPropertyDescriptor(target, name);
        },
        get() { throw new Error('[[Get]] trap should be unreachable'); },
        set() { throw new Error('[[Set]] trap should be unreachable'); },
        deleteProperty() { throw new Error('[[Delete]] trap should be unreachable'); },
        defineProperty() { throw new Error('[[DefineOwnProperty]] trap should be unreachable'); }
    });

    var result = Object.getOwnPropertyDescriptors(P);
    shouldBe('ownKeys()|getOwnPropertyDescriptor(0)|getOwnPropertyDescriptor(1)|getOwnPropertyDescriptor(a)|getOwnPropertyDescriptor(Symbol(test))', log.join('|'));
    shouldBeDataProperty(result[0], 0, 'result[0]');
    shouldBeDataProperty(result[1], 1, 'result[1]');
    shouldBeDataProperty(result.a, 2, 'result["a"]');
    shouldBeDataProperty(result[sym], 3, 'result[Symbol(test)]');

    var result2 = Object.getOwnPropertyDescriptors(O);
    shouldBeDataProperty(result2[0], 0, 'result2[0]');
    shouldBeDataProperty(result2[1], 1, 'result2[1]');
    shouldBeDataProperty(result2.a, 2, 'result2["a"]');
    shouldBeDataProperty(result2[sym], 3, 'result2[Symbol(test)]');
})();

(function testDuplicatePropertyNames() {
    var i = 0;
    var log = [];
    var P = new Proxy({}, {
    ownKeys() {
        log.push(`ownKeys()`);
        return [ 'A', 'A' ];
    },
    getOwnPropertyDescriptor(t, name) {
        log.push(`getOwnPropertyDescriptor(${name})`);
        if (i++) return;
        return {
            configurable: true,
            writable: false,
            value: 'VALUE'
        };
    },
    get() { throw new Error('[[Get]] trap should be unreachable'); },
    set() { throw new Error('[[Set]] trap should be unreachable'); },
    deleteProperty() { throw new Error('[[Delete]] trap should be unreachable'); },
    defineProperty() { throw new Error('[[DefineOwnProperty]] trap should be unreachable'); }
  });

  var result = Object.getOwnPropertyDescriptors(P);
  shouldBe(true, result.A.configurable, 'for result.A.configurable');
  shouldBe(false, result.A.writable, 'for result.A.writable');
  shouldBe('VALUE', result.A.value, 'for result.A.value');
  shouldBe(false, result.A.enumerable, 'for result.A.enumerable');
  shouldBe(true, Object.hasOwnProperty.call(result, 'A'));
  shouldBe('ownKeys()|getOwnPropertyDescriptor(A)|getOwnPropertyDescriptor(A)', log.join('|'));
})();

(function testUndefinedPropertyDescriptor() {
    var log = [];
    var P = new Proxy({}, {
        ownKeys() {
            log.push(`ownKeys()`);
            return ['fakeProperty'];
        },
        getOwnPropertyDescriptor(t, name) {
            log.push(`getOwnPropertyDescriptor(${name})`);
            return undefined;
        },
        get() { throw new Error('[[Get]] trap should be unreachable'); },
        set() { throw new Error('[[Set]] trap should be unreachable'); },
        deleteProperty() { throw new Error('[[Delete]] trap should be unreachable'); },
        defineProperty() { throw new Error('[[DefineOwnProperty]] trap should be unreachable'); }
    });

    var result = Object.getOwnPropertyDescriptors(P);
    shouldBe(false, result.hasOwnProperty('fakeProperty'));
    shouldBe(false, 'fakeProperty' in result);
    shouldBe('ownKeys()|getOwnPropertyDescriptor(fakeProperty)', log.join('|'));
})();
