if (this.importScripts) {
    importScripts('../../../resources/js-test-pre.js');
    importScripts('shared.js');
}

description("Test IndexedDB key comparison using IDBFactory.cmp().");

function test()
{
    removeVendorPrefixes();

    shouldBeEqualToString("typeof indexedDB.cmp", "function");

    testValidKeys();
    testInvalidKeys();
    testIdenticalKeys();
    finishJSTest();
}

function testValidKeys()
{
    debug("");
    debug("compare valid keys");

    var keys = [
        "-Infinity",
        "-Number.MAX_VALUE",
        "-1",
        "-Number.MIN_VALUE",
        "0",
        "Number.MIN_VALUE",
        "1",
        "Number.MAX_VALUE",
        "Infinity",

        "new Date(0)",
        "new Date(1000)",
        "new Date(1317399931023)",

        "''",
        "'\x00'",
        "'a'",
        "'aa'",
        "'b'",
        "'ba'",

        "'\xA2'", // U+00A2 CENT SIGN
        "'\u6C34'", // U+6C34 CJK UNIFIED IDEOGRAPH (water)
        "'\uD834\uDD1E'", // U+1D11E MUSICAL SYMBOL G-CLEF (UTF-16 surrogate pair)
        "'\uFFFD'", // U+FFFD REPLACEMENT CHARACTER

        "[]",

        "[-Infinity]",
        "[-Number.MAX_VALUE]",
        "[-1]",
        "[-Number.MIN_VALUE]",
        "[0]",
        "[Number.MIN_VALUE]",
        "[1]",
        "[Number.MAX_VALUE]",
        "[Infinity]",

        "[new Date(0)]",
        "[new Date(1000)]",
        "[new Date(1317399931023)]",

        "['']",
        "['\x00']",
        "['a']",
        "['aa']",
        "['b']",
        "['ba']",

        "['\xA2']", // U+00A2 CENT SIGN
        "['\u6C34']", // U+6C34 CJK UNIFIED IDEOGRAPH (water)
        "['\uD834\uDD1E']", // U+1D11E MUSICAL SYMBOL G-CLEF (UTF-16 surrogate pair)
        "['\uFFFD']", // U+FFFD REPLACEMENT CHARACTER

        "[[]]",

        "[[], []]",
        "[[], [], []]",

        "[[[]]]",
        "[[[[]]]]"
    ];

    var i, key1, key2;
    for (i = 0; i < keys.length - 1; i += 1) {
        key1 = keys[i];
        key2 = keys[i + 1];
        shouldBe("indexedDB.cmp(" + key1 + "," + key2 + ")", "-1");
        shouldBe("indexedDB.cmp(" + key2 + "," + key1 + ")", "1");
        shouldBe("indexedDB.cmp(" + key1 + "," + key1 + ")", "0");
        shouldBe("indexedDB.cmp(" + key2 + "," + key2 + ")", "0");
    }
}

function testInvalidKeys()
{
    debug("");
    debug("compare invalid keys");

    var invalidKeys = [
        "void 0", // undefined
        "true",
        "false",
        "NaN",
        "new Date(NaN)",
        "null",
        "{}",
        "function () {}",
        "/regex/",
        "self",
        "self.document",
        "self.document.body"
    ];

    var i, key1, key2;
    for (i = 0; i < invalidKeys.length - 1; i += 1) {
        key1 = invalidKeys[i];
        key2 = invalidKeys[i + 1];
        evalAndExpectException("indexedDB.cmp(" + key1 + ", " + key2 + ")", "0", "'DataError'");
        evalAndExpectException("indexedDB.cmp(" + key2 + ", " + key1 + ")", "0", "'DataError'");
        evalAndExpectException("indexedDB.cmp(" + key1 + ", 'valid')", "0", "'DataError'");
        evalAndExpectException("indexedDB.cmp('valid', " + key1 + ")", "0", "'DataError'");
        evalAndExpectException("indexedDB.cmp(" + key2 + ", 'valid')", "0", "'DataError'");
        evalAndExpectException("indexedDB.cmp('valid', " + key2 + ")", "0", "'DataError'");
    }
}

function testIdenticalKeys()
{
    debug("");
    debug("compare identical keys");

    shouldBe("indexedDB.cmp(0, -0)", "0");
}

test();