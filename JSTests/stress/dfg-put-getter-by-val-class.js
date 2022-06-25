function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function testAttribute(object, name, type) {
    shouldBe(Reflect.has(object, name), true);
    let desc = Reflect.getOwnPropertyDescriptor(object, name);
    shouldBe(desc.configurable, true);
    shouldBe(desc.enumerable, false);
    if (type === 'get') {
        shouldBe(typeof desc.get, 'function');
        shouldBe(typeof desc.set, 'undefined');
    } else if (type === 'set') {
        shouldBe(typeof desc.get, 'undefined');
        shouldBe(typeof desc.set, 'function');
    } else {
        shouldBe(typeof desc.get, 'function');
        shouldBe(typeof desc.set, 'function');
    }
}
noInline(testAttribute);

function getter(name)
{
    class Cocoa {
        constructor() {
            this.ok = 42;
        }
        get [name]() {
            return this.ok;
        }
    }

    let object = new Cocoa();
    testAttribute(object.__proto__, 'hello', 'get');
    return object.hello;
}
noInline(getter);

for (var i = 0; i < 10000; ++i)
    shouldBe(getter('hello'), 42);
