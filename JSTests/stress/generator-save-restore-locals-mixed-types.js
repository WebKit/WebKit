function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${String(actual)}, expected ${String(expected)}`);
}

var symbol = Symbol("s");
function* mixed() {
    let int = 1, double = 2.5, string = "s", object = { v: 3 }, undef, nul = null, bool = true, big = 10n, sym = symbol, array = [1, 2];
    try {
        int += yield int;
        double += yield double;
        undef = yield string + object.v;
        big += yield nul;
        bool = !(yield bool);
        array.push(yield sym);
    } catch (error) {
        return [int, double, string, object.v, undef, nul, bool, big, sym, array.length, error];
    }
    return [int, double, string, object.v, undef, nul, bool, big, sym, array.length, "done"];
}

function check(result, error) {
    shouldBe(result[0], 11);
    shouldBe(result[1], 3);
    shouldBe(result[2], "s");
    shouldBe(result[3], 3);
    shouldBe(result[4], undefined);
    shouldBe(result[5], null);
    shouldBe(result[6], true);
    shouldBe(result[7], 15n);
    shouldBe(result[8], symbol);
    shouldBe(result[9], error === "boom" ? 2 : 3);
    shouldBe(result[10], error);
}

for (let i = 0; i < testLoopCount; ++i) {
    let iterator = mixed();
    shouldBe(iterator.next().value, 1);
    shouldBe(iterator.next(10).value, 2.5);
    shouldBe(iterator.next(0.5).value, "s3");
    shouldBe(iterator.next(undefined).value, null);
    shouldBe(iterator.next(5n).value, true);
    shouldBe(iterator.next(false).value, symbol);
    let last = i & 1 ? iterator.throw("boom") : iterator.next(7);
    shouldBe(last.done, true);
    check(last.value, i & 1 ? "boom" : "done");
}
