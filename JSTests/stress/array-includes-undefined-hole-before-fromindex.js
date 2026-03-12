function shouldBe(actual, expected, msg) {
    if (actual !== expected)
        throw new Error("FAIL: " + msg + " | got " + actual + ", expected " + expected);
}

{
    function test(arr, search, from) {
        return arr.includes(search, from);
    }
    noInline(test);

    let warmup = [0, 1, 2];
    for (let i = 0; i < testLoopCount; ++i)
        shouldBe(test(warmup, 99, 0), false, "warmup");

    let arr = [, 1, 2];
    shouldBe(test(arr, undefined, 1), false, "Int32: [,1,2].includes(undefined, 1)");
    shouldBe(test(arr, undefined, 2), false, "Int32: [,1,2].includes(undefined, 2)");
    shouldBe(test(arr, undefined, 3), false, "Int32: [,1,2].includes(undefined, 3)");
    shouldBe(test(arr, undefined, 0), true, "Int32: [,1,2].includes(undefined, 0)");

    let arr2 = [1, , 3];
    shouldBe(test(arr2, undefined, 0), true,  "Int32: [1,,3].includes(undefined, 0)");
    shouldBe(test(arr2, undefined, 1), true,  "Int32: [1,,3].includes(undefined, 1)");
    shouldBe(test(arr2, undefined, 2), false, "Int32: [1,,3].includes(undefined, 2)");
}

{
    function test(arr, search, from) {
        return arr.includes(search, from);
    }
    noInline(test);

    let warmup = [0.5, 1.5, 2.5];
    for (let i = 0; i < testLoopCount; ++i)
        shouldBe(test(warmup, 99.9, 0), false, "warmup");

    let arr = [, 1.5, 2.5];
    shouldBe(test(arr, undefined, 1), false, "Double: [,1.5,2.5].includes(undefined, 1)");
    shouldBe(test(arr, undefined, 2), false, "Double: [,1.5,2.5].includes(undefined, 2)");
    shouldBe(test(arr, undefined, 0), true,  "Double: [,1.5,2.5].includes(undefined, 0)");

    let arr2 = [1.5, , 3.5];
    shouldBe(test(arr2, undefined, 2), false, "Double: [1.5,,3.5].includes(undefined, 2)");
}

{
    function test(arr, search, from) {
        return arr.includes(search, from);
    }
    noInline(test);

    let o1 = {};
    let o2 = {};
    let warmup = [o1, o2, o1];
    for (let i = 0; i < testLoopCount; ++i) {
        shouldBe(test(warmup, undefined, 0), false, "warmup");
        shouldBe(test(warmup, null, 0), false, "warmup");
    }

    let arr = [, o1, o2];
    shouldBe(test(arr, undefined, 1), false, "Contiguous: [,o1,o2].includes(undefined, 1)");
    shouldBe(test(arr, undefined, 2), false, "Contiguous: [,o1,o2].includes(undefined, 2)");
    shouldBe(test(arr, undefined, 0), true,  "Contiguous: [,o1,o2].includes(undefined, 0)");
}

{
    shouldBe(eval("[, 1, 2].includes(undefined, 1)"), false, "LLInt Int32");
    shouldBe(eval("[, 1.5, 2.5].includes(undefined, 1)"), false, "LLInt Double");
    shouldBe(eval("[1, , 3].includes(undefined, 2)"), false, "LLInt Int32 middle");
    shouldBe(eval("[1.5, , 3.5].includes(undefined, 2)"), false, "LLInt Double middle");
}

{
    shouldBe([, 1, 2].includes(undefined, -1), false, "Int32 negative fromIndex");
    shouldBe([, 1, 2].includes(undefined, -2), false, "Int32 negative fromIndex -2");
    shouldBe([, 1, 2].includes(undefined, -3), true,  "Int32 negative fromIndex -3 (covers hole)");
    shouldBe([, 1.5, 2.5].includes(undefined, -1), false, "Double negative fromIndex");
}
