function shouldBe(a, b) {
    if (a !== b)
        throw new Error(`Expected ${b} but got ${a}`);
}

var fn = () => {};
fn.a = 1;
Object.defineProperty(fn, "name", {enumerable: true});
Object.defineProperty(fn, "length", {enumerable: true});

const result = Object.entries(fn);

shouldBe(result.length, 3);
shouldBe(result[0][0], "length");
shouldBe(result[0][1], 0);
shouldBe(result[1][0], "name");
shouldBe(result[1][1], "fn");
shouldBe(result[2][0], "a");
shouldBe(result[2][1], 1);
