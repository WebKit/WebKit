function testElementStyle(propertyJS, propertyCSS, type, value)
{
    shouldBe("e.style." + propertyJS, "'" + value + "'");
    shouldBe("e.style.getPropertyCSSValue('" + propertyCSS + "').cssText", "'" + value + "'");
}

function testComputedStyle(propertyJS, propertyCSS, type, value)
{
    computedStyle = window.getComputedStyle(e, null);
    shouldBe("computedStyle." + propertyJS, "'" + value + "'");
    shouldBe("computedStyle.getPropertyCSSValue('" + propertyCSS + "').cssText", "'" + value + "'");
}

function valueSettingTest(value)
{
    debug("Value '" + value + "':");
    e.style.webkitTextAlignLast = value;
    testElementStyle("webkitTextAlignLast", "-webkit-text-align-last", "[object CSSPrimitiveValue]", value);
    testComputedStyle("webkitTextAlignLast", "-webkit-text-align-last", "[object CSSPrimitiveValue]", value);
    debug('');
}

function invalidValueSettingTest(value, defaultValue)
{
    debug("Invalid value test - '" + value + "':");
    e.style.webkitTextAlignLast = value;
    testElementStyle("webkitTextAlignLast", "-webkit-text-align-last", "[object CSSPrimitiveValue]", defaultValue);
    testComputedStyle("webkitTextAlignLast", "-webkit-text-align-last", "[object CSSPrimitiveValue]", defaultValue);
    debug('');
}

description("This test checks that -webkit-text-align-last parses properly the properties from CSS 3 Text.");

e = document.getElementById('test');

debug("Test the initial value:");
testComputedStyle("webkitTextAlignLast", "-webkit-text-align-last", "[object CSSPrimitiveValue]", 'auto');
debug('');

valueSettingTest('start');
valueSettingTest('end');
valueSettingTest('left');
valueSettingTest('right');
valueSettingTest('center');
valueSettingTest('justify');
valueSettingTest('auto');

defaultValue = 'auto'
e.style.webkitTextAlignLast = defaultValue;
invalidValueSettingTest('-webkit-left', defaultValue);
invalidValueSettingTest('-webkit-right', defaultValue);
invalidValueSettingTest('-webkit-center', defaultValue);
invalidValueSettingTest('-webkit-match-parent', defaultValue);
invalidValueSettingTest('-webkit-auto', defaultValue);
