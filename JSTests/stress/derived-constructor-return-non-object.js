function shouldThrow(testFunction, expectedError) {
    let actualError = null;
    try {
        testFunction();
    } catch (e) {
        actualError = e;
    }

    if (!actualError) {
        throw new Error(
            `Expected to throw "${expectedError}" but no error was thrown`,
        );
    }

    const actualMessage = String(actualError);
    if (actualMessage !== expectedError) {
        throw new Error(
            `Expected error: "${expectedError}"\nActual error: "${actualMessage}"`,
        );
    }
}

{
    class Base {}
    class DerivedReturningNumber extends Base {
        constructor() {
            super();
            return 123;
        }
    }

    shouldThrow(() => {
        new DerivedReturningNumber();
    }, "TypeError: Cannot return a non-object type in the constructor of a derived class DerivedReturningNumber.");
}

{
    class Base {}
    class DerivedReturningString extends Base {
        constructor() {
            super();
            return "hello";
        }
    }

    shouldThrow(() => {
        new DerivedReturningString();
    }, "TypeError: Cannot return a non-object type in the constructor of a derived class DerivedReturningString.");
}

{
    class Base {}
    class DerivedReturningBoolean extends Base {
        constructor() {
            super();
            return true;
        }
    }

    shouldThrow(() => {
        new DerivedReturningBoolean();
    }, "TypeError: Cannot return a non-object type in the constructor of a derived class DerivedReturningBoolean.");
}

{
    class Base {}
    shouldThrow(() => {
        new (class extends Base {
            constructor() {
                super();
                return 42;
            }
        })();
    }, "TypeError: Cannot return a non-object type in the constructor of a derived class.");
}
