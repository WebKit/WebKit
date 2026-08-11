// These should all succeed.
var bi64a = new BigInt64Array(new SharedArrayBuffer(128));
var bu64a = new BigUint64Array(new SharedArrayBuffer(128));

function shouldFail(f, name)
{
    try {
        f();
    } catch (e) {
        if (e.name == name.name)
            return;
        throw new Error(f + " threw the wrong error: " + e);
    }
    throw new Error(f + " succeeded!");
}

function shouldSucceed(f)
{
    f();
}

for (bad of [bu64a]) {
    shouldFail(() => Atomics.notify(bad, 0, 0n), TypeError);
    shouldFail(() => Atomics.wait(bad, 0, 0n), TypeError);
    shouldFail(() => Atomics.waitAsync(bad, 0, 0n), TypeError);
    shouldFail(() => waiterListSize(bad, 0), TypeError);
}

for (idx of [-1, -1000000000000, 10000, 10000000000000]) {
    for (a of [bi64a, bu64a]) {
        shouldFail(() => Atomics.add(a, idx, 0n), RangeError);
        shouldFail(() => Atomics.and(a, idx, 0n), RangeError);
        shouldFail(() => Atomics.compareExchange(a, idx, 0n, 0n), RangeError);
        shouldFail(() => Atomics.exchange(a, idx, 0n), RangeError);
        shouldFail(() => Atomics.load(a, idx), RangeError);
        shouldFail(() => Atomics.or(a, idx, 0n), RangeError);
        shouldFail(() => Atomics.store(a, idx, 0n), RangeError);
        shouldFail(() => Atomics.sub(a, idx, 0n), RangeError);
        shouldFail(() => Atomics.xor(a, idx, 0n), RangeError);
    }
    shouldFail(() => Atomics.notify(bi64a, idx, 0n), RangeError);
    shouldFail(() => Atomics.wait(bi64a, idx, 0n), RangeError);
    shouldFail(() => Atomics.waitAsync(bi64a, idx, 0n), RangeError);
    shouldFail(() => waiterListSize(bi64a, idx), RangeError);
}

for (idx of ["hello"]) {
    for (a of [bi64a, bu64a]) {
        shouldSucceed(() => Atomics.add(a, idx, 0n));
        shouldSucceed(() => Atomics.and(a, idx, 0n));
        shouldSucceed(() => Atomics.compareExchange(a, idx, 0n, 0n));
        shouldSucceed(() => Atomics.exchange(a, idx, 0n));
        shouldSucceed(() => Atomics.load(a, idx));
        shouldSucceed(() => Atomics.or(a, idx, 0n));
        shouldSucceed(() => Atomics.store(a, idx, 0n));
        shouldSucceed(() => Atomics.sub(a, idx, 0n));
        shouldSucceed(() => Atomics.xor(a, idx, 0n));
    }
    shouldSucceed(() => Atomics.notify(bi64a, idx, 0));
    shouldSucceed(() => Atomics.wait(bi64a, idx, 0n, 1));
    shouldSucceed(() => Atomics.waitAsync(bi64a, idx, 0n, 1));
}

function runAtomic(array, index, init, name, args, expectedResult, expectedOutcome)
{
    array[index] = init;
    var result = Atomics[name](array, index, ...args);
    if (result != expectedResult)
        throw new Error("Expected Atomics." + name + "(array, " + index + ", " + args.join(", ") + ") to return " + expectedResult + " but returned " + result + " for " + Object.prototype.toString.apply(array));
    if (array[index] !== expectedOutcome)
        throw new Error("Expected Atomics." + name + "(array, " + index + ", " + args.join(", ") + ") to result in array[" + index + "] = " + expectedOutcome + " but got " + array[index] + " for " + Object.prototype.toString.apply(array));
}

for (a of [bi64a, bu64a]) {
    runAtomic(a, 0, 13n, "add", [42n], 13n, 55n);
    runAtomic(a, 0, 13n, "and", [42n], 13n, 8n);
    runAtomic(a, 0, 13n, "compareExchange", [25n, 42n], 13n, 13n);
    runAtomic(a, 0, 13n, "compareExchange", [13n, 42n], 13n, 42n);
    runAtomic(a, 0, 13n, "exchange", [42n], 13n, 42n);
    runAtomic(a, 0, 13n, "load", [], 13n, 13n);
    runAtomic(a, 0, 13n, "or", [42n], 13n, 47n);
    runAtomic(a, 0, 13n, "store", [42n], 42n, 42n);
    runAtomic(a, 0, 42n, "sub", [13n], 42n, 29n);
    runAtomic(a, 0, 13n, "xor", [42n], 13n, 39n);
}

bi64a[0] = 0n;
var result = Atomics.wait(bi64a, 0, 1n);
if (result != "not-equal")
    throw "Error: bad result from Atomics.wait: " + result;

var result = Atomics.waitAsync(bi64a, 0, 1n);
if (result.value != "not-equal")
    throw "Error: bad result from Atomics.waitAsync: " + result;

for (timeout of [0, 1, 10]) {
    var result = Atomics.wait(bi64a, 0, 0n, timeout);
    if (result != "timed-out")
        throw "Error: bad result from Atomics.wait: " + result;
}

var result = Atomics.waitAsync(bi64a, 0, 0n, 0);
if (result.value != "timed-out")
    throw "Error: bad result from Atomics.waitAsync: " + result;
