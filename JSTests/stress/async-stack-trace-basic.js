//@ requireOptions("--useAsyncStackTrace=1")

const source = "async-stack-trace-basic.js";

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
    async function one(x) {
        nop();
        nop();

        await two(x);
    }

    async function two(x) {
        await x;
        nop();


        nop();
        throw new Error("error");
    }

    for (let i = 0; i < testLoopCount; i++) {
        shouldThrowAsync(
            async function test() {
                await one(1);
            }, Error, "error",
            [
                ["two", "78:24"],
                ["async one", "69:18"],
                ["async test", "84:26"],
                ["drainMicrotasks", "[native code]"],
                ["shouldThrowAsync", "19:20"],
                ["global code", "82:25"]
            ],
        );
        drainMicrotasks();
    }
}

{
    async function one(x) {

        nop();


        return await two(x);
    }

    async function two(x) {

        await x;


        nop();
        return +x; // This will raise a TypeError.
    }

    for (let i = 0; i < testLoopCount; i++) {
        shouldThrowAsync(
            async function test () {
                await one(Symbol());
            }, TypeError, "Cannot convert a symbol to a number",
            [
                ["two", "114:16"],
                ["async one", "105:25"],
                ["async test", "120:26"],
                ["drainMicrotasks", "[native code]"],
                ["shouldThrowAsync", "19:20"],
                ["global code", "118:25"]
            ],
        );
        drainMicrotasks();
    }
}

{
    function callOne(x) {


        nop();
        return one(x);
    }

    function callTwo(x) {


        nop();


        return two(x);
    }

    async function one(x) {


        nop();
        return await callTwo(x);
    }

    async function two(x) {
        await x;
        throw new Error("error");
    }

    for (let i = 0; i < testLoopCount; i++) {
        shouldThrowAsync(
            async function test () {
                await callOne(1);
            }, Error, "error",
            [
                ["two", "161:24"],
                ["async one", "156:29"],
                ["async test", "167:30"],
                ["drainMicrotasks", "[native code]"],
                ["shouldThrowAsync", "19:20"],
                ["global code", "165:25"]
            ],
        );
        drainMicrotasks();
    }
}

{
    async function one(x) {
        nop();
        nop();

        await two(x);
    }

    async function two(x) {
        await x;
        nop();


        nop();
        await throwError();
    }

    async function throwError() {
        throw new Error("error");
    }

    for (let i = 0; i < testLoopCount; i++) {
        shouldThrowAsync(
            async function test() {
                await one(1);
            }, Error, "error",
            [
                ["throwError", "200:24"],
                ["throwError", "199:35"],
                ["two", "196:25"],
                ["async one", "187:18"],
                ["async test", "206:26"],
                ["drainMicrotasks", "[native code]"],
                ["shouldThrowAsync", "19:20"],
                ["global code", "204:25"]
            ],
        );
        drainMicrotasks();
    }
}
