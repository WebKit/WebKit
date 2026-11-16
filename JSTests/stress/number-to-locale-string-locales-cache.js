function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${actual}, expected: ${expected}`);
}

function shouldThrow(func, errorType) {
    let error;
    try {
        func();
    } catch (e) {
        error = e;
    }
    if (!(error instanceof errorType))
        throw new Error(`bad error: ${String(error)}`);
}

shouldBe((1234.5).toLocaleString("de-DE"), "1.234,5");
fullGC();
shouldBe((1234.5).toLocaleString("de-DE"), "1.234,5");
shouldBe(1234n.toLocaleString("de-DE"), "1.234");

const numbers = [0, -0, 1, -1, 1234, 1234567.891, 0.000001, -1e-7, 1e21, 2 ** 53, NaN, Infinity, -Infinity];
const bigints = [0n, 1n, -1n, 1234n, 2n ** 64n, -123456789012345678901234567890n];
const locales = ["en-US", "de-DE", "ja-JP", "fr", "hi-IN", "ar-EG", "en-US-u-nu-thai", "zh-Hans-CN-u-nu-hanidec", "und"];

for (const locale of locales) {
    const format = new Intl.NumberFormat(locale);
    for (let i = 0; i < 3; ++i) {
        for (const number of numbers)
            shouldBe(number.toLocaleString(locale), format.format(number));
        for (const bigint of bigints)
            shouldBe(bigint.toLocaleString(locale), format.format(bigint));
    }
}

for (let i = 0; i < 100; ++i) {
    const locale = locales[i % locales.length];
    const format = new Intl.NumberFormat(locale);
    shouldBe((1234567.891).toLocaleString(locale), format.format(1234567.891));
    shouldBe(1234567n.toLocaleString(locale), format.format(1234567n));
}

for (let i = 0; i < 3; ++i) {
    shouldThrow(() => (1).toLocaleString("xx-fake-INVALID!"), RangeError);
    shouldThrow(() => 1n.toLocaleString("xx-fake-INVALID!"), RangeError);
    shouldBe((1234.5).toLocaleString("en-US"), "1,234.5");
    shouldBe(1234n.toLocaleString("en-US"), "1,234");
}

for (let i = 0; i < 3; ++i) {
    shouldBe((1234.5).toLocaleString("de-DE"), "1.234,5");
    shouldBe((1234.5).toLocaleString(["en-US"]), "1,234.5");
    shouldBe((1234.5).toLocaleString("de-DE"), "1.234,5");
    shouldBe((1234.5).toLocaleString("de-DE", { maximumFractionDigits: 0 }), "1.235");
    shouldBe((1234.5).toLocaleString("de-DE"), "1.234,5");
    shouldBe((1234.5).toLocaleString("de-DE", { style: "percent" }), new Intl.NumberFormat("de-DE", { style: "percent" }).format(1234.5));
    shouldThrow(() => (1234.5).toLocaleString("de-DE", null), TypeError);
    shouldThrow(() => 1234n.toLocaleString("de-DE", null), TypeError);
    shouldBe((1234.5).toLocaleString(undefined), (1234.5).toLocaleString());
    shouldBe((1234.5).toLocaleString("de-DE"), "1.234,5");
    shouldBe(1234n.toLocaleString("de-DE", { minimumFractionDigits: 2 }), "1.234,00");
    shouldBe(1234n.toLocaleString("de-DE"), "1.234");
    shouldBe(1234n.toLocaleString(undefined), 1234n.toLocaleString());
}

const prefix = "de";
for (let i = 0; i < 3; ++i) {
    shouldBe((1234.5).toLocaleString(prefix + "-DE"), "1.234,5");
    shouldBe(1234n.toLocaleString(prefix + "-DE"), "1.234");
}

shouldBe(new Number(1234.5).toLocaleString("de-DE"), "1.234,5");
shouldBe(Object(1234n).toLocaleString("de-DE"), "1.234");
shouldBe([1234.5, 1234n, 0.5].toLocaleString("de-DE"), "1.234,5,1.234,0,5");
shouldBe(new Float64Array([1234.5, 0.5]).toLocaleString("de-DE"), "1.234,5,0,5");

Object.prototype.style = "percent";
Object.prototype.maximumFractionDigits = 0;
Object.prototype.useGrouping = false;
for (let i = 0; i < 3; ++i) {
    shouldBe((1234.5).toLocaleString("en-US"), "1,234.5");
    shouldBe(1234n.toLocaleString("en-US"), "1,234");
    shouldBe((1234.5).toLocaleString("en-US", {}), "123450%");
}
delete Object.prototype.style;
delete Object.prototype.maximumFractionDigits;
delete Object.prototype.useGrouping;
shouldBe((1234.5).toLocaleString("en-US"), "1,234.5");

const originalFormat = Object.getOwnPropertyDescriptor(Intl.NumberFormat.prototype, "format");
const originalNumberFormat = Intl.NumberFormat;
Object.defineProperty(Intl.NumberFormat.prototype, "format", { get() { throw new Error("should not be called"); } });
Intl.NumberFormat = function () { throw new Error("should not be called"); };
for (let i = 0; i < 3; ++i) {
    shouldBe((1234.5).toLocaleString("en-US"), "1,234.5");
    shouldBe((1234.5).toLocaleString("ja-JP"), "1,234.5");
    shouldBe(1234n.toLocaleString("de-DE"), "1.234");
}
Intl.NumberFormat = originalNumberFormat;
Object.defineProperty(Intl.NumberFormat.prototype, "format", originalFormat);

const other = createGlobalObject();
for (let i = 0; i < 3; ++i) {
    shouldBe((1234.5).toLocaleString("en-US"), "1,234.5");
    shouldBe(other.Number.prototype.toLocaleString.call(1234.5, "de-DE"), "1.234,5");
    shouldBe(other.BigInt.prototype.toLocaleString.call(1234n, "de-DE"), "1.234");
    shouldBe(1234n.toLocaleString("en-US"), "1,234");
}
