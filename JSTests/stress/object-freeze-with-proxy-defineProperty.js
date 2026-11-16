// See https://tc39.es/ecma262/#sec-setintegritylevel (step 7.b.ii)

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`Bad value: ${actual}!`);
}

var seenDescriptors = {};
var proxy = new Proxy({
    foo: 1,
    get bar() {},
    set bar(_v) {},
}, {
    defineProperty: function(target, key, descriptor) {
        seenDescriptors[key] = descriptor;
        return Reflect.defineProperty(target, key, descriptor);
    },
});

Object.freeze(proxy);

shouldBe(seenDescriptors.foo.value, undefined);
shouldBe(seenDescriptors.foo.writable, false);
shouldBe(seenDescriptors.foo.enumerable, undefined);
shouldBe(seenDescriptors.foo.configurable, false);

shouldBe(seenDescriptors.bar.get, undefined);
shouldBe(seenDescriptors.bar.set, undefined);
shouldBe(seenDescriptors.bar.enumerable, undefined);
shouldBe(seenDescriptors.bar.configurable, false);
