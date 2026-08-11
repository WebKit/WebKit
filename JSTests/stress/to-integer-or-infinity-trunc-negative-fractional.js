function shouldBe(a, b) {
    if (a !== b)
        throw new Error(`Expected ${b} but got ${a}`);
}

const ta = new Uint8Array(1);
const result = ta.with(-0.5, 123);

shouldBe(result[0], 123);

