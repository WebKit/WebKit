function shouldThrow(fn, error, message, stackFunctions) {
    try {
        fn();
        throw new Error('Expected to throw, but succeeded');
    } catch (e) {
        if (!(e instanceof error))
            throw new Error(`Expected to throw ${error.name} but got ${e.name}`);
        if (e.message !== message)
            throw new Error(`Expected ${error.name} with '${message}' but got '${e.message}'`);

        const stackLines = e.stack.split('\n').filter(line => line.trim());
        if (stackLines.length !== stackFunctions.length) {
            throw new Error(
                `\nActual stack trace:\n${e.stack}\n`
            );
        }
        for (let i = 0; i < stackFunctions.length; i++) {
            const expectedFunction = stackFunctions[i];
            const stackLine = stackLines[i];
            let found = false;
            if (stackLine.startsWith(`${expectedFunction}@`))
                found = true;
            if (!found) {
                throw new Error(`Actual stack trace:\n${e.stack}`);
            }
        }
    }
}

{
    class MyError extends Error {}
    function throwMyError () {
        throw new MyError("my error");
    }
    for (let i = 0; i < testLoopCount; i++) {
        shouldThrow(() => {
            throwMyError();
        }, MyError, "my error", [
            "throwMyError",
            "",
            "shouldThrow",
            "global code"
        ]);
    }
}

{
    class MyError1 extends Error {}
    class MyError2 extends MyError1 {}
    function throwMyError () {
        throw new MyError2("my error 2");
    }
    for (let i = 0; i < testLoopCount; i++) {
        shouldThrow(() => {
            throwMyError();
        }, MyError2, "my error 2", [
            "throwMyError",
            "",
            "shouldThrow",
            "global code"
        ]);
    }
}

{
    class MyError extends Error {
        constructor(...args) { super(...args); }
    }
    function throwMyError () {
        throw new MyError("my error");
    }
    for (let i = 0; i < testLoopCount; i++) {
        shouldThrow(() => {
            throwMyError();
        }, MyError, "my error", [
            "throwMyError",
            "",
            "shouldThrow",
            "global code"
        ]);
    }
}

{
    class MyError1 extends Error {
        constructor(...args) { super(...args); }
    }
    class MyError2 extends MyError1 {
        constructor(...args) { super(...args); }
    }
    function throwMyError () {
        throw new MyError2("my error 2");
    }
    for (let i = 0; i < testLoopCount; i++) {
        shouldThrow(() => {
            throwMyError();
        }, MyError2, "my error 2", [
            "throwMyError",
            "",
            "shouldThrow",
            "global code"
        ]);
    }
}
