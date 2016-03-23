function shouldBe(expected, actual, msg) {
    if (msg === void 0)
        msg = '';
    else
        msg = ' for ' + msg;
    if (actual !== expected)
        throw new Error('bad value' + msg + ': ' + actual + '. Expected ' + expected);
}

function shouldThrow(func, errorType) {
    try {
        func();
        throw new Error('Expected ' + func + '() to throw ' + errorType.name + ', but did not throw.');
    } catch (e) {
        if (e instanceof errorType) return;
        throw new Error('Expected ' + func + '() to throw ' + errorType.name + ', but threw ' + e);
    }
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

(function testMeta() {
    shouldBe(1, Object.getOwnPropertyDescriptors.length);

    shouldBe('getOwnPropertyDescriptors', Object.getOwnPropertyDescriptors.name);

    var propertyDescriptor = Reflect.getOwnPropertyDescriptor(Object, 'getOwnPropertyDescriptors');
    shouldBe(false, propertyDescriptor.enumerable);
    shouldBe(true, propertyDescriptor.writable);
    shouldBe(true, propertyDescriptor.configurable);

    shouldThrow(() => new Object.getOwnPropertyDescriptors({}), TypeError);
})();

(function testToObject() {
    shouldThrow(() => Object.getOwnPropertyDescriptors(null), TypeError);
    shouldThrow(() => Object.getOwnPropertyDescriptors(undefined), TypeError);
    shouldThrow(() => Object.getOwnPropertyDescriptors(), TypeError);
})();

(function testPrototypeProperties() {
    function F() {};
    F.prototype.a = 'A';
    F.prototype.b = 'B';
    F.prototype[9999] = '0';
    F.prototype[0] = '0';

    var F2 = new F();
    Object.defineProperties(F2, {
        'b': {
            enumerable: false,
            configurable: true,
            writable: false,
            value: 'Shadowed "B"'
        },
        'c': {
            enumerable: false,
            configurable: true,
            writable: false,
            value: 'C'
        },
        9998: {
            enumerable: true,
            configurable: true,
            writable: true,
            value: 'X'
        }
    });

    var result = Object.getOwnPropertyDescriptors(F2);

    shouldBe(undefined, result[0]);

    shouldBe(result[9998].enumerable, true);
    shouldBe(result[9998].configurable, true);
    shouldBe(result[9998].writable, true);
    shouldBe(result[9998].value, 'X')

    shouldBe(undefined, result[9999]);

    shouldBe(undefined, result.a);

    shouldBe(result.b.enumerable, false);
    shouldBe(result.b.configurable, true);
    shouldBe(result.b.writable, false);
    shouldBe(result.b.value, 'Shadowed "B"');

    shouldBe(result.c.enumerable, false);
    shouldBe(result.c.configurable, true);
    shouldBe(result.c.writable, false);
    shouldBe(result.c.value, 'C');
})();

(function testGlobalProxy(global) {
    var symbol = Symbol('test');
    global[symbol] = 'Symbol(test)';

    var result = Object.getOwnPropertyDescriptors(global);

    shouldBeDataProperty(result[symbol], 'Symbol(test)', 'global[Symbol(test)]');
    delete global[symbol];
})(this);

(function testIndexedProperties() {
    var object = { 0: 'test' };
    var result = Object.getOwnPropertyDescriptors(object);
    shouldBeDataProperty(result[0], 'test', 'result[0]');
    shouldBeDataProperty(result['0'], 'test', 'result["0"]');
    shouldBe(result[0], result['0']);
})();


(function testPropertiesIndexedSetterOnPrototypeThrows() {
    var symbol = Symbol('test');
    Object.defineProperties(Object.prototype, {
        0: {
            configurable: true,
            get() { return; },
            set(v) { throw new Error("Setter on prototype should be unreachable!"); }
        },
        a: {
            configurable: true,
            get() { return; },
            set(v) { throw new Error("Setter on prototype should be unreachable!"); }
        },
        [symbol]: {
            configurable: true,
            get() { return; },
            set(v) { throw new Error("Setter on prototype should be unreachable!"); }
        }
    });
    var result = Object.getOwnPropertyDescriptors({ 0: 1, a: 2, [symbol]: 3 })
    delete Object.prototype[0];
    delete Object.prototype.a;
    delete Object.prototype[symbol];
    shouldBeDataProperty(result[0], 1, 'result[0]');
    shouldBeDataProperty(result.a, 2, 'result["a"]');
    shouldBeDataProperty(result[symbol], 3, 'result[symbol]');
})();
