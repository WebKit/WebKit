//@ requireOptions("--useExplicitResourceManagement=1")

function shouldBe(a, b) {
    if (a !== b)
        throw new Error(`Expected ${b} but got ${a}`);
}

function test(thisObj) {
    return Iterator.prototype[Symbol.dispose].call(thisObj);
}


{
    let returnGetCount = 0;
    const obj = {
        get return() {
            returnGetCount++;
            return undefined;
        }
    };
    test(obj);
    shouldBe(returnGetCount, 1);
    test(obj);
    shouldBe(returnGetCount, 2);
}

{
    let returnCallCount = 0;
    const obj = {
        return() {
            returnCallCount++;
        }
    };
    test(obj);
    shouldBe(returnCallCount, 1);
    test(obj);
    shouldBe(returnCallCount, 2);
}
