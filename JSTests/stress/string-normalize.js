function unicode(value) {
    return value.split('').map((val) => "\\u" + ("0000" + val.charCodeAt(0).toString(16)).slice(-4)).join('');
}

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${unicode(String(actual))}`);
}

function shouldThrow(func, errorMessage) {
    var errorThrown = false;
    var error = null;
    try {
        func();
    } catch (e) {
        errorThrown = true;
        error = e;
    }
    if (!errorThrown)
        throw new Error('not thrown');
    if (String(error) !== errorMessage)
        throw new Error(`bad error: ${String(error)}`);
}

shouldBe(String.prototype.hasOwnProperty('normalize'), true);
shouldBe(String.prototype.hasOwnProperty.length, 1);
shouldBe(Object.getOwnPropertyDescriptor(String.prototype, 'normalize').writable, true);
shouldBe(Object.getOwnPropertyDescriptor(String.prototype, 'normalize').enumerable, false);
shouldBe(Object.getOwnPropertyDescriptor(String.prototype, 'normalize').configurable, true);

shouldThrow(() => {
    "Test".normalize("Invalid");
}, `RangeError: argument does not match any normalization form`);

function normalizeTest(original, defaultValue, nfc, nfd, nfkc, nfkd) {
    shouldBe(original.normalize(), defaultValue);
    shouldBe(original.normalize("NFC"), nfc);
    shouldBe(original.normalize("NFD"), nfd);
    shouldBe(original.normalize("NFKC"), nfkc);
    shouldBe(original.normalize("NFKD"), nfkd);
}

{
    let text = "Cocoa";
    normalizeTest(text, text, text, text, text, text);
}

{
    // うさぎ
    // \u3046\u3055\u304e
    let text = "\u3046\u3055\u304e";
    normalizeTest(text, text, text, "\u3046\u3055\u304d\u3099", text, "\u3046\u3055\u304d\u3099");
}

{
    // é
    let text = "\u00e9";
    normalizeTest(text, text, text, "\u0065\u0301", text, "\u0065\u0301");
}

{
    // http://unicode.org/faq/normalization.html#6
    let text = "\u03d3";
    normalizeTest(text, text, text, "\u03d2\u0301", "\u038e", "\u03a5\u0301");
}
{
    // http://unicode.org/faq/normalization.html#6
    let text = "\u03d4";
    normalizeTest(text, text, text, "\u03d2\u0308", "\u03ab", "\u03a5\u0308");
}
{
    // http://unicode.org/faq/normalization.html#6
    let text = "\u1e9b";
    normalizeTest(text, text, text, "\u017f\u0307", "\u1e61", "\u0073\u0307");
}

{
    // http://unicode.org/faq/normalization.html#6
    let text = "\u1e9b";
    normalizeTest(text, text, text, "\u017f\u0307", "\u1e61", "\u0073\u0307");
}

{
    // http://unicode.org/faq/normalization.html#12
    normalizeTest("\ud834\udd60",
            "\ud834\udd58\ud834\udd65\ud834\udd6e",
            "\ud834\udd58\ud834\udd65\ud834\udd6e",
            "\ud834\udd58\ud834\udd65\ud834\udd6e",
            "\ud834\udd58\ud834\udd65\ud834\udd6e",
            "\ud834\udd58\ud834\udd65\ud834\udd6e");
    normalizeTest("\uFB2C",
            "\u05e9\u05bc\u05c1",
            "\u05e9\u05bc\u05c1",
            "\u05e9\u05bc\u05c1",
            "\u05e9\u05bc\u05c1",
            "\u05e9\u05bc\u05c1",
            "\u05e9\u05bc\u05c1"
            );
    normalizeTest("\u0390",
            "\u0390",
            "\u0390",
            "\u03b9\u0308\u0301",
            "\u0390",
            "\u03b9\u0308\u0301"
            );
    normalizeTest("\u1F82",
            "\u1f82",
            "\u1f82",
            "\u03b1\u0313\u0300\u0345",
            "\u1f82",
            "\u03b1\u0313\u0300\u0345"
            );
    normalizeTest("\uFDFA",
            "\ufdfa",
            "\ufdfa",
            "\ufdfa",
            "\u0635\u0644\u0649\u0020\u0627\u0644\u0644\u0647\u0020\u0639\u0644\u064a\u0647\u0020\u0648\u0633\u0644\u0645",
            "\u0635\u0644\u0649\u0020\u0627\u0644\u0644\u0647\u0020\u0639\u0644\u064a\u0647\u0020\u0648\u0633\u0644\u0645"
            );
}
