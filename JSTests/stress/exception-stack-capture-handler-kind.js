function shouldBe(actual, expected, message) {
    if (actual !== expected)
        throw new Error(`${message}: expected ${expected} but got ${actual}`);
}

function thrower(value, shouldThrow) {
    if (shouldThrow)
        throw value;
}
noInline(thrower);

function userCatch(value, shouldThrow) {
    try {
        if (shouldThrow)
            throw value;
    } catch { }
}
noInline(userCatch);

function userCatchAroundCall(value, shouldThrow) {
    try {
        thrower(value, shouldThrow);
    } catch { }
}
noInline(userCatchAroundCall);

function promiseConstructor(value, shouldThrow) {
    return new Promise((resolve) => resolve(thrower(value, shouldThrow)));
}
noInline(promiseConstructor);

function promiseTry(value, shouldThrow) {
    return Promise.try(thrower, value, shouldThrow);
}
noInline(promiseTry);

async function asyncThrow(value, shouldThrow) {
    if (shouldThrow)
        throw value;
}
noInline(asyncThrow);

async function asyncTryFinallyAroundCall(value, shouldThrow) {
    try {
        thrower(value, shouldThrow);
    } finally { }
}
noInline(asyncTryFinallyAroundCall);

function callAsyncInsideUserCatch(asyncFunction, value, shouldThrow) {
    try {
        return asyncFunction(value, shouldThrow);
    } catch {
        throw new Error("async function must not throw synchronously");
    }
}
noInline(callAsyncInsideUserCatch);

// Throwing on every iteration would keep the callers in baseline, since a DFG or FTL catch is an OSR exit.
function shouldThrowAt(i) {
    return !(i % 16);
}

function testUserCatch(name, run) {
    for (let i = 0; i < testLoopCount; ++i) {
        let value = { };
        run(value, shouldThrowAt(i));
        if (shouldThrowAt(i))
            shouldBe($vm.lastExceptionHasCapturedStack(value), false, `${name} #${i}`);
    }
}

function testRejection(name, run) {
    for (let i = 0; i < testLoopCount; ++i) {
        let value = { };
        let promise = run(value, shouldThrowAt(i));
        if (shouldThrowAt(i))
            shouldBe($vm.lastExceptionHasCapturedStack(value), true, `${name} #${i}`);
        promise.catch(() => { });
    }
}

testUserCatch("user catch", userCatch);
testUserCatch("user catch around call", userCatchAroundCall);
testRejection("Promise constructor", promiseConstructor);
testRejection("Promise.try", promiseTry);
testRejection("async throw", (value, shouldThrow) => callAsyncInsideUserCatch(asyncThrow, value, shouldThrow));
testRejection("async try/finally", (value, shouldThrow) => callAsyncInsideUserCatch(asyncTryFinallyAroundCall, value, shouldThrow));
