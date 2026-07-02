// Regression test for https://bugs.webkit.org/show_bug.cgi?id=316443
// NativeExecutable now holds name/length the same way FunctionExecutable does, and the
// internal promise resolving / combinator / finally functions reify their "name" and "length"
// from the NativeExecutable instead of receiving them eagerly at creation time. These functions
// are spec-anonymous built-ins (CreateBuiltinFunction with name ""), so they must expose an
// *own* "name" property whose value is "" (not undefined, not inherited from Function.prototype),
// and an *own* "length" property. This locks that observable shape down.

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`FAIL: expected '${expected}' actual '${actual}'`);
}

function checkAnonymousBuiltin(label, f, expectedLength) {
    shouldBe(typeof f, "function");

    // "name" must be an own property (not merely inherited from Function.prototype.name === "").
    shouldBe(Object.prototype.hasOwnProperty.call(f, "name"), true);
    shouldBe(f.name, "");

    let nameDesc = Object.getOwnPropertyDescriptor(f, "name");
    shouldBe(nameDesc.value, "");
    shouldBe(nameDesc.writable, false);
    shouldBe(nameDesc.enumerable, false);
    shouldBe(nameDesc.configurable, true);

    // "length" must be an own property with the expected value.
    shouldBe(Object.prototype.hasOwnProperty.call(f, "length"), true);
    shouldBe(f.length, expectedLength);

    let lengthDesc = Object.getOwnPropertyDescriptor(f, "length");
    shouldBe(lengthDesc.value, expectedLength);
    shouldBe(lengthDesc.writable, false);
    shouldBe(lengthDesc.enumerable, false);
    shouldBe(lengthDesc.configurable, true);

    if (label) { /* keep label referenced for debugging */ }
}

// Resolve / reject functions handed to a Promise executor (CreateResolvingFunctions).
{
    let resolve, reject;
    new Promise(function (res, rej) {
        resolve = res;
        reject = rej;
    });
    checkAnonymousBuiltin("resolve", resolve, 1);
    checkAnonymousBuiltin("reject", reject, 1);
}

// Promise.withResolvers exposes the same pair of functions.
{
    let { resolve, reject } = Promise.withResolvers();
    checkAnonymousBuiltin("withResolvers.resolve", resolve, 1);
    checkAnonymousBuiltin("withResolvers.reject", reject, 1);
}

// Reading "name" first, then "length" (and vice versa) must both reify correctly and independently.
{
    let resolve, reject;
    new Promise(function (res, rej) { resolve = res; reject = rej; });
    // Force name reification before touching length.
    shouldBe(resolve.name, "");
    shouldBe(Object.prototype.hasOwnProperty.call(resolve, "length"), true);
    shouldBe(resolve.length, 1);

    // Force length reification before touching name on the other function.
    shouldBe(reject.length, 1);
    shouldBe(Object.prototype.hasOwnProperty.call(reject, "name"), true);
    shouldBe(reject.name, "");
}

// The "name" property is configurable, so user code may redefine it; doing so must not
// corrupt the originally-observed value on a freshly created pair.
{
    let resolve;
    new Promise(function (res) { resolve = res; });
    Object.defineProperty(resolve, "name", { value: "mutated", configurable: true });
    shouldBe(resolve.name, "mutated");

    let resolve2;
    new Promise(function (res) { resolve2 = res; });
    checkAnonymousBuiltin("resolve2", resolve2, 1);
}

// Run many times so the test covers all JIT tiers / repeated allocation paths.
for (let i = 0; i < testLoopCount; ++i) {
    let resolve, reject;
    new Promise(function (res, rej) { resolve = res; reject = rej; });
    shouldBe(resolve.name, "");
    shouldBe(reject.name, "");
    shouldBe(resolve.length, 1);
    shouldBe(reject.length, 1);
    shouldBe(Object.prototype.hasOwnProperty.call(resolve, "name"), true);
    shouldBe(Object.prototype.hasOwnProperty.call(reject, "name"), true);
}
