function shouldBe(actual, expected, message) {
    if (actual !== expected)
        throw new Error(`${message}: expected ${expected.slice(0, 40)}... (${expected.length}) but got ${actual.slice(0, 40)}... (${actual.length})`);
}

function check(radix, widths) {
    const r = BigInt(radix);
    const last = (radix - 1).toString(radix);
    let x = 0n;
    let expected = "";
    for (let i = 0; i < widths.length; i++) {
        const power = r ** BigInt(widths[i]);
        if (i & 1) {
            x = x * power + 1n;
            expected += "0".repeat(widths[i] - 1) + "1";
        } else {
            x = x * power + power - 1n;
            expected += last.repeat(widths[i]);
        }
    }
    shouldBe(x.toString(radix), expected, `radix ${radix}, ${widths}`);
}

check(10, [100000, 60001, 19, 77777, 1, 77202]);
check(3, [40, 70000, 12345, 1, 50000, 28614]);
check(24, [30000, 13, 9987, 16000]);
check(36, [1, 49999, 25000, 12, 24988]);
