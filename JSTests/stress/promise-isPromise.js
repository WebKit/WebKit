//@ requireOptions("--usePromiseIsPromise=1")

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected ${expected} but got ${actual}`);
}

shouldBe(Promise.isPromise(undefined), false);
shouldBe(Promise.isPromise(null), false);
shouldBe(Promise.isPromise(true), false);
shouldBe(Promise.isPromise(42), false);
shouldBe(Promise.isPromise("test"), false);
shouldBe(Promise.isPromise([]), false);
shouldBe(Promise.isPromise({}), false);

shouldBe(Promise.isPromise(Promise), false);
shouldBe(Promise.isPromise(Promise.prototype), false);
shouldBe(Promise.isPromise(new Promise(() => {})), true);
shouldBe(Promise.isPromise(Promise.resolve()), true);
shouldBe(Promise.isPromise(Promise.reject()), true);

shouldBe(Promise.isPromise((async function() { })()), true);

class CustomPromise extends Promise { }
shouldBe(Promise.isPromise(CustomPromise), false);
shouldBe(Promise.isPromise(CustomPromise.prototype), false);
shouldBe(Promise.isPromise(new CustomPromise(() => {})), true);

class Thenable {
    then() { }
}
shouldBe(Promise.isPromise(Thenable), false);
shouldBe(Promise.isPromise(Thenable.prototype), false);
shouldBe(Promise.isPromise(new Thenable), false);
