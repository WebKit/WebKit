function assert(condition, message) {
    if (!condition)
        throw new Error(message);
}

function churn(depth) {
    if (depth <= 0)
        return 0;
    let sum = 0;
    const objects = [];
    for (let i = 0; i < 200; ++i)
        objects.push({ a: i, b: [i] });
    for (let i = 0; i < objects.length; ++i)
        sum += objects[i].a;
    return sum + churn(depth - 1);
}

function collect(allocate) {
    let allocated = allocate();
    for (let round = 0; round < 3; ++round) {
        churn(3);
        $.clearKeptObjects();
        $.gc();
    }
    return allocated;
}

let unretained = collect(function() {
    function make(payload) {
        const host = {};
        const worker = () => host;
        void arguments[0];
        return worker;
    }
    let payload = {};
    let ref = new WeakRef(payload);
    let worker = make(payload);
    payload = null;
    return { ref, worker };
});
assert(unretained.ref.deref() === undefined, "a closure over another binding kept the arguments object alive");
assert(typeof unretained.worker() === "object", "the other binding is still reachable");

let retained = collect(function() {
    function make(payload) {
        return () => arguments[0];
    }
    let payload = { kept: true };
    let ref = new WeakRef(payload);
    let arrow = make(payload);
    payload = null;
    return { ref, arrow };
});
assert(retained.ref.deref() === retained.arrow(), "an arrow that reads arguments must keep that object alive");

function sloppyLocal(value) {
    const unused = () => 1;
    return arguments[0];
}
assert(sloppyLocal(7) === 7, "sloppy arguments[0]");

function sloppyAlias(value) {
    const unused = () => 1;
    arguments[0] = 5;
    return value;
}
assert(sloppyAlias(1) === 5, "sloppy arguments still aliases an uncaptured parameter");

function sloppyCapturedParameter(value) {
    const read = () => value;
    arguments[0] = 5;
    return [value, read()];
}
let captured = sloppyCapturedParameter(1);
assert(captured[0] === 5 && captured[1] === 5, "scoped arguments still aliases a captured parameter");

function strictLocal(value) {
    "use strict";
    const unused = () => 1;
    return arguments[0];
}
assert(strictLocal(7) === 7, "strict arguments[0]");

function strictAssign(value) {
    "use strict";
    const unused = () => 1;
    arguments[0] = 5;
    return value;
}
assert(strictAssign(1) === 1, "strict arguments does not alias the parameter");

function withShadow(value) {
    const unused = () => 1;
    with ({ arguments: [42] })
        return arguments[0];
}
assert(withShadow(1) === 42, "with must still shadow arguments");

function evalReads(value) {
    const unused = () => 1;
    return eval("arguments[0]");
}
assert(evalReads(8) === 8, "direct eval must still see arguments");

function defaults(value = 1) {
    const unused = () => 2;
    arguments[0] = 9;
    return [value, arguments[0]];
}
let defaulted = defaults(3);
assert(defaulted[0] === 3 && defaulted[1] === 9, "non-simple arguments stays unmapped");

function nestedOwnArguments(value) {
    const unused = () => 1;
    function inner() { return arguments[0]; }
    return [arguments[0], inner(2)];
}
let nested = nestedOwnArguments(4);
assert(nested[0] === 4 && nested[1] === 2, "a nested function has its own arguments");

function* generatorNoParams() {
    const unused = () => 1;
    yield arguments[0];
}
assert(generatorNoParams(4).next().value === 4, "generator with no parameters");

function* generatorWithParam(value) {
    const unused = () => 1;
    yield arguments[0];
}
assert(generatorWithParam(5).next().value === 5, "generator with a parameter");

async function asyncNoParams() {
    const unused = () => 1;
    await 1;
    return arguments[0];
}
let asyncValue;
let asyncError;
asyncNoParams(6).then(value => { asyncValue = value; }, error => { asyncError = error; });
drainMicrotasks();
if (asyncError)
    throw asyncError;
assert(asyncValue === 6, "async function with no parameters");

async function* asyncGeneratorNoParams() {
    const unused = () => 1;
    yield arguments[0];
}
let asyncGeneratorValue;
let asyncGeneratorError;
asyncGeneratorNoParams(7).next().then(result => { asyncGeneratorValue = result.value; }, error => { asyncGeneratorError = error; });
drainMicrotasks();
if (asyncGeneratorError)
    throw asyncGeneratorError;
assert(asyncGeneratorValue === 7, "async generator with no parameters");

async function asyncNoAwait() {
    const unused = () => 1;
    return arguments[0];
}
let noAwaitValue;
asyncNoAwait(8).then(value => { noAwaitValue = value; });
drainMicrotasks();
assert(noAwaitValue === 8, "async function without await");

function restArguments(value, ...rest) {
    const unused = () => 1;
    return [arguments[0], arguments[1], rest[0]];
}
let rested = restArguments(1, 2);
assert(rested[0] === 1 && rested[1] === 2 && rested[2] === 2, "rest arguments");
