function shouldBe(a, b) {
    if (a !== b)
        throw new Error(`Expected ${b} but got ${a}`);
}

var fn = () => {};
fn.a = 1;
Object.defineProperty(fn, "name", {enumerable: true});
Object.defineProperty(fn, "length", {enumerable: true});

const result = Object.keys(fn);

shouldBe(result.length, 3);
shouldBe(result[0], "length");
shouldBe(result[1], "name");
shouldBe(result[2], "a");
