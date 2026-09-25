function shouldBe(actual, expected, label) {
    if (actual !== expected)
        throw new Error(`bad value for ${label}: ${actual}, expected ${expected}`);
}

const other = createGlobalObject();
const otherThen = other.Promise.prototype.then;

function realmOf(promise) {
    const prototype = Object.getPrototypeOf(promise);
    if (prototype === Promise.prototype)
        return "main";
    if (prototype === other.Promise.prototype)
        return "other";
    return "unknown";
}

function identity(value) { return value; }

function callThen(promise) {
    return promise.then(identity);
}
noInline(callThen);

function callThenWithoutConstructor(promise) {
    return promise.then(identity);
}
noInline(callThenWithoutConstructor);

function callBorrowedThen(promise) {
    return promise.then(identity);
}
noInline(callBorrowedThen);

for (let i = 0; i < testLoopCount; ++i) {
    shouldBe(realmOf(callThen(other.Promise.resolve(i))), "other", "then on other realm promise");

    const withoutConstructor = other.Promise.resolve(i);
    withoutConstructor.constructor = undefined;
    shouldBe(realmOf(callThenWithoutConstructor(withoutConstructor)), "other", "then on other realm promise without constructor");

    const borrowed = Promise.resolve(i);
    borrowed.then = otherThen;
    shouldBe(realmOf(callBorrowedThen(borrowed)), "main", "other realm then on main realm promise");
}
