function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected ${expected} but got ${actual}`);
}

function shouldThrow(func, errorType) {
    let error;
    try {
        func();
    } catch (e) {
        error = e;
    }

    if (!(error instanceof errorType))
        throw new Error(`Expected ${errorType.name}!`);
}

function testLegacyConstructor(constructor, wrongOptions)
{
    let symbol = null;
    {
        let object = new constructor();
        let newObject = constructor.call(object);
        shouldBe(object, newObject);
        let symbols = Object.getOwnPropertySymbols(newObject);
        shouldBe(symbols.length, 1);
        shouldBe(symbols[0].description, `IntlLegacyConstructedSymbol`);
        shouldBe(newObject[symbols[0]] instanceof constructor, true);
        symbol = symbols[0];
        let descriptor = Object.getOwnPropertyDescriptor(newObject, symbol);
        shouldBe(descriptor.writable, false);
        shouldBe(descriptor.enumerable, false);
        shouldBe(descriptor.configurable, false);
    }
    {
        let object = new constructor();
        Object.freeze(object);
        shouldThrow(() => constructor.call(object), TypeError);
    }
    {
        let object = {
            __proto__: constructor.prototype,
            [symbol]: new constructor()
        };
        shouldBe(typeof constructor.prototype.resolvedOptions.call(object), `object`);
        shouldThrow(() => {
            constructor.prototype.formatToParts.call(object);
        }, TypeError);
    }
    {
        let object = { __proto__: constructor.prototype };
        shouldThrow(() => {
            constructor.call(object, 'en', wrongOptions);
        }, RangeError);
    }
    {
        Object.defineProperty(constructor, Symbol.hasInstance, {
            value: function () { throw new Error("this should not be called"); },
            writable: true,
            configurable: true,
            enumerable: true,
        });
        let object = new constructor();
        let newObject = constructor.call(object);
        shouldBe(object === newObject, true); // Symbol.hasInstance should be ignored for legacy constructor.
    }
    {
        let object = {
            [symbol]: new constructor()
        };
        Object.defineProperty(constructor, Symbol.hasInstance, {
            value: function () { return true; },
            writable: true,
            configurable: true,
            enumerable: true,
        });
        shouldThrow(() => {
            constructor.prototype.resolvedOptions.call(object);
        }, TypeError);
        shouldThrow(() => {
            constructor.prototype.formatToParts.call(object);
        }, TypeError);
    }
    {
        let object = { __proto__: constructor.prototype };
        Object.defineProperty(constructor, Symbol.hasInstance, {
            value: function () { throw new Error("this will not be called"); },
            writable: true,
            configurable: true,
            enumerable: true,
        });
        shouldThrow(() => {
            constructor.call(object, 'en', wrongOptions);
        }, RangeError);
        constructor.call(object, 'en');
    }
}

testLegacyConstructor(Intl.NumberFormat, { style: "wrong" });
testLegacyConstructor(Intl.DateTimeFormat, { timeStyle: "wrong" });
