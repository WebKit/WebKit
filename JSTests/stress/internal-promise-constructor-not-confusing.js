function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

var InternalPromise = $vm.createBuiltin(`(function () {
    return @InternalPromise;
})`)();

var isPromise = $vm.createBuiltin(`(function (p) {
    return @isPromise(p);
})`);

function DerivedPromise() { }

for (var i = 0; i < 1e4; ++i) {
    var promise = Reflect.construct(InternalPromise, [function (resolve) { resolve(42); }], DerivedPromise);
    shouldBe(promise.toString(), `[object Object]`);
    shouldBe(isPromise(promise), true);
}
drainMicrotasks();
for (var i = 0; i < 1e4; ++i) {
    var promise = Reflect.construct(Promise, [function (resolve) { resolve(42); }], DerivedPromise);
    shouldBe(promise.toString(), `[object Object]`);
    shouldBe(isPromise(promise), true);
}
drainMicrotasks();
for (var i = 0; i < 1e4; ++i) {
    var promise = Reflect.construct(InternalPromise, [function (resolve) { resolve(42); }], DerivedPromise);
    shouldBe(promise.toString(), `[object Object]`);
    shouldBe(isPromise(promise), true);
}
