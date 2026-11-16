function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + ' expected ' + expected);
}

// When the separator is an object, operationArrayJoinGeneric must not call
// ToString itself: doing so would let the side effect run, and a subsequent
// revalidation failure (e.g. mutated array length / indexing type) would
// trigger an OSR exit, after which the bytecode-level slow path would call
// ToString again. Verify ToString is invoked exactly once per join() call
// across all tiers.

function join(arr, sep) {
    return arr.join(sep);
}
noInline(join);

// Plain object separator: counter should equal the number of calls.
{
    var arr = [1, 2, 3];
    var calls = 0;
    var sep = { toString() { ++calls; return ","; } };
    for (var i = 0; i < testLoopCount; ++i)
        shouldBe(join(arr, sep), "1,2,3");
    shouldBe(calls, testLoopCount);
}

// Object separator whose ToString shrinks the array. Even when the mutation
// invalidates the fast path, ToString must run exactly once per join().
{
    var calls = 0;
    function call() {
        var arr = [1, 2, 3];
        var sep = { toString() { ++calls; arr.length = 1; return ","; } };
        return arr.join(sep);
    }
    noInline(call);
    for (var i = 0; i < testLoopCount; ++i)
        shouldBe(call(), "1,,");
    shouldBe(calls, testLoopCount);
}

// Object separator whose ToString transitions the array's indexing type
// (Int32 -> Contiguous). ToString must still run exactly once per call.
{
    var calls = 0;
    function call() {
        var arr = [1, 2, 3];
        var sep = { toString() { ++calls; arr[1] = "X"; return ","; } };
        return arr.join(sep);
    }
    noInline(call);
    for (var i = 0; i < testLoopCount; ++i)
        shouldBe(call(), "1,X,3");
    shouldBe(calls, testLoopCount);
}

// Object separator with empty-string ToString that punches a hole. The
// empty-separator fast path would otherwise read the hole; ToString still
// runs exactly once per call.
{
    var calls = 0;
    function call() {
        var arr = [1, 2, 3];
        var sep = { toString() { ++calls; delete arr[1]; return ""; } };
        return arr.join(sep);
    }
    noInline(call);
    for (var i = 0; i < testLoopCount; ++i)
        shouldBe(call(), "13");
    shouldBe(calls, testLoopCount);
}

// Object separator with empty-string ToString that shrinks the array. The
// empty-separator fast path would otherwise read past the new length;
// ToString still runs exactly once per call.
{
    var calls = 0;
    function call() {
        var arr = [1, 2, 3];
        var sep = { toString() { ++calls; arr.length = 1; return ""; } };
        return arr.join(sep);
    }
    noInline(call);
    for (var i = 0; i < testLoopCount; ++i)
        shouldBe(call(), "1");
    shouldBe(calls, testLoopCount);
}
