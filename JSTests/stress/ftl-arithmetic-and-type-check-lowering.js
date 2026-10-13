function shouldBe(actual, expected, message) {
    if (actual !== expected && !(actual !== actual && expected !== expected))
        throw new Error(`${message}: expected ${String(expected)} but got ${String(actual)}`);
}

let clamped = new Uint8ClampedArray(4);
function storeClamped(value) { clamped[0] = value; return clamped[0]; }
noInline(storeClamped);
let clampedInputs = [-1, 0, 1, 254, 255, 256, -0x80000000, 0x7fffffff, 128];

function lessThan(a, b) { return a < b; }
noInline(lessThan);
let compareInputs = [[1, 2], [2, 1], [-1, 1], [1, -1], [0x7fffffff, -0x80000000], [1.5, 2], [2, 1.5], ["a", "b"], [1, "2"], [null, 1], [undefined, 1], [true, 2]];

function mul(a, b) { return a * b; }
function div(a, b) { return a / b; }
function mod(a, b) { return a % b; }
noInline(mul);
noInline(div);
noInline(mod);

function not(value) { return !value; }
function boxCompare(a, b) { return [a === b]; }
noInline(not);
noInline(boxCompare);

function sloppyThis() { return this; }
function callSloppyThis(receiver) { return sloppyThis.call(receiver); }
noInline(callSloppyThis);

function lastIndexOf(array, value, fromIndex) { return Array.prototype.lastIndexOf.call(array, value, fromIndex); }
noInline(lastIndexOf);
let arrayLike = { length: 3, 0: "a", 1: "b", 2: "a" };

for (let i = 0; i < testLoopCount; ++i) {
    let clampedInput = clampedInputs[i % clampedInputs.length];
    shouldBe(storeClamped(clampedInput), Math.min(255, Math.max(0, clampedInput)), "Uint8ClampedArray store");

    let [a, b] = compareInputs[i % compareInputs.length];
    shouldBe(lessThan(a, b), a < b, `${String(a)} < ${String(b)}`);

    shouldBe(mul(i & 7, 3), (i & 7) * 3, "mul");
    shouldBe(1 / mul(0, -(i & 3)), 1 / (0 * -(i & 3)), "mul negative zero");
    shouldBe(div(-0x80000000, (i & 1) ? -1 : 2), (i & 1) ? 0x80000000 : -0x40000000, "div overflow");
    shouldBe(div(i, (i & 1) ? 0 : 1), (i & 1) ? Infinity : i, "div by zero");
    shouldBe(mod(-0x80000000, (i & 1) ? -1 : 3), (i & 1) ? -0 : -2, "mod overflow");
    shouldBe(1 / mod(-(i & 7) * 4, 4), -Infinity, "mod negative zero");

    shouldBe(not(i & 1 ? true : false), !(i & 1), "boolean not");
    shouldBe(boxCompare(i & 1, 1)[0], (i & 1) === 1, "boxed boolean");

    let receiver = (i & 3) ? { i } : globalThis;
    shouldBe(callSloppyThis(receiver), receiver, "sloppy this");

    shouldBe(lastIndexOf(arrayLike, "a", (i & 3) - 2), [0, 2, 0, 0][i & 3], "lastIndexOf with negative fromIndex");
}
