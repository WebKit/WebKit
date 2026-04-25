function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function shouldBeOneOf(actual, expectedArray) {
    if (!expectedArray.some((value) => value === actual))
        throw new Error('bad value: ' + actual + ' expected values: ' + expectedArray);
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

var regionNames = new Intl.DisplayNames(['en'], {type: 'region'});
shouldThrow(() => regionNames.of(""), `RangeError: argument is not a region subtag`);
shouldThrow(() => regionNames.of("-"), `RangeError: argument is not a region subtag`);
shouldThrow(() => regionNames.of("0"), `RangeError: argument is not a region subtag`);
shouldThrow(() => regionNames.of("00"), `RangeError: argument is not a region subtag`);
shouldThrow(() => regionNames.of("0000"), `RangeError: argument is not a region subtag`);
shouldThrow(() => regionNames.of("a"), `RangeError: argument is not a region subtag`);
shouldThrow(() => regionNames.of("aaa"), `RangeError: argument is not a region subtag`);
shouldBe(regionNames.of("419"), `Latin America`);
shouldBe(regionNames.of("JP"), `Japan`);
shouldBe(regionNames.of("US"), `United States`);

var languageNames = new Intl.DisplayNames(['en'], {type: 'language'});
shouldThrow(() => languageNames.of(""), `RangeError: argument is not a language id`);
shouldThrow(() => languageNames.of("-"), `RangeError: argument is not a language id`);
shouldThrow(() => languageNames.of("--"), `RangeError: argument is not a language id`);
shouldThrow(() => languageNames.of("en-"), `RangeError: argument is not a language id`);
shouldBe(languageNames.of("en-US"), `American English`);
shouldThrow(() => languageNames.of("en_US"), `RangeError: argument is not a language id`);
shouldBe(languageNames.of("en-us"), `American English`);
shouldThrow(() => languageNames.of("root"), `RangeError: argument is not a language id`);
shouldThrow(() => languageNames.of("root-US"), `RangeError: argument is not a language id`);
shouldBe(languageNames.of("es-419"), `Latin American Spanish`);
if (languageNames.of('zh-Hant') !== `Chinese, Traditional`)
    shouldBe(languageNames.of('zh-Hant'), `Traditional Chinese`);
if (languageNames.of('zh-Hans-HK') !== `Chinese, Simplified (Hong Kong)` && languageNames.of('zh-Hans-HK') !== `Simplified Chinese (Hong Kong SAR China)`)
    shouldBeOneOf(languageNames.of('zh-Hans-HK'), [`Simplified Chinese (Hong Kong)`, `Chinese (Simplified, Hong Kong SAR China)`]);
shouldThrow(() => languageNames.of("Hant"), `RangeError: argument is not a language id`); // Script only
shouldBe(languageNames.of('sr-Latn'), `Serbian (Latin)`); // Language-Script
shouldBe(languageNames.of('sr-Cyrl'), `Serbian (Cyrillic)`); // Language-Script
shouldThrow(() => languageNames.of("zh-HK-Hans"), `RangeError: argument is not a language id`);
shouldBe(languageNames.of('sl-rozaj'), `Slovenian (Resian)`); // Language-Variant
shouldThrow(() => languageNames.of("sl_rozaj"), `RangeError: argument is not a language id`);
shouldThrow(() => languageNames.of("sl-rozaj-Cyrl"), `RangeError: argument is not a language id`);
shouldThrow(() => languageNames.of("sl-rozaj-"), `RangeError: argument is not a language id`);
shouldThrow(() => languageNames.of("sl-_rozaj"), `RangeError: argument is not a language id`);
shouldThrow(() => languageNames.of("sl-Cyrl-"), `RangeError: argument is not a language id`);
shouldThrow(() => languageNames.of("-sl-Cyrl-"), `RangeError: argument is not a language id`);
let latinItalyEasternAmerican = languageNames.of('hy-Latn-IT-arevela');
shouldBe(latinItalyEasternAmerican === `Armenian (Latin, Italy, Eastern Armenian)` || latinItalyEasternAmerican === `Armenian (Latin, Italy)`, true); // Language-Script-Region-Variant
shouldBe(languageNames.of('hy-Latn-IT'), `Armenian (Latin, Italy)`); // Language-Script-Region
let latinEasternAmerican = languageNames.of('hy-Latn-arevela');
shouldBe(latinEasternAmerican === `Armenian (Latin, Eastern Armenian)` || latinEasternAmerican === `Armenian (Latin)`, true); // Language-Script-Variant
shouldThrow(() => languageNames.of("hy-Latn-ITZ-arevela"), `RangeError: argument is not a language id`);
shouldThrow(() => languageNames.of("hy-Latn-00-arevela"), `RangeError: argument is not a language id`);
shouldThrow(() => languageNames.of("hy-Latn-arevelazzzzzzzzzzz"), `RangeError: argument is not a language id`);
shouldThrow(() => languageNames.of("hy-Latn-arevelazzzzzzzzzzz"), `RangeError: argument is not a language id`);
shouldThrow(() => languageNames.of("hyzzzzzzzzz-Latn-arevela"), `RangeError: argument is not a language id`);

var scriptNames = new Intl.DisplayNames(['en'], {type: 'script'});
shouldThrow(() => scriptNames.of(""), `RangeError: argument is not a script subtag`);
shouldThrow(() => scriptNames.of("-"), `RangeError: argument is not a script subtag`);
shouldThrow(() => scriptNames.of("Latn0"), `RangeError: argument is not a script subtag`);
shouldThrow(() => scriptNames.of("Lan0"), `RangeError: argument is not a script subtag`);
shouldThrow(() => scriptNames.of("Lan"), `RangeError: argument is not a script subtag`);
shouldBe(scriptNames.of("Latn"), `Latin`);
shouldBe(scriptNames.of("latn"), `Latin`); // Canonicalization can make it work.

var currencyNames = new Intl.DisplayNames(['en'], {type: 'currency'});
shouldThrow(() => currencyNames.of(""), `RangeError: argument is not a well-formed currency code`);
shouldThrow(() => currencyNames.of("-"), `RangeError: argument is not a well-formed currency code`);
shouldThrow(() => currencyNames.of("ab"), `RangeError: argument is not a well-formed currency code`);
shouldThrow(() => currencyNames.of("jpyx"), `RangeError: argument is not a well-formed currency code`);
shouldThrow(() => currencyNames.of("jp0"), `RangeError: argument is not a well-formed currency code`);
shouldBe(currencyNames.of("jpy"), `Japanese Yen`);
shouldBe(currencyNames.of("JPY"), `Japanese Yen`);
