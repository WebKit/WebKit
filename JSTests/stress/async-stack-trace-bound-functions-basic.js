//@ requireOptions("--useAsyncStackTrace=1")

const source = "async-stack-trace-bound-functions-basic.js";

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

    let stackLineIndex = 0;
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
            else if (stackLine === `${expectedFunction}@${source}`)
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
    async function foo(x) {

        nop();

        nop();

        await bar.bind(this)(x);
    }

    async function bar(x) {


        nop();


        await baz.bind(this)(x);
    }

    async function baz(x) {
        await x;

        nop();
        throw new Error("error from baz");
    }

    for (let i = 0; i < testLoopCount; i++) {
        shouldThrowAsync(
            async function test() {
                await foo(1);
            }, Error, "error from baz",
            [
                ["baz", "87:24"],
                ["async bar", "80:29"],
                ["async foo", "71:29"],
                ["async test", "93:26"],
                ["drainMicrotasks", "[native code]"],
                ["shouldThrowAsync", "19:20"],
                ["global code", "91:25"]
            ],
        );
        drainMicrotasks();
    }
}

