function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(
            "bad value: actual=" + actual + ", expected=" + expected,
        );
}

{
    let map = new Map([
        [1, "a"],
        [2, "b"],
        [3, "c"],
    ]);
    let result = [];

    for (let item of map.entries().flatMap(([k, v]) => [[k, v]])) {
        result.push(item);
        if (result.length === 2) {
            break;
        }
    }

    shouldBe(result.length, 2);
}

{
    let getReturnCount = 0;
    let callReturnCount = 0;

    let baseIterator = {
        count: 0,
        next() {
            if (this.count < 3) {
                return { done: false, value: ++this.count };
            }
            return { done: true };
        },
        get return() {
            getReturnCount++;
            return function () {
                callReturnCount++;
                return { done: true };
            };
        },
    };

    let iter = {
        [Symbol.iterator]() {
            return baseIterator;
        },
    };

    let result1 = [];
    for (let item of Iterator.from(iter).flatMap((x) => [x])) {
        result1.push(item);
    }
    shouldBe(result1.length, 3);
    shouldBe(getReturnCount, 0);

    baseIterator.count = 0;
    getReturnCount = 0;
    callReturnCount = 0;

    let result2 = [];
    for (let item of Iterator.from(iter).flatMap((x) => [x])) {
        result2.push(item);
        if (result2.length === 1) {
            break;
        }
    }
    shouldBe(result2.length, 1);
}

{
    let set = new Set(["a", "b", "c"]);
    let result = [];

    for (let item of set.values().flatMap((x) => [x, x.toUpperCase()])) {
        result.push(item);
        if (result.length === 3) {
            break;
        }
    }

    shouldBe(result.length, 3);
    shouldBe(result[0], "a");
    shouldBe(result[1], "A");
    shouldBe(result[2], "b");
}

