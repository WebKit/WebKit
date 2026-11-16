function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: " + String(actual) + ", expected " + String(expected));
}

// Species of a subclassed receiver must be respected with multiple arguments.
class MyArray extends Array {}
for (let i = 0; i < testLoopCount; i++) {
    let ma = new MyArray();
    ma.push(1, 2);
    let r = ma.concat([3], [4]);
    shouldBe(r instanceof MyArray, true);
    shouldBe(r.length, 4);
    shouldBe(r.join(","), "1,2,3,4");
}

// A derived array argument is spreadable even though it is not a plain array.
for (let i = 0; i < testLoopCount; i++) {
    let da = MyArray.of(5, 6);
    shouldBe([1].concat(da, [2]).join(","), "1,5,6,2");
    shouldBe([1].concat([0], da).join(","), "1,0,5,6");
}

// A Proxy over an array is spreadable.
for (let i = 0; i < testLoopCount; i++) {
    let prox = new Proxy([9, 10], {});
    shouldBe([1].concat(prox, [2]).join(","), "1,9,10,2");
    shouldBe([1].concat([2], prox).join(","), "1,2,9,10");
}

// An indexed accessor on the receiver must be invoked.
for (let i = 0; i < testLoopCount; i++) {
    let evil = [1, 2];
    Object.defineProperty(evil, 0, { get() { return 42; } });
    let r = evil.concat([3], [4]);
    shouldBe(r.join(","), "42,2,3,4");
}

// A Symbol.species getter on the receiver's constructor must be honored.
for (let i = 0; i < testLoopCount; i++) {
    let called = 0;
    class Weird extends Array {
        static get [Symbol.species]() {
            called++;
            return Array;
        }
    }
    let hijacked = Weird.of(1);
    let r = hijacked.concat([2], [3]);
    shouldBe(called >= 1, true);
    shouldBe(r instanceof Weird, false);
    shouldBe(r.join(","), "1,2,3");
}

// Frozen sources are readable.
for (let i = 0; i < testLoopCount; i++) {
    let frozen = Object.freeze([7, 8]);
    shouldBe([].concat(frozen, [9]).join(","), "7,8,9");
}
