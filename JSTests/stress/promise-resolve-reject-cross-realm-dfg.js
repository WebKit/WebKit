function shouldBe(actual, expected, label) {
    if (actual !== expected)
        throw new Error(`bad value for ${label}: ${actual}, expected ${expected}`);
}

const other = createGlobalObject();

function realmOf(value) {
    const prototype = Object.getPrototypeOf(value);
    if (prototype === Promise.prototype || prototype === TypeError.prototype || prototype === Function.prototype)
        return "main";
    if (prototype === other.Promise.prototype || prototype === other.TypeError.prototype || prototype === other.Function.prototype)
        return "other";
    return "unknown";
}

function nothing() { }

const notConstructor = () => { };
notConstructor.resolve = other.Promise.resolve;
notConstructor.reject = other.Promise.reject;

let executor;
function Capture(newExecutor) {
    executor = newExecutor;
    return new Promise(newExecutor);
}
Capture.resolve = other.Promise.resolve;
Capture.reject = other.Promise.reject;

function callResolve(constructor, value) {
    return constructor.resolve(value);
}
noInline(callResolve);

function callReject(constructor, value) {
    return constructor.reject(value);
}
noInline(callReject);

function realmOfError(callee, constructor) {
    try {
        callee(constructor, 0);
    } catch (error) {
        return realmOf(error);
    }
    return "no error";
}

for (let i = 0; i < testLoopCount; ++i) {
    shouldBe(realmOf(callResolve(other.Promise, i)), "other", "resolve");

    const rejected = callReject(other.Promise, i);
    rejected.catch(nothing);
    shouldBe(realmOf(rejected), "other", "reject");

    if (i % 100 !== 99)
        continue;

    // The executor and the TypeError are created in the realm of the running function.
    callResolve(Capture, i);
    shouldBe(realmOf(executor), "other", "executor of resolve");
    callReject(Capture, i).catch(nothing);
    shouldBe(realmOf(executor), "other", "executor of reject");
    shouldBe(realmOfError(callResolve, notConstructor), "other", "TypeError of resolve");
    shouldBe(realmOfError(callReject, notConstructor), "other", "TypeError of reject");
}
