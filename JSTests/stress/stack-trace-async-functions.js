const source = "stack-trace-async-functions.js";

function nop() {}

function shouldThrowAsync(run, errorType, message, stackFunctions) {
    let actual;
    var hadError = false;
    run().then(
        function (value) {
            actual = value;
        },
        function (error) {
            hadError = true;
            actual = error;
        },
    );
    drainMicrotasks();
    if (!hadError) {
        throw new Error("Expected " + run + "() to throw " + errorType.name + ", but did not throw.");
    }
    if (!(actual instanceof errorType)) {
        throw new Error("Expected " + run + "() to throw " + errorType.name + ", but threw '" + actual + "'");
    }
    if (message !== void 0 && actual.message !== message) {
        throw new Error("Expected " + run + "() to throw '" + message + "', but threw '" + actual.message + "'");
    }

    const stackTrace = actual.stack;
    if (!stackTrace) {
        throw new Error("Expected error to have stack trace, but it was undefined");
    }

    const stackLines = stackTrace.split('\n').filter(line => line.trim());

    for (let i = 0; i < stackFunctions.length; i++) {
        const [expectedFunction, expectedLocation] = stackFunctions[i];
        const isNativeCode = expectedLocation === "[native code]" 
        const stackLine = stackLines[i];

        let found = false;

        if (isNativeCode) {
            if (stackLine === `${expectedFunction}@[native code]`)
                found = true;
        } else {
            if (stackLine === `${expectedFunction}@${source}:${expectedLocation}`)
                found = true;
            if (stackLine === `${expectedFunction}@${source}`)
                found = true;
        }

        if (!found) {
            throw new Error(
                `Expected stack trace to contain '${expectedFunction}' at '${expectedLocation}', but got '${stackLine}'` +
                `\nActual stack trace:\n${stackTrace}\n`
            );
        }
    }
}

{
    async function foo() {

        nop();

        throw new Error("error from foo");
    }

    for (let i = 0; i < testLoopCount; i++) {
        shouldThrowAsync(
            async function test() {
                await foo();
            }, Error, "error from foo",
            [
                ["foo", "66:24"],
                ["test", "72:26"],
                ["shouldThrowAsync", "8:8"],
                ["global code", "70:25"]
            ],
        );
    }
}

{
    const foo = async () => {

        nop();
        throw new Error("error from foo");
    };

    for (let i = 0; i < testLoopCount; i++) {
        shouldThrowAsync(
            async function test() {
                await foo();
            }, Error, "error from foo",
            [
                ["", "88:24"],
                ["test", "94:26"],
                ["shouldThrowAsync", "8:8"],
                ["global code", "92:25"]
            ],
        );
    }
}

{
    class Foo {
        async foo() {
            nop();

            throw new Error("error from foo");
        }
    }
    const foo = new Foo();

    for (let i = 0; i < testLoopCount; i++) {
        shouldThrowAsync(
            async function test() {
                await foo.foo();
            }, Error, "error from foo",
            [
                ["foo", "111:28"],
                ["test", "119:30"],
                ["shouldThrowAsync", "8:8"],
                ["global code", "117:25"]
            ],
        );
    }
}
