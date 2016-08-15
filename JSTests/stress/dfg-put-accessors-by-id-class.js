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

function getter()
{
    class Cocoa {
        get hello() {
            return 42;
        }
    }

    let object = new Cocoa();
    testAttribute(object.__proto__, 'hello', 'get');
    return object.hello;
}
noInline(getter);

function setter()
{
    class Cocoa {
        constructor() {
            this.value = 0;
        }
        set hello(value) {
            this.value = value;
        }
    }

    let object = new Cocoa();
    testAttribute(object.__proto__, 'hello', 'set');
    object.hello = 42;
    return object.value;

}
noInline(setter);

function accessors()
{
    class Cocoa {
        constructor() {
            this.value = 0;
        }
        get hello() {
            return this.value;
        }
        set hello(value) {
            this.value = value;
        }
    }

    let object = new Cocoa();
    testAttribute(object.__proto__, 'hello', 'getset');
    object.hello = 42;
    return object.hello;
}
noInline(accessors);

for (var i = 0; i < 10000; ++i) {
    shouldBe(getter(), 42);
    shouldBe(setter(), 42);
    shouldBe(accessors(), 42);
}
