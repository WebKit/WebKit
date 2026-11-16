function shouldBe(actual, expected, text) {
    if (!Object.is(actual, expected))
        throw new Error(`bad value for ${text}: ${actual}, expected: ${expected}`);
}

// Decimals of 19 significant digits within one unit in the last digit of the exact midpoint between two
// adjacent doubles. These are the numbers whose rounding is hardest to decide from an approximate product.
let seed = 4242;
function rand() { seed = (seed * 16807) % 2147483647; return seed / 2147483647; }

function midpointDigits(bits) {
    let biasedExponent = Number((bits >> 52n) & 0x7ffn);
    let fraction = bits & ((1n << 52n) - 1n);
    let significand = biasedExponent ? fraction | (1n << 52n) : fraction;
    let exponent = (biasedExponent ? biasedExponent - 1075 : -1074) - 1;
    let numerator = 2n * significand + 1n;
    if (exponent >= 0)
        return { digits: (numerator << BigInt(exponent)).toString(), decimalExponent: 0 };
    return { digits: (numerator * 5n ** BigInt(-exponent)).toString(), decimalExponent: exponent };
}

let texts = [];
for (let i = 0; i < 1500; ++i) {
    let bits;
    if (i % 10 === 0)
        bits = BigInt(1 + Math.floor(rand() * 2 ** 26)) * BigInt(1 + Math.floor(rand() * 2 ** 26));
    else
        bits = (BigInt(1 + Math.floor(rand() * 2045)) << 52n) | (BigInt(Math.floor(rand() * 2 ** 26)) << 26n) | BigInt(Math.floor(rand() * 2 ** 26));
    let { digits, decimalExponent } = midpointDigits(bits);
    let dropped = Math.max(digits.length - 19, 0);
    let head = BigInt(digits.slice(0, 19));
    for (let delta of [-1n, 0n, 1n]) {
        let text = (head + delta).toString();
        texts.push(`${text[0]}.${text.slice(1)}e${decimalExponent + dropped + text.length - 1}`);
    }
}

// JSONCache matches a key only when at least 32 characters of source remain.
const padding = " ".repeat(40);
let array = JSON.parse(`[${texts.join(",")}]`);
let wide = JSON.parse(`["\u3042",${texts.join(",")}]`);
let objectsText = `[${texts.map(text => `{"a":${text}}`).join(",")}]${padding}`;
let objects;
for (let i = 0; i < 3; ++i)
    objects = JSON.parse(objectsText);
for (let i = 0; i < texts.length; ++i) {
    let expected = Number(texts[i]);
    shouldBe(array[i], expected, texts[i]);
    shouldBe(wide[i + 1], expected, texts[i]);
    shouldBe(objects[i].a, expected, texts[i]);
}
