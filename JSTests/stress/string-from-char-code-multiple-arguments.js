function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: " + JSON.stringify(actual) + " expected: " + JSON.stringify(expected));
}

function reference(codes) {
    let result = "";
    for (let i = 0; i < codes.length; ++i)
        result += String.fromCharCode(codes[i]);
    return result;
}

function check(codes) {
    let expected = reference(codes);
    let actual = String.fromCharCode.apply(null, codes);
    shouldBe(actual, expected);
    shouldBe(is8BitString(actual), /^[\x00-\xff]*$/.test(expected));
}

function thrownBy(func) {
    try {
        func();
    } catch (error) {
        return error;
    }
    throw new Error("did not throw");
}

function two(a, b) { return String.fromCharCode(a, b); }
noInline(two);
function three(a, b, c) { return String.fromCharCode(a, b, c); }
noInline(three);

shouldBe(is8BitString(two(0, 0xff)), true);
shouldBe(is8BitString(two(0x10041, 0x7fff0042)), true);
shouldBe(is8BitString(two(0x41, 65.5)), true);
shouldBe(is8BitString(two(0xff, 0x100)), false);
shouldBe(is8BitString(two(0x100, 0xff)), false);
shouldBe(is8BitString(two(0x3042, 65.5)), false);

shouldBe(String.fromCharCode(), "");
shouldBe(String.fromCharCode.apply(null, []), "");

for (let i = 0; i < testLoopCount; ++i) {
    shouldBe(two(0x41, 0x42), "AB");
    shouldBe(two(0, 0xff), "\x00\xff");
    shouldBe(two(0, 0), "\0\0");
    shouldBe(two(0x10000, -0x10000), "\0\0");
    shouldBe(two(0xff, 0x100), "\xff\u0100");
    shouldBe(two(0x100, 0xff), "\u0100\xff");
    shouldBe(two(0xd83d, 0xde00), "\u{1f600}");
    shouldBe(three(0x41, 0x3042, 0x42), "A\u3042B");
    shouldBe(two(0x10041, 0x7fff0042), "AB");
    shouldBe(two(-1, -0x80000000), "\uffff\x00");
    shouldBe(two(-65536 + 0x41, 0x42), "AB");
    shouldBe(two(65.9, 66.1), "AB");
    shouldBe(two(0x41, 4294967296 + 0x42), "AB");
    shouldBe(three(0x41, "66", true), "AB\x01");
    shouldBe(three(undefined, null, NaN), "\0\0\0");
    shouldBe(three(0x3042, 65.5, Infinity), "\u3042A\0");
}

for (let length of [4, 7, 8, 9, 16, 31, 32, 33, 255, 1024]) {
    for (let position of [-1, 0, 1, length >> 1, length - 1]) {
        let codes = [];
        for (let i = 0; i < length; ++i)
            codes.push(i === position ? 0x3042 + i : i & 0xff);
        check(codes);
        let doubles = codes.slice();
        doubles[0] = 65.5;
        check(doubles);
        for (let mixedPosition of [0, length >> 1, length - 1]) {
            let mixed = codes.slice();
            mixed[mixedPosition] = "12354";
            check(mixed);
        }
    }
}

for (let codes of [[0x41, 0xff, 0x42], [0x41, 0x3042, 0x42]]) {
    shouldBe(String.fromCharCode(...codes), reference(codes));
    shouldBe(String.fromCharCode.apply(null, new Uint16Array(codes)), reference(codes));
}

{
    let log = [];
    let make = (name, value) => ({ valueOf() { log.push(name); return value; } });
    shouldBe(String.fromCharCode(0x41, make("a", 0x42), 0x3042, make("b", 0x43)), "AB\u3042C");
    shouldBe(log.join(), "a,b");

    log = [];
    let error = new Error("thrown");
    shouldBe(thrownBy(() => String.fromCharCode(0x41, make("a", 0x42), { valueOf() { log.push("throw"); throw error; } }, make("c", 0x43))), error);
    shouldBe(log.join(), "a,throw");
    shouldBe(thrownBy(() => String.fromCharCode(0x3042, 0x42, { valueOf() { throw error; } })), error);
}

for (let length of [4, 1024]) {
    let codes = [];
    for (let i = 0; i < length; ++i)
        codes.push(0x41 + (i & 15));
    let expected = reference(codes);
    let nested = null;
    codes[1] = {
        valueOf() {
            nested = String.fromCharCode.apply(null, new Array(length).fill(66.5));
            codes.length = 0;
            gc();
            return 0x42;
        }
    };
    shouldBe(String.fromCharCode.apply(null, codes), expected);
    shouldBe(nested, "B".repeat(length));
}

for (let invalid of [Symbol("s"), 1n])
    shouldBe(thrownBy(() => String.fromCharCode(0x41, invalid)) instanceof TypeError, true);
