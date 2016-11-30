function shouldBe(expected, actual, msg = "") {
    if (msg)
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

function shouldThrow(run, errorType, message) {
    let actual;
    var hadError = false;

    try {
        actual = run();
    } catch (e) {
        hadError = true;
        actual = e;
    }

    if (!hadError)
        throw new Error("Expected " + run + "() to throw " + errorType.name + ", but did not throw.");
    if (!(actual instanceof errorType))
        throw new Error("Expeced " + run + "() to throw " + errorType.name + " , but threw '" + actual + "'");
    if (message !== void 0 && actual.message !== message)
        throw new Error("Expected " + run + "() to throw '" + message + "', but threw '" + actual.message + "'");
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

function shouldThrowSyntaxError(str, message) {
    try {
        eval(str);
        throw new Error("Expected `" + str + "` to throw a SyntaxError, but did not throw.")
    } catch (e) {
        if (e.constructor !== SyntaxError)
            throw new Error("Expected `" + str + "` to throw a SyntaxError, but threw '" + e + "'");
        if (message !== void 0 && e.message !== message)
            throw new Error("Expected `" + str + "` to throw SyntaxError: '" + message + "', but threw '" + e + "'");
    }
}

// Do not install `AsyncFunction` constructor on global object
shouldBe(undefined, this.AsyncFunction);
let AsyncFunction = (async function() {}).constructor;

// Let functionPrototype be the intrinsic object %AsyncFunctionPrototype%.
async function asyncFunctionForProto() {}
shouldBe(AsyncFunction.prototype, Object.getPrototypeOf(asyncFunctionForProto));
shouldBe(AsyncFunction.prototype, Object.getPrototypeOf(async function() {}));
shouldBe(AsyncFunction.prototype, Object.getPrototypeOf(async () => {}));
shouldBe(AsyncFunction.prototype, Object.getPrototypeOf({ async method() {} }.method));

// FIXME: AsyncFunction constructor should build functions with correct prototype.
// shouldBe(AsyncFunction.prototype, Object.getPrototypeOf(AsyncFunction()));

// AsyncFunctionCreate does not produce an object with a Prototype
shouldBe(undefined, asyncFunctionForProto.prototype);
shouldBe(false, asyncFunctionForProto.hasOwnProperty("prototype"));
shouldBe(undefined, (async function() {}).prototype);
shouldBe(false, (async function() {}).hasOwnProperty("prototype"));
shouldBe(undefined, (async() => {}).prototype);
shouldBe(false, (async() => {}).hasOwnProperty("prototype"));
shouldBe(undefined, ({ async method() {} }).method.prototype);
shouldBe(false, ({ async method() {} }).method.hasOwnProperty("prototype"));
shouldBe(undefined, AsyncFunction().prototype);
shouldBe(false, AsyncFunction().hasOwnProperty("prototype"));

// AsyncFunction.prototype[ @@toStringTag ]
var descriptor = Object.getOwnPropertyDescriptor(AsyncFunction.prototype, Symbol.toStringTag);
shouldBe("AsyncFunction", descriptor.value);
shouldBe(false, descriptor.enumerable);
shouldBe(false, descriptor.writable);
shouldBe(true, descriptor.configurable);

shouldBe(1, AsyncFunction.length);

// Let F be ! FunctionAllocate(functionPrototype, Strict, "non-constructor")
async function asyncNonConstructorDecl() {}
shouldThrow(() => new asyncNonConstructorDecl(), TypeError);
shouldThrow(() => new (async function() {}), TypeError);
shouldThrow(() => new ({ async nonConstructor() {} }).nonConstructor(), TypeError);
shouldThrow(() => new (() => "not a constructor!"), TypeError);
shouldThrow(() => new (AsyncFunction()), TypeError);

// Normal completion
async function asyncDecl() { return "test"; }
shouldBeAsync("test", asyncDecl);
shouldBeAsync("test2", async function() { return "test2"; });
shouldBeAsync("test3", async () => "test3");
shouldBeAsync("test4", () => ({ async f() { return "test4"; } }).f());

class MyError extends Error {};

// Throw completion
async function asyncDeclThrower(e) { throw new MyError(e); }
shouldThrowAsync(() => asyncDeclThrower("boom!"), MyError, "boom!");
shouldThrowAsync(() => (async function(e) { throw new MyError(e); })("boom!!!"), MyError, "boom!!!");
shouldThrowAsync(() => (async e => { throw new MyError(e) })("boom!!"), MyError, "boom!!");
shouldThrowAsync(() => ({ async thrower(e) { throw new MyError(e); } }).thrower("boom!!!!"), MyError, "boom!!!!");

function resolveLater(value) { return Promise.resolve(value); }
function rejectLater(error) { return Promise.reject(error); }

// Resume after Normal completion
var log = [];
async function resumeAfterNormal(value) {
    log.push("start:" + value);
    value = await resolveLater(value + 1);
    log.push("resume:" + value);
    value = await resolveLater(value + 1);
    log.push("resume:" + value);
    return value + 1;
}

shouldBeAsync(4, () => resumeAfterNormal(1));
shouldBe("start:1 resume:2 resume:3", log.join(" "));

var O = {
    async resumeAfterNormal(value) {
        log.push("start:" + value);
        value = await resolveLater(value + 1);
        log.push("resume:" + value);
        value = await resolveLater(value + 1);
        log.push("resume:" + value);
        return value + 1;
    }
};
log = [];
shouldBeAsync(5, () => O.resumeAfterNormal(2));
shouldBe("start:2 resume:3 resume:4", log.join(" "));

var resumeAfterNormalArrow = async (value) => {
    log.push("start:" + value);
    value = await resolveLater(value + 1);
    log.push("resume:" + value);
    value = await resolveLater(value + 1);
    log.push("resume:" + value);
    return value + 1;
};
log = [];
shouldBeAsync(6, () => resumeAfterNormalArrow(3));
shouldBe("start:3 resume:4 resume:5", log.join(" "));

var resumeAfterNormalEval = AsyncFunction("value", `
    log.push("start:" + value);
    value = await resolveLater(value + 1);
    log.push("resume:" + value);
    value = await resolveLater(value + 1);
    log.push("resume:" + value);
    return value + 1;
`);
log = [];
shouldBeAsync(7, () => resumeAfterNormalEval(4));
shouldBe("start:4 resume:5 resume:6", log.join(" "));

// Resume after Throw completion
async function resumeAfterThrow(value) {
    log.push("start:" + value);
    try {
        value = await rejectLater("throw1");
    } catch (e) {
        log.push("resume:" + e);
    }
    try {
        value = await rejectLater("throw2");
    } catch (e) {
        log.push("resume:" + e);
    }
    return value + 1;
}

log = [];
shouldBeAsync(2, () => resumeAfterThrow(1));
shouldBe("start:1 resume:throw1 resume:throw2", log.join(" "));

var O = {
    async resumeAfterThrow(value) {
        log.push("start:" + value);
        try {
            value = await rejectLater("throw1");
        } catch (e) {
            log.push("resume:" + e);
        }
        try {
            value = await rejectLater("throw2");
        } catch (e) {
            log.push("resume:" + e);
        }
        return value + 1;
    }
}
log = [];
shouldBeAsync(3, () => O.resumeAfterThrow(2));
shouldBe("start:2 resume:throw1 resume:throw2", log.join(" "));

var resumeAfterThrowArrow = async (value) => {
    log.push("start:" + value);
    try {
        value = await rejectLater("throw1");
    } catch (e) {
        log.push("resume:" + e);
    }
    try {
        value = await rejectLater("throw2");
    } catch (e) {
        log.push("resume:" + e);
    }
    return value + 1;
};
log = [];
shouldBeAsync(4, () => resumeAfterThrowArrow(3));
shouldBe("start:3 resume:throw1 resume:throw2", log.join(" "));

var resumeAfterThrowEval = AsyncFunction("value", `
    log.push("start:" + value);
    try {
        value = await rejectLater("throw1");
    } catch (e) {
        log.push("resume:" + e);
    }
    try {
        value = await rejectLater("throw2");
    } catch (e) {
        log.push("resume:" + e);
    }
    return value + 1;
`);
log = [];
shouldBeAsync(5, () => resumeAfterThrowEval(4));
shouldBe("start:4 resume:throw1 resume:throw2", log.join(" "));

// MethoodDefinition SyntaxErrors
shouldThrowSyntaxError("var obj = { async foo : true };", "Unexpected token ':'. Expected a parenthesis for argument list.");
shouldThrowSyntaxError("var obj = { async foo = true };", "Unexpected token '='. Expected a parenthesis for argument list.");
shouldThrowSyntaxError("var obj = { async foo , bar };", "Unexpected token ','. Expected a parenthesis for argument list.");
shouldThrowSyntaxError("var obj = { async foo }", "Unexpected token '}'. Expected a parenthesis for argument list.");

shouldThrowSyntaxError("var obj = { async 0 : true };", "Unexpected token ':'. Expected a parenthesis for argument list.");
shouldThrowSyntaxError("var obj = { async 0 = true };", "Unexpected token '='. Expected a parenthesis for argument list.");
shouldThrowSyntaxError("var obj = { async 0 , bar };", "Unexpected token ','. Expected a parenthesis for argument list.");
shouldThrowSyntaxError("var obj = { async 0 }", "Unexpected token '}'. Expected a parenthesis for argument list.");

shouldThrowSyntaxError("var obj = { async 'foo' : true };", "Unexpected token ':'. Expected a parenthesis for argument list.");
shouldThrowSyntaxError("var obj = { async 'foo' = true };", "Unexpected token '='. Expected a parenthesis for argument list.");
shouldThrowSyntaxError("var obj = { async 'foo' , bar };", "Unexpected token ','. Expected a parenthesis for argument list.");
shouldThrowSyntaxError("var obj = { async 'foo' }", "Unexpected token '}'. Expected a parenthesis for argument list.");

shouldThrowSyntaxError("var obj = { async ['foo'] : true };", "Unexpected token ':'. Expected a parenthesis for argument list.");
shouldThrowSyntaxError("var obj = { async ['foo'] = true };", "Unexpected token '='. Expected a parenthesis for argument list.");
shouldThrowSyntaxError("var obj = { async ['foo'] , bar };", "Unexpected token ','. Expected a parenthesis for argument list.");
shouldThrowSyntaxError("var obj = { async ['foo'] }", "Unexpected token '}'. Expected a parenthesis for argument list.");

shouldThrowSyntaxError("class C { async foo : true };", "Unexpected token ':'. Expected an opening '(' before a async method's parameter list.");
shouldThrowSyntaxError("class C { async foo = true };", "Unexpected token '='. Expected an opening '(' before a async method's parameter list.");
shouldThrowSyntaxError("class C { async foo , bar };", "Unexpected token ','. Expected an opening '(' before a async method's parameter list.");
shouldThrowSyntaxError("class C { async foo }", "Unexpected token '}'. Expected an opening '(' before a async method's parameter list.");

shouldThrowSyntaxError("class C { async 0 : true };", "Unexpected token ':'. Expected an opening '(' before a async method's parameter list.");
shouldThrowSyntaxError("class C { async 0 = true };", "Unexpected token '='. Expected an opening '(' before a async method's parameter list.");
shouldThrowSyntaxError("class C { async 0 , bar };", "Unexpected token ','. Expected an opening '(' before a async method's parameter list.");
shouldThrowSyntaxError("class C { async 0 }", "Unexpected token '}'. Expected an opening '(' before a async method's parameter list.");

shouldThrowSyntaxError("class C { async 'foo' : true };", "Unexpected token ':'. Expected an opening '(' before a async method's parameter list.");
shouldThrowSyntaxError("class C { async 'foo' = true };", "Unexpected token '='. Expected an opening '(' before a async method's parameter list.");
shouldThrowSyntaxError("class C { async 'foo' , bar };", "Unexpected token ','. Expected an opening '(' before a async method's parameter list.");
shouldThrowSyntaxError("class C { async 'foo' }", "Unexpected token '}'. Expected an opening '(' before a async method's parameter list.");

shouldThrowSyntaxError("class C { async ['foo'] : true };", "Unexpected token ':'. Expected an opening '(' before a async method's parameter list.");
shouldThrowSyntaxError("class C { async ['foo'] = true };", "Unexpected token '='. Expected an opening '(' before a async method's parameter list.");
shouldThrowSyntaxError("class C { async ['foo'] , bar };", "Unexpected token ','. Expected an opening '(' before a async method's parameter list.");
shouldThrowSyntaxError("class C { async ['foo'] }", "Unexpected token '}'. Expected an opening '(' before a async method's parameter list.");

// Ensure awaited builtin Promise objects are themselves wrapped in a new Promise,
// per https://tc39.github.io/ecma262/#sec-async-functions-abstract-operations-async-function-await
log = [];
async function awaitedPromisesAreWrapped() {
    log.push("before");
    await Promise.resolve();
    log.push("after");
}
awaitedPromisesAreWrapped();
Promise.resolve().then(() => log.push("Promise.resolve()"));
drainMicrotasks();
shouldBe("before|Promise.resolve()|after", log.join("|"));