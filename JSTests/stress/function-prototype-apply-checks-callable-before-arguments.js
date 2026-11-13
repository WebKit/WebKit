//@ runDefault
//@ runNoFTL
//@ runFTLNoCJIT

function assert(condition) {
    if (!condition)
        throw new Error("Bad assertion");
}

if (typeof testLoopCount === "undefined")
    var testLoopCount = 1000;

const notAFunctionMessage = "|this| is not a function inside Function.prototype.apply";

function arrayLike(log, name) {
    return {
        get length() {
            log.push(name + " length");
            return 1;
        },
        get 0() {
            log.push(name + " element");
            return 1;
        },
    };
}

function expectTypeError(thunk, log) {
    let error;
    try {
        thunk();
    } catch (e) {
        error = e;
    }
    assert(error instanceof TypeError);
    assert(error.message === notAFunctionMessage);
    assert(log.length === 0);
}

function add(a, b) {
    return a + b;
}

function first(value) {
    return value;
}

function arity() {
    return arguments.length;
}

let callTarget = {};
let fastTarget = { apply: Function.prototype.apply };
let spreadTarget = { apply: Function.prototype.apply };
let inheritedTarget = Object.create(Function.prototype);

for (let i = 0; i < testLoopCount; i++) {
    assert(add.apply(null, [i, 1]) === i + 1);
    assert(add.apply(null, { length: 2, 0: i, 1: 2 }) === i + 2);
    assert(arity.apply(null, null) === 0);
    assert(arity.apply(null, undefined) === 0);
    assert(Function.prototype.apply.call(add, null, [i, 3]) === i + 3);

    let log = [];
    expectTypeError(() => Function.prototype.apply.call(callTarget, null, arrayLike(log, "call")), log);
    expectTypeError(() => Function.prototype.apply.call(null, null, arrayLike(log, "null")), log);
    expectTypeError(() => Function.prototype.apply.call(undefined, null, arrayLike(log, "undefined")), log);
    expectTypeError(() => fastTarget.apply(null, arrayLike(log, "fast")), log);
    expectTypeError(() => inheritedTarget.apply(null, arrayLike(log, "inherited")), log);
    expectTypeError(() => spreadTarget.apply(...[null, arrayLike(log, "spread")]), log);
    expectTypeError(() => fastTarget.apply(null, 1), log);

    log = [];
    let thisArgRan = false;
    let argArrayRan = false;
    try {
        Function.prototype.apply.call({}, (thisArgRan = true, null), (argArrayRan = true, arrayLike(log, "order")));
    } catch (e) {
        assert(e instanceof TypeError);
        assert(e.message === notAFunctionMessage);
    }
    assert(thisArgRan);
    assert(argArrayRan);
    assert(log.length === 0);

    let seen = 0;
    assert(first.apply(null, { get length() { seen++; return 1; }, 0: 5 }) === 5);
    assert(seen === 1);

    let custom = {
        apply(thisArg, args) {
            return args;
        },
    };
    let customArgs = arrayLike(log, "custom");
    assert(custom.apply(null, customArgs) === customArgs);

    let reflectLog = [];
    let reflectError;
    try {
        Reflect.apply({}, null, arrayLike(reflectLog, "reflect"));
    } catch (e) {
        reflectError = e;
    }
    assert(reflectError instanceof TypeError);
    assert(reflectError.message === "Reflect.apply requires the first argument be a function");
    assert(reflectLog.length === 0);
}
