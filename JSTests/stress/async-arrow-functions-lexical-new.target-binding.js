function shouldBe(expected, actual, msg) {
    if (msg === void 0)
        msg = "";
    else
        msg = " for " + msg;
    if (actual !== expected)
        throw new Error("bad value" + msg + ": " + actual + ". Expected " + expected);
}

function shouldBeAsync(expected, run, msg) {
    let actual;
    var hadError = false;
    run().then(function(value) { actual = value; },
               function(error) { hadError = true; actual = error; });
    drainMicrotasks();

    if (hadError)
        throw actual;

    shouldBe(expected, actual, msg);
}

function shouldThrowAsync(run, errorType, message) {
    let actual;
    var hadError = false;
    run().then(function(value) { actual = value; },
               function(error) { hadError = true; actual = error; });
    drainMicrotasks();

    if (!hadError)
        throw new Error("Expected " + run + "() to throw " + errorType.name + ", but did not throw.");
    if (!(actual instanceof errorType))
        throw new Error("Expected " + run + "() to throw " + errorType.name + ", but threw '" + actual + "'");
    if (message !== void 0 && actual.message !== message)
        throw new Error("Expected " + run + "() to throw '" + message + "', but threw '" + actual.message + "'");
}

function C1() {
    return async () => await new.target;
}

function C2() {
    return async () => { return await new.target };
}

function C2WithAwait() {
    return async () => { 
        var self = new.target; await new.target;
        return new.target;
    }
}

shouldBeAsync(C1, new C1());
shouldBeAsync(undefined, C1());
shouldBeAsync(C2, new C2());
shouldBeAsync(undefined, C2());
shouldBeAsync(C2WithAwait, new C2WithAwait());
shouldBeAsync(undefined, C2WithAwait());

shouldThrowAsync(async () => await new.target, ReferenceError);
shouldThrowAsync(async () => { return await new.target; }, ReferenceError);
