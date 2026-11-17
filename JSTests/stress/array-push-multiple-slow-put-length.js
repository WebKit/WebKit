function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: " + actual + " !== " + expected);
}

let seenLengths = new Uint32Array(200);
let seenCount = 0;

Object.defineProperty(Array.prototype, 0, {
    set() {
        seenLengths[seenCount++] = this.length;
        Object.defineProperty(this, "kept", { value: "kept" });
    }
});
Object.defineProperty(Array.prototype, 1, {
    set() {
        seenLengths[seenCount++] = this.length;
        shouldBe(this.kept, "kept");
    }
});

function push(array) {
    return array.push("a", "b");
}
noInline(push);

for (let i = 0; i < 100; ++i) {
    let array = [];
    shouldBe(push(array), 2);
    shouldBe(array.length, 2);
    shouldBe(array.kept, "kept");
}

shouldBe(seenCount, 200);
for (let i = 0; i < seenCount; ++i)
    shouldBe(seenLengths[i], 0);
