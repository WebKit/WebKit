function shouldBe(actual, expected, msg) {
    if (actual !== expected)
        throw new Error(`${msg}: expected ${expected} but got ${actual}`);
}

function testFastPath() {
    {
        let r = /a/g;
        r.lastIndex = Number.MAX_SAFE_INTEGER + 1;
        let results = [...r[Symbol.matchAll]("aaa")];
        shouldBe(results.length, 0, "fast: MAX_SAFE_INTEGER + 1");
    }

    {
        let r = /a/g;
        r.lastIndex = Infinity;
        let results = [...r[Symbol.matchAll]("aaa")];
        shouldBe(results.length, 0, "fast: Infinity");
    }

    {
        let r = /a/g;
        r.lastIndex = 1e100;
        let results = [...r[Symbol.matchAll]("aaa")];
        shouldBe(results.length, 0, "fast: 1e100");
    }

    {
        let r = /a/g;
        r.lastIndex = 2.5;
        let results = [...r[Symbol.matchAll]("aaa")];
        shouldBe(results.length, 1, "fast: 2.5 -> 2");
        shouldBe(results[0].index, 2, "fast: 2.5 index");
    }

    {
        let r = /a/g;
        r.lastIndex = NaN;
        let results = [...r[Symbol.matchAll]("aaa")];
        shouldBe(results.length, 3, "fast: NaN -> 0");
    }

    {
        let r = /a/g;
        r.lastIndex = -5;
        let results = [...r[Symbol.matchAll]("aaa")];
        shouldBe(results.length, 3, "fast: -5 -> 0");
    }

    {
        let r = /a/g;
        r.lastIndex = -Infinity;
        let results = [...r[Symbol.matchAll]("aaa")];
        shouldBe(results.length, 3, "fast: -Infinity -> 0");
    }

    {
        let r = /a/g;
        r.lastIndex = Number.MAX_SAFE_INTEGER;
        let results = [...r[Symbol.matchAll]("aaa")];
        shouldBe(results.length, 0, "fast: MAX_SAFE_INTEGER");
    }
}

function testSlowPath() {
    class MyRegExp extends RegExp {}

    {
        let r = new MyRegExp("a", "g");
        r.lastIndex = Number.MAX_SAFE_INTEGER + 1;
        let results = [...r[Symbol.matchAll]("aaa")];
        shouldBe(results.length, 0, "slow: MAX_SAFE_INTEGER + 1");
    }

    {
        let r = new MyRegExp("a", "g");
        r.lastIndex = Infinity;
        let results = [...r[Symbol.matchAll]("aaa")];
        shouldBe(results.length, 0, "slow: Infinity");
    }

    {
        let r = new MyRegExp("a", "g");
        r.lastIndex = 2.5;
        let results = [...r[Symbol.matchAll]("aaa")];
        shouldBe(results.length, 1, "slow: 2.5 -> 2");
    }
}

for (let i = 0; i < testLoopCount; i++) {
    testFastPath();
    testSlowPath();
}
