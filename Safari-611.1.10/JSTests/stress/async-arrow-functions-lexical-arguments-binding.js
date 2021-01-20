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

var noArgumentsArrow = async () => await [...arguments];
shouldThrowAsync(() => noArgumentsArrow(1, 2, 3), ReferenceError);
var noArgumentsArrow2 = async () => { return await [...arguments]; }
shouldThrowAsync(() => noArgumentsArrow2(1, 2, 3), ReferenceError);

shouldBeAsync("[1,2,3]", () => (function() { return (async () => JSON.stringify([...arguments]))(); })(1, 2, 3));
shouldBeAsync("[4,5,6]", () => (function() { return (async () => { return JSON.stringify([...await arguments]) })(); })(4, 5, 6));

(function testArgumentsBinding() {
    var argsBinding;
    var promise = (function() { argsBinding = arguments; return (async() => arguments)() })(1, 2, 3);
    shouldBeAsync(argsBinding, () => promise);
})();
