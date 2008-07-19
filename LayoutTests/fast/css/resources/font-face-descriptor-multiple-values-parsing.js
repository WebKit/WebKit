description("This tests that the font-weight, font-style and font-variant properties accept value lists when they appear inside @font-face rules, and that the value 'all' is allowed only by itself.");

function test(property, value)
{
    var style = document.createElement("style");
    style.appendChild(document.createTextNode("@font-face{" + property + ":" + value + ";}"));
    var head = document.getElementsByTagName("head")[0];
    head.appendChild(style);
    var result = style.sheet.rules[0].style.getPropertyCSSValue(property).cssText;
    head.removeChild(style);
    return result;
}

shouldBe('test("font-weight", "100")', '"100"');
shouldBe('test("font-weight", "all")', '"all"');
shouldBe('test("font-weight", "100, 200")', '"100, 200"');
shouldBe('test("font-weight", "bold, normal")', '"bold, normal"');
shouldBe('test("font-weight", "100, 200, 300, 400, 500, 600, 700, 100")', '"100, 200, 300, 400, 500, 600, 700, 100"');
shouldThrow('test("font-weight", "all, 100")', '"TypeError: Result of expression \'style.sheet.rules[0].style.getPropertyCSSValue(property)\' [null] is not an object."');
shouldThrow('test("font-weight", "bold, normal, all")', '"TypeError: Result of expression \'style.sheet.rules[0].style.getPropertyCSSValue(property)\' [null] is not an object."');
shouldThrow('test("font-weight", "")', '"TypeError: Result of expression \'style.sheet.rules[0].style.getPropertyCSSValue(property)\' [null] is not an object."');

shouldBe('test("font-style", "normal")', '"normal"');
shouldBe('test("font-style", "italic")', '"italic"');
shouldBe('test("font-style", "normal, oblique")', '"normal, oblique"');
shouldBe('test("font-style", "all")', '"all"');
shouldThrow('test("font-style", "all, normal")', '"TypeError: Result of expression \'style.sheet.rules[0].style.getPropertyCSSValue(property)\' [null] is not an object."');
shouldThrow('test("font-style", "italic, all")', '"TypeError: Result of expression \'style.sheet.rules[0].style.getPropertyCSSValue(property)\' [null] is not an object."');
shouldThrow('test("font-style", "")', '"TypeError: Result of expression \'style.sheet.rules[0].style.getPropertyCSSValue(property)\' [null] is not an object."');

shouldBe('test("font-variant", "normal")', '"normal"');
shouldBe('test("font-variant", "small-caps")', '"small-caps"');
shouldBe('test("font-variant", "normal, small-caps")', '"normal, small-caps"');
shouldBe('test("font-variant", "all")', '"all"');
shouldThrow('test("font-variant", "all, normal")', '"TypeError: Result of expression \'style.sheet.rules[0].style.getPropertyCSSValue(property)\' [null] is not an object."');
shouldThrow('test("font-variant", "small-caps, all")', '"TypeError: Result of expression \'style.sheet.rules[0].style.getPropertyCSSValue(property)\' [null] is not an object."');
shouldThrow('test("font-variant", "")', '"TypeError: Result of expression \'style.sheet.rules[0].style.getPropertyCSSValue(property)\' [null] is not an object."');

var successfullyParsed = true;
