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

(function () {
    class A {
        method() {
            shouldBe(typeof A, 'function');
        }

        static staticMethod() {
            shouldBe(typeof A, 'function');
        }
    }

    let originalA = A;

    originalA.staticMethod();
    (new originalA()).method();
    A = undefined;
    originalA.staticMethod();
    (new originalA()).method();
}());


(function () {
    class A {
        method() {
            shouldThrow(() => {
                A = 42;
            }, `TypeError: Attempted to assign to readonly property.`);
        }

        static staticMethod() {
            shouldThrow(() => {
                A = 42;
            }, `TypeError: Attempted to assign to readonly property.`);
        }
    }

    let originalA = A;

    originalA.staticMethod();
    (new originalA()).method();
    A = undefined;
    originalA.staticMethod();
    (new originalA()).method();
}());
