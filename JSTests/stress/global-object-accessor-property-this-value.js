function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`Bad value: ${actual}.`);
}

(function sameRealmNonIndex() {
    let __foo = -1;
    Object.defineProperty(globalThis, "foo", {
        get() {
            shouldBe(this, globalThis);
            return __foo;
        },
        set(value) {
            shouldBe(this, globalThis);
            __foo = value;
        },
    });

    for (let i = 0; i < 1e6; i++) {
        shouldBe(foo, i - 1);
        foo = i;
        shouldBe(foo, i);
    }
})();

(function sameRealmIndexed() {
    let __0 = -1;
    Object.defineProperty(globalThis, "0", {
        get() {
            shouldBe(this, globalThis);
            return __0;
        },
        set(value) {
            shouldBe(this, globalThis);
            __0 = value;
        },
    });

    for (let i = 0; i < 1e6; i++) {
        shouldBe(globalThis[0], i - 1);
        globalThis[0] = i;
        shouldBe(globalThis[0], i);
    }
})();

(function crossRealmNonIndex() {
    const OtherFunction = $vm.createGlobalObject().Function;

    Object.defineProperty(globalThis, "bar", {
        get: new OtherFunction("return this;"),
        set: new OtherFunction("value", "value.thisValue = this;"),
    });

    for (let i = 0; i < 1e6; i++) {
        shouldBe(bar, globalThis);

        const value = {};
        bar = value;
        shouldBe(value.thisValue, globalThis);
    }
})();

(function crossRealmIndexed() {
    const OtherFunction = $vm.createGlobalObject().Function;

    Object.defineProperty(globalThis, "1", {
        get: new OtherFunction("return this;"),
        set: new OtherFunction("value", "value.thisValue = this;"),
    });

    for (let i = 0; i < 1e6; i++) {
        shouldBe(globalThis[1], globalThis);

        const value = {};
        globalThis[1] = value;
        shouldBe(value.thisValue, globalThis);
    }
})();
