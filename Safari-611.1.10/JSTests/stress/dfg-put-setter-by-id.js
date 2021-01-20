function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function testAttribute(object, name, type) {
    shouldBe(Reflect.has(object, name), true);
    let desc = Reflect.getOwnPropertyDescriptor(object, name);
    shouldBe(desc.configurable, true);
    shouldBe(desc.enumerable, true);
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

function setter(name)
{
    var object = {
        set hello(value) {
            this.ok = value;
        },
        [name]: 0
    };

    testAttribute(object, 'hello', 'set');
    object.hello = 42;
    return object.ok;

}
noInline(setter);

for (var i = 0; i < 10000; ++i)
    shouldBe(setter('ok'), 42);
