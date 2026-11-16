function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: " + String(actual) + ", expected " + String(expected));
}

// Holes in sources must stay holes in the result.
for (let i = 0; i < testLoopCount; i++) {
    let holey = [1, , 3];
    let r = [0].concat(holey, [4], 9);
    shouldBe(r.length, 6);
    shouldBe(2 in r, false);
    shouldBe(r[1], 1);
    shouldBe(r[3], 3);
    shouldBe(r[4], 4);
    shouldBe(r[5], 9);
}

// Once Array.prototype has indexed properties, holes must forward to the prototype
// and the resolved values become own properties of the result.
Array.prototype[1] = "proto";
for (let i = 0; i < testLoopCount; i++) {
    let holey = [1, , 3];
    let r = [].concat(holey, [4]);
    shouldBe(r.length, 4);
    shouldBe(r[1], "proto");
    shouldBe(r.hasOwnProperty(1), true);
    shouldBe(r[3], 4);

    let r2 = [0].concat([5], holey);
    shouldBe(r2.length, 5);
    shouldBe(r2[3], "proto");
    shouldBe(r2.hasOwnProperty(3), true);
}
delete Array.prototype[1];
