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
            this.ok = 0;
        }
        set hello(value) {
            this.ok = value;
        }
        get [name]() {
        }
    }

    let object = new Cocoa();
    testAttribute(object.__proto__, 'hello', 'set');
    testAttribute(object.__proto__, 'dummy', 'get');
    object.hello = 42;
    return object.ok;
}
noInline(getter);

for (var i = 0; i < 10000; ++i)
    shouldBe(getter('dummy'), 42);
