function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function shouldThrow(func, errorMessage) {
    var errorThrown = false;
    var error = null;
    try {
        func();
    } catch (e) {
        errorThrown = true;
        error = e;
    }
    if (!errorThrown)
        throw new Error('not thrown');
    if (String(error) !== errorMessage)
        throw new Error(`bad error: ${String(error)}`);
}

shouldThrow(() => {
    2n >>> 1n;
}, `TypeError: BigInt does not support >>> operator`);

{
    let called = false;
    shouldThrow(() => {
        2n >>> {
            [Symbol.toPrimitive]() {
                called = true;
                return 2n;
            }
        };
    }, `TypeError: BigInt does not support >>> operator`);
    shouldBe(called, true);
}

{
    let calledLeft = false;
    let calledRight = false;
    shouldThrow(() => {
        ({
            [Symbol.toPrimitive]() {
                shouldBe(calledLeft, false);
                shouldBe(calledRight, false);
                calledLeft = true;
                return 2n;
            }
        }) >>> ({
            [Symbol.toPrimitive]() {
                shouldBe(calledLeft, true);
                shouldBe(calledRight, false);
                calledRight = true;
                return 2n;
            }
        });
    }, `TypeError: BigInt does not support >>> operator`);
    shouldBe(calledLeft, true);
    shouldBe(calledRight, true);
}

{
    let calledLeft = false;
    let calledRight = false;
    shouldThrow(() => {
        ({
            [Symbol.toPrimitive]() {
                shouldBe(calledLeft, false);
                shouldBe(calledRight, false);
                calledLeft = true;
                return 2;
            }
        }) >>> ({
            [Symbol.toPrimitive]() {
                shouldBe(calledLeft, true);
                shouldBe(calledRight, false);
                calledRight = true;
                return 2n;
            }
        });
    }, `TypeError: BigInt does not support >>> operator`);
    shouldBe(calledLeft, true);
    shouldBe(calledRight, true);
}

{
    let calledLeft = false;
    let calledRight = false;
    shouldThrow(() => {
        ({
            [Symbol.toPrimitive]() {
                shouldBe(calledLeft, false);
                shouldBe(calledRight, false);
                calledLeft = true;
                return 2n;
            }
        }) >>> ({
            [Symbol.toPrimitive]() {
                shouldBe(calledLeft, true);
                shouldBe(calledRight, false);
                calledRight = true;
                return 2;
            }
        });
    }, `TypeError: BigInt does not support >>> operator`);
    shouldBe(calledLeft, true);
    shouldBe(calledRight, true);
}
