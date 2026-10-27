function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + ' expected: ' + expected);
}

function set(target, key, value, receiver) {
    return Reflect.set(target, key, value, receiver);
}
noInline(set);

function warmUp(proxy) {
    Reflect.getOwnPropertyDescriptor(proxy, 'warmUp');
    Reflect.defineProperty(proxy, 'warmUp', { value: 0, writable: true, enumerable: true, configurable: true });
}

for (var i = 0; i < testLoopCount; ++i) {
    for (var warm of [false, true]) {
        var target = $vm.createStaticCustomValue();
        var proxy = new Proxy(target, { });
        if (warm)
            warmUp(proxy);
        shouldBe(set({ }, 'testStaticValueSetFlag', i, proxy), true);
        shouldBe(target.testStaticValueSetterCalled, undefined);
        shouldBe(Object.getOwnPropertyDescriptor(target, 'testStaticValueSetFlag').value, i);

        var reified = $vm.createCustomTestGetterSetter();
        var reifiedProxy = new Proxy(reified, { });
        if (warm)
            warmUp(reifiedProxy);
        shouldBe(set({ }, 'customValue2', i, reifiedProxy), true);
        shouldBe(Object.hasOwn(reified, 'value2'), false);
        shouldBe(Object.getOwnPropertyDescriptor(reified, 'customValue2').value, i);
    }

    var direct = $vm.createStaticCustomValue();
    shouldBe(set({ }, 'testStaticValueSetFlag', i, direct), true);
    shouldBe(direct.testStaticValueSetterCalled, true);
}

var defineCount = 0;
var trapped = $vm.createStaticCustomValue();
var trappedProxy = new Proxy(trapped, {
    defineProperty(t, key, descriptor) {
        ++defineCount;
        return Reflect.defineProperty(t, key, descriptor);
    }
});
shouldBe(set({ }, 'testStaticValue', 42, trappedProxy), true);
shouldBe(defineCount, 1);
shouldBe(Object.getOwnPropertyDescriptor(trapped, 'testStaticValue').value, 42);
