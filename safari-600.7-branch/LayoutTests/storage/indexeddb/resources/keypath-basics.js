if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
    importScripts('shared.js');
}

description("Test for valid and invalid keypaths");

indexedDBTest(prepareDatabase);
function prepareDatabase()
{
    db = event.target.result;
    event.target.transaction.onabort = unexpectedAbortCallback;
    evalAndLog("store = db.createObjectStore('name')");
    shouldBeNull("store.keyPath");
    deleteAllObjectStores(db);

    testKeyPaths = [
        { keyPath: "null", storeExpected: "null", indexExpected: "'null'" },
        { keyPath: "undefined", storeExpected: "null", indexExpected: "'undefined'" },
        { keyPath: "''" },
        { keyPath: "'foo'" },
        { keyPath: "'foo.bar.baz'" },

        // IdentifierStart
        { keyPath: "'$'" },
        { keyPath: "'_'" },
        { keyPath: "'\\u0391'" }, // GREEK CAPITAL LETTER ALPHA (Lc)
        { keyPath: "'\\u0371'" }, // GREEK SMALL LETTER ALPHA (Lu)
        { keyPath: "'\\u01C5'" }, // LATIN CAPITAL LETTER D WITH SMALL LETTER Z WITH CARON (Lt)
        { keyPath: "'\\u02B0'" }, // MODIFIER LETTER SMALL H (Lm)
        { keyPath: "'\\u00AA'" }, // FEMININE ORDINAL INDICATOR (Lo)
        { keyPath: "'\\u16EE'" }, // RUNIC ARLAUG SYMBOL (Nl)

        // IdentifierPart
        { keyPath: "'_$'" },
        { keyPath: "'__'" },
        { keyPath: "'_\\u0391'" }, // GREEK CAPITAL LETTER ALPHA (Lc)
        { keyPath: "'_\\u0371'" }, // GREEK SMALL LETTER ALPHA (Lu)
        { keyPath: "'_\\u01C5'" }, // LATIN CAPITAL LETTER D WITH SMALL LETTER Z WITH CARON (Lt)
        { keyPath: "'_\\u02B0'" }, // MODIFIER LETTER SMALL H (Lm)
        { keyPath: "'_\\u00AA'" }, // FEMININE ORDINAL INDICATOR (Lo)
        { keyPath: "'_\\u16EE'" }, // RUNIC ARLAUG SYMBOL (Nl)
        { keyPath: "'_\\u0300'" }, // COMBINING GRAVE ACCENT (Mn)
        { keyPath: "'_\\u0903'" }, // DEVANAGARI SIGN VISARGA (Mc)
        { keyPath: "'_\\u0300'" }, // DIGIT ZERO (Nd)
        { keyPath: "'_\\u203F'" }, // UNDERTIE (Pc)
        { keyPath: "'_\\u200C'" }, // ZWNJ
        { keyPath: "'_\\u200D'" }  // ZWJ
    ];

    testKeyPaths.forEach(function (testCase) {
        storeExpected = ('storeExpected' in testCase) ? testCase.storeExpected : testCase.keyPath;
        indexExpected = ('indexExpected' in testCase) ? testCase.indexExpected : testCase.keyPath;

        evalAndLog("store = db.createObjectStore('name', {keyPath: " + testCase.keyPath + "})");
        shouldBe("store.keyPath", storeExpected);
        evalAndLog("index = store.createIndex('name', " + testCase.keyPath + ")");
        shouldBe("index.keyPath", indexExpected);
        deleteAllObjectStores(db);
    });

    testInvalidKeyPaths();
}

