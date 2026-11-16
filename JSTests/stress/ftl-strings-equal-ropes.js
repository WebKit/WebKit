function shouldBe(actual, expected, message) {
    if (actual !== expected)
        throw new Error(`${message}: expected ${expected} but got ${actual}`);
}

function equal(a, b) { return a === b; }
noInline(equal);

let flat = ["hello world", "hello worle", "あい", "あぅ", "hello", ""];

for (let i = 0; i < testLoopCount; ++i) {
    let left = flat[i % flat.length];
    let right = flat[(i >> 3) % flat.length];
    let ropeLeft = left.substring(0, left.length >> 1) + left.substring(left.length >> 1);
    let ropeRight = right.substring(0, right.length >> 1) + right.substring(right.length >> 1);
    let expected = flat.indexOf(left) === flat.indexOf(right);
    shouldBe(equal(left, right), expected, "flat/flat");
    shouldBe(equal(ropeLeft, right), expected, "rope/flat");
    shouldBe(equal(left, ropeRight), expected, "flat/rope");
    shouldBe(equal(ropeLeft, ropeRight), expected, "rope/rope");
}
