function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

var InternalPromise = $vm.createBuiltin(`(function () {
    return @InternalPromise;
})`)();

function DerivedPromise() { }

for (var i = 0; i < 1e4; ++i) {
    var promise = Reflect.construct(InternalPromise, [function (resolve) { resolve(42); }], DerivedPromise);
    shouldBe(promise.toString(), `[object InternalPromise]`);
}
drainMicrotasks();
for (var i = 0; i < 1e4; ++i) {
    var promise = Reflect.construct(Promise, [function (resolve) { resolve(42); }], DerivedPromise);
    shouldBe(promise.toString(), `[object Promise]`);
}
drainMicrotasks();
for (var i = 0; i < 1e4; ++i) {
    var promise = Reflect.construct(InternalPromise, [function (resolve) { resolve(42); }], DerivedPromise);
    shouldBe(promise.toString(), `[object InternalPromise]`);
}