function testInvalidKeyPaths()
{
    debug("");
    debug("testInvalidKeyPaths():");
    deleteAllObjectStores(db);

    debug("");
    debug("Object store key path may not be empty or an array if autoIncrement is true");
    testKeyPaths = ["''", "['a']", "['']"];
    testKeyPaths.forEach(function (keyPath) {
        store = evalAndExpectException("store = db.createObjectStore('storeName', {autoIncrement: true, keyPath: " + keyPath + "})", "DOMException.INVALID_ACCESS_ERR");
        deleteAllObjectStores(db);
    });

    debug("");
    debug("Key paths which are never valid:");
    testKeyPaths = [
        "' '",
        "'foo '",
        "'foo bar'",
        "'foo. bar'",
        "'foo .bar'",
        "'foo..bar'",
        "'+foo'",
        "'foo%'",
        "'1'",
        "'1.0'",
        "[]",

        // IdentifierPart but not IdentifierStart
        "'\\u0300'", // COMBINING GRAVE ACCENT (Mn)
        "'\\u0903'", // DEVANAGARI SIGN VISARGA (Mc)
        "'\\u0300'", // DIGIT ZERO (Nd)
        "'\\u203F'", // UNDERTIE (Pc)
        "'\\u200C'", // ZWNJ
        "'\\u200D'", // ZWJ

        // Neither IdentifierPart nor IdentifierStart
        "'\\u002D'", // HYPHEN-MINUS (Pd)
        "'\\u0028'", // LEFT PARENTHESIS (Ps)
        "'\\u0029'", // RIGHT PARENTHESIS (Pe)
        "'\\u00AB'", // LEFT-POINTING DOUBLE ANGLE QUOTATION MARK (Pi)
        "'\\u00BB'", // RIGHT-POINTING DOUBLE ANGLE QUOTATION MARK (Pf)
        "'\\u0021'", // EXCLAMATION MARK (Po)
        "'\\u002B'", // PLUS SIGN (Sm)
        "'\\u00A2'", // CENT SIGN (Sc)
        "'\\u005E'", // CIRCUMFLEX ACCENT (Sk)
        "'\\u00A6'", // BROKEN BAR (So)
        "'\\u00A0'", // NO-BREAK SPACE (Zs)
        "'\\u2028'", // LINE SEPARATOR (Zl)
        "'\\u2029'", // PARAGRAPH SEPARATOR (Zp)
        "'\\u0000'", // NULL (Cc)
        "'\\u00AD'", // SOFT HYPHEN (Cf)
        "'\\uD800'", // Surrogate (Cs)
        "'\\uE000'", // Private Use (Co)
        "'\\uFFFE'", // Special
        "'\\uFFFF'", // Special

        // Neither IdentifierPart nor IdentifierStart
        "'_\\u002D'", // HYPHEN-MINUS (Pd)
        "'_\\u0028'", // LEFT PARENTHESIS (Ps)
        "'_\\u0029'", // RIGHT PARENTHESIS (Pe)
        "'_\\u00AB'", // LEFT-POINTING DOUBLE ANGLE QUOTATION MARK (Pi)
        "'_\\u00BB'", // RIGHT-POINTING DOUBLE ANGLE QUOTATION MARK (Pf)
        "'_\\u0021'", // EXCLAMATION MARK (Po)
        "'_\\u002B'", // PLUS SIGN (Sm)
        "'_\\u00A2'", // CENT SIGN (Sc)
        "'_\\u005E'", // CIRCUMFLEX ACCENT (Sk)
        "'_\\u00A6'", // BROKEN BAR (So)
        "'_\\u00A0'", // NO-BREAK SPACE (Zs)
        "'_\\u2028'", // LINE SEPARATOR (Zl)
        "'_\\u2029'", // PARAGRAPH SEPARATOR (Zp)
        "'_\\u0000'", // NULL (Cc)
        "'_\\u00AD'", // SOFT HYPHEN (Cf)
        "'_\\uD800'", // Surrogate (Cs)
        "'_\\uE000'", // Private Use (Co)
        "'_\\uFFFE'", // Special
        "'_\\uFFFF'"  // Special
    ];
    testKeyPaths.forEach(function (keyPath) {
        evalAndExpectException("db.createObjectStore('name', {keyPath: " + keyPath + "})", "DOMException.SYNTAX_ERR");
        evalAndExpectException("db.createObjectStore('name').createIndex('name', " + keyPath + ")", "DOMException.SYNTAX_ERR");
        deleteAllObjectStores(db);
    });

    finishJSTest();
}
