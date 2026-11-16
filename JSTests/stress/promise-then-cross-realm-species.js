function shouldBe(actual, expected, label) {
    if (actual !== expected)
        throw new Error(`bad value for ${label}: ${actual}, expected ${expected}`);
}

const other = createGlobalObject();

function realmOf(promise) {
    const prototype = Object.getPrototypeOf(promise);
    if (prototype === Promise.prototype)
        return "main";
    if (prototype === other.Promise.prototype)
        return "other";
    return "unknown";
}

function withoutConstructor(promise) {
    promise.constructor = undefined;
    return promise;
}

function withOwnProperty(promise) {
    promise.unrelated = 1;
    return promise;
}

const mainThen = Promise.prototype.then;
const mainCatch = Promise.prototype.catch;
const mainFinally = Promise.prototype.finally;
const otherThen = other.Promise.prototype.then;
const otherCatch = other.Promise.prototype.catch;
const otherFinally = other.Promise.prototype.finally;

function identity(value) { return value; }
function nothing() { }

// SpeciesConstructor reads promise.constructor[@@species], which is the %Promise% of the promise's realm.
shouldBe(realmOf(mainThen.call(other.Promise.resolve(1), identity)), "other", "then on other realm promise");
shouldBe(realmOf(mainCatch.call(other.Promise.resolve(1), identity)), "other", "catch on other realm promise");
shouldBe(realmOf(mainFinally.call(other.Promise.resolve(1), nothing)), "other", "finally on other realm promise");
shouldBe(realmOf(mainFinally.call(other.Promise.resolve(1), 1)), "other", "finally with non callable on other realm promise");
shouldBe(realmOf(mainThen.call(withOwnProperty(other.Promise.resolve(1)), identity)), "other", "then on other realm promise with own property");

shouldBe(realmOf(otherThen.call(Promise.resolve(1), identity)), "main", "other realm then on main realm promise");
shouldBe(realmOf(otherCatch.call(Promise.resolve(1), identity)), "main", "other realm catch on main realm promise");
shouldBe(realmOf(otherFinally.call(Promise.resolve(1), nothing)), "main", "other realm finally on main realm promise");
shouldBe(realmOf(otherFinally.call(Promise.resolve(1), 1)), "main", "other realm finally with non callable on main realm promise");
shouldBe(realmOf(otherThen.call(withOwnProperty(Promise.resolve(1)), identity)), "main", "other realm then on main realm promise with own property");

// When promise.constructor is undefined, the default constructor is the %Promise% of the realm of the running `then`.
// catch and finally invoke promise.then, which is the `then` of the promise's realm.
shouldBe(realmOf(mainThen.call(withoutConstructor(other.Promise.resolve(1)), identity)), "main", "then without constructor");
shouldBe(realmOf(mainCatch.call(withoutConstructor(other.Promise.resolve(1)), identity)), "other", "catch without constructor");
shouldBe(realmOf(mainFinally.call(withoutConstructor(other.Promise.resolve(1)), nothing)), "other", "finally without constructor");
shouldBe(realmOf(mainFinally.call(withoutConstructor(other.Promise.resolve(1)), 1)), "other", "finally with non callable without constructor");

shouldBe(realmOf(mainThen.call(Promise.resolve(1), identity)), "main", "then on main realm promise");
shouldBe(realmOf(mainCatch.call(Promise.resolve(1), identity)), "main", "catch on main realm promise");
shouldBe(realmOf(mainFinally.call(Promise.resolve(1), nothing)), "main", "finally on main realm promise");
shouldBe(realmOf(other.Promise.resolve(1).then(identity)), "other", "method call on other realm promise");

// An unhandled rejection is reported to the realm of the derived promise.
const reported = [];
setUnhandledRejectionCallback(function () { reported.push("main"); });
other.setUnhandledRejectionCallback(function () { reported.push("other"); });
mainThen.call(other.Promise.resolve(1), function () { throw new Error("rejected"); });
drainMicrotasks();
shouldBe(reported.join(), "other", "realm the unhandled rejection is reported to");
