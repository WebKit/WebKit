function shouldBe(actual, expected, message) {
    if (actual !== expected)
        throw new Error(`${message}: expected ${expected.slice(0, 40)}... (${expected.length}) but got ${actual.slice(0, 40)}... (${actual.length})`);
}

function referenceToString(x, radix) {
    const sign = x < 0n;
    if (sign)
        x = -x;
    const chunk = BigInt(radix) ** 9n;
    const parts = [];
    while (x >= chunk) {
        parts.push((x % chunk).toString(radix).padStart(9, "0"));
        x /= chunk;
    }
    parts.push(x.toString(radix));
    return (sign ? "-" : "") + parts.reverse().join("");
}

function makeOperand(width, digits, seed, shape) {
    const hexDigits = width / 4;
    const mask = (1n << BigInt(width)) - 1n;
    const shift = BigInt(64 - width);
    const parts = new Array(digits);
    let mix = BigInt.asUintN(64, 0x9e3779b97f4a7c15n * BigInt(seed + 1));
    for (let i = 0; i < digits; i++) {
        mix = BigInt.asUintN(64, mix * 6364136223846793005n + 1442695040888963407n);
        let digit;
        switch (shape) {
        case "random":
            digit = mix >> shift;
            break;
        case "ones":
            digit = mask;
            break;
        case "sparse":
            digit = (i * 7 + seed) % 5 === 0 ? mix >> shift : 0n;
            break;
        case "top":
            digit = i ? 0n : 1n << BigInt(width - 1);
            break;
        }
        parts[i] = digit.toString(16).padStart(hexDigits, "0");
    }
    if (shape === "random" || shape === "sparse")
        parts[0] = "8" + parts[0].slice(1);
    return BigInt("0x" + parts.join(""));
}

const radixes = [3, 5, 7, 12, 17, 24, 25, 36];
const shapes = ["random", "ones", "sparse", "top"];

let count = 0;
for (const width of [32, 64]) {
    for (const digits of [11, 12, 13, 23, 24, 25, 31, 32, 33, 47, 48, 49, 63, 64, 65, 127, 128, 129, 255, 256, 257]) {
        for (const shape of digits < 100 ? shapes : [shapes[digits % shapes.length]]) {
            const x = makeOperand(width, digits, digits, shape);
            for (const radix of [radixes[count++ % radixes.length], 10]) {
                const expected = referenceToString(x, radix);
                shouldBe(x.toString(radix), expected, `${width} bit digits, ${digits} ${shape} radix ${radix}`);
                shouldBe((-x).toString(radix), "-" + expected, `${width} bit digits, -${digits} ${shape} radix ${radix}`);
            }
        }
    }
}

for (const radix of [3, 10, 24, 36]) {
    const r = BigInt(radix);
    const last = (radix - 1).toString(radix);
    const exponents = new Set([0, 1]);
    for (const width of [32, 64]) {
        const chunkChars = Math.floor(width / Math.log2(radix));
        for (let chars = chunkChars; chars < 2000; chars *= 2) {
            exponents.add(chars - 1);
            exponents.add(chars);
            exponents.add(chars + 1);
        }
    }
    for (const high of [1500, 2000]) {
        const power = r ** BigInt(high);
        shouldBe(power.toString(radix), "1" + "0".repeat(high), `${radix} ** ${high}`);
        for (const low of exponents) {
            if (low >= high)
                continue;
            const lowPower = r ** BigInt(low);
            shouldBe((power + lowPower).toString(radix), "1" + "0".repeat(high - low - 1) + "1" + "0".repeat(low), `${radix} ** ${high} + ${radix} ** ${low}`);
            shouldBe((power - lowPower).toString(radix), last.repeat(high - low) + "0".repeat(low), `${radix} ** ${high} - ${radix} ** ${low}`);
            shouldBe((power * lowPower - 1n).toString(radix), last.repeat(high + low), `${radix} ** ${high + low} - 1`);
        }
    }
}

{
    const x = makeOperand(64, 500, 500, "random");
    if (BigInt(x.toString()) !== x)
        throw new Error("500 digits round trip");
}
