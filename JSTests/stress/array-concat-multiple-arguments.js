function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: " + String(actual) + ", expected " + String(expected));
}

function arrayEq(a, b) {
    if (a.length !== b.length)
        return false;
    for (let i = 0; i < a.length; i++) {
        if (a[i] !== b[i])
            return false;
    }
    return true;
}

// All indexing type combinations across receiver and multiple arguments.
for (let i = 0; i < testLoopCount; i++) {
    shouldBe([].concat([1, 2], [3, 4]).join(","), "1,2,3,4");
    shouldBe([0].concat([1, 2], [3], [4, 5, 6]).join(","), "0,1,2,3,4,5,6");
    shouldBe([1, 2].concat([1.5, 2.5], [3, 4]).join(","), "1,2,1.5,2.5,3,4");
    shouldBe([0.5].concat([1.5], [2.5, 3.5]).join(","), "0.5,1.5,2.5,3.5");
    shouldBe(["a"].concat([1, 2], [1.5]).join(","), "a,1,2,1.5");
    shouldBe(["a", "b"].concat(["c"], [1, 2]).join(","), "a,b,c,1,2");
    shouldBe([].concat([], []).length, 0);
    shouldBe([].concat([], [1], []).join(","), "1");
}

// Undecided (allocated but unset) arrays as sources.
for (let i = 0; i < testLoopCount; i++) {
    let und = new Array(2);
    let r = [1].concat(und, [2]);
    shouldBe(r.length, 4);
    shouldBe(1 in r, false);
    shouldBe(2 in r, false);
    shouldBe(r[3], 2);

    r = [].concat(new Array(3), new Array(2));
    shouldBe(r.length, 5);
    shouldBe(0 in r, false);
}

// Self concat only reads the sources.
for (let i = 0; i < testLoopCount; i++) {
    let s = [1, 2];
    shouldBe(s.concat(s, s).join(","), "1,2,1,2,1,2");
    shouldBe(s.length, 2);
}

// The result must not share storage with any (copy-on-write) source.
for (let i = 0; i < testLoopCount; i++) {
    let a = [1, 2, 3];
    let r = [].concat(a, [4]);
    r[0] = 99;
    shouldBe(a[0], 1);
    let r2 = [].concat(a, []);
    r2[1] = 88;
    shouldBe(a[1], 2);
}

// Many arguments.
{
    let args = [];
    let expected = [];
    for (let i = 0; i < 64; i++) {
        args.push([i, i + 0.5]);
        expected.push(i, i + 0.5);
    }
    for (let i = 0; i < testLoopCount; i++) {
        let r = Array.prototype.concat.apply([], args);
        if (!arrayEq(r, expected))
            throw new Error("bad many-args result");
    }
}

// Total size crossing the sparse array threshold must still produce a correct result.
{
    let big1 = new Array(50000).fill(1);
    let big2 = new Array(60000).fill(2);
    let r = [].concat(big1, big2, [3]);
    shouldBe(r.length, 110001);
    shouldBe(r[0], 1);
    shouldBe(r[49999], 1);
    shouldBe(r[50000], 2);
    shouldBe(r[109999], 2);
    shouldBe(r[110000], 3);
}
