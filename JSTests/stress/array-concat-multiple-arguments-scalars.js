function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: " + String(actual) + ", expected " + String(expected));
}

// Scalar arguments of every primitive kind are appended as single elements.
for (let i = 0; i < testLoopCount; i++) {
    shouldBe([1, 2].concat(3, 4).join(","), "1,2,3,4");
    shouldBe([1, 2].concat(3, 4.5).join(","), "1,2,3,4.5");
    shouldBe([1.5].concat(2, 3.5).join(","), "1.5,2,3.5");
    shouldBe(["a"].concat("b", "c").join(","), "a,b,c");
    shouldBe([1, 2].concat("x", 3).join(","), "1,2,x,3");
    shouldBe([1.5].concat("x", 2).join(","), "1.5,x,2");

    let r = [1].concat(undefined, null, true, false);
    shouldBe(r.length, 5);
    shouldBe(r.hasOwnProperty(1), true);
    shouldBe(r[1], undefined);
    shouldBe(r[2], null);
    shouldBe(r[3], true);
    shouldBe(r[4], false);

    let sym = Symbol("s");
    shouldBe([1].concat(sym, 2)[1], sym);
}

// Mixing arrays and scalars in one call.
for (let i = 0; i < testLoopCount; i++) {
    shouldBe([0].concat([1, 2], 3).join(","), "0,1,2,3");
    shouldBe([0].concat(1, [2, 3]).join(","), "0,1,2,3");
    shouldBe([].concat([1.5], 2, [3.5, 4.5]).join(","), "1.5,2,3.5,4.5");
}

// Object scalars keep their identity and are never coerced.
for (let i = 0; i < testLoopCount; i++) {
    let o = { x: 1 };
    let r = [1].concat(o, [2]);
    shouldBe(r.length, 3);
    shouldBe(r[1], o);

    let evil = { valueOf() { throw new Error("must not coerce"); } };
    shouldBe([1].concat(evil, 2)[1], evil);
}

// Special double values survive the copy.
for (let i = 0; i < testLoopCount; i++) {
    let r = [0.5, NaN].concat(1, [2.5]);
    shouldBe(r.length, 4);
    shouldBe(Number.isNaN(r[1]), true);
    shouldBe(r[2], 1);

    r = [1.5].concat(-0, Infinity);
    shouldBe(Object.is(r[1], -0), true);
    shouldBe(r[2], Infinity);

    r = [-0].concat(0.5, [1.5]);
    shouldBe(Object.is(r[0], -0), true);
}

// Scalars appended after holey sources land at the right indices.
for (let i = 0; i < testLoopCount; i++) {
    let holey = [1, , 3];
    let r = [0].concat(holey, 9);
    shouldBe(r.length, 5);
    shouldBe(2 in r, false);
    shouldBe(r[4], 9);
}
