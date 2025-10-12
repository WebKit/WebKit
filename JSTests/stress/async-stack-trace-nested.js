//@ requireOptions("--useAsyncStackTrace=1")

const source = "async-stack-trace-nested.js";

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

async function level1() {

    nop();


    await level2();
}

async function level2() {


    nop();

    await level3();


    nop();
}

async function level3() {

    await level4();


    nop();
}

async function level4() {


    nop();


    await level5();
}

async function level5() {
    nop();



    await level6();
}

async function level6() {

    nop();

    await level7();
}

async function level7() {


    nop();
    await level8();
}

async function level8() {

    await level9();
}

async function level9() {


    await level10();


}

async function level10() {
    await delayedOperation(1);

    await delayedOperation(2);


    await problematicFunction();
}

async function delayedOperation(id) {

    nop();
    await id; // Simple await
}

async function problematicFunction() {


    nop();
    throw new Error("error");
}

for (let i = 0; i < testLoopCount; i++) {
    shouldThrowAsync(
        async function test () {
            await level1();
        }, Error, "error",
        [
            ["problematicFunction", "154:20"],
            ["problematicFunction", "150:36"],
            ["level10", "141:30"],
            ["async level9", "130:18"],
            ["async level8", "124:17"],
            ["async level7", "119:17"],
            ["async level6", "112:17"],
            ["async level5", "105:17"],
            ["async level4", "97:17"],
            ["async level3", "85:17"],
            ["async level2", "77:17"],
            ["async level1", "69:17"],
            ["async test", "160:25"],
            ["drainMicrotasks", "[native code]"],
            ["shouldThrowAsync", "19:20"],
            ["global code", "158:21"]
        ],
    );
    drainMicrotasks();
}

