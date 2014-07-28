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
    e.style.webkitTextJustify = value;
    testElementStyle("webkitTextJustify", "-webkit-text-justify", "[object CSSPrimitiveValue]", value);
    testComputedStyle("webkitTextJustify", "-webkit-text-justify", "[object CSSPrimitiveValue]", value);
    debug('');
}

function invalidValueSettingTest(value, defaultValue)
{
    debug("Invalid value test - '" + value + "':");
    e.style.webkitTextJustify = value;
    testElementStyle("webkitTextJustify", "-webkit-text-justify", "[object CSSPrimitiveValue]", defaultValue);
    testComputedStyle("webkitTextJustify", "-webkit-text-justify", "[object CSSPrimitiveValue]", defaultValue);
    debug('');
}

function computedValueSettingTest(value, defaultValue)
{
    debug("Computed value test - '" + value + "':");
    e.style.webkitTextJustify = value;
    testElementStyle("webkitTextJustify", "-webkit-text-justify", "[object CSSPrimitiveValue]", value);
    testComputedStyle("webkitTextJustify", "-webkit-text-justify", "[object CSSPrimitiveValue]", defaultValue);
    debug('');
}

description("This test checks that -webkit-text-justify parses properly the properties from CSS 3 Text.");

e = document.getElementById('test');

debug("Test the initial value:");
testComputedStyle("webkitTextJustify", "-webkit-text-justify", "[object CSSPrimitiveValue]", 'auto');
debug('');

valueSettingTest('auto');
valueSettingTest('none');
valueSettingTest('inter-word');
valueSettingTest('distribute');

defaultValue = 'auto'
e.style.webkitTextJustify = defaultValue;
invalidValueSettingTest('-webkit-left', defaultValue);
invalidValueSettingTest('-webkit-right', defaultValue);
invalidValueSettingTest('-webkit-center', defaultValue);
invalidValueSettingTest('-webkit-match-parent', defaultValue);
invalidValueSettingTest('-webkit-auto', defaultValue);
invalidValueSettingTest('solid', defaultValue);
invalidValueSettingTest('normal', defaultValue);
invalidValueSettingTest('bold', defaultValue);
invalidValueSettingTest('background', defaultValue);
invalidValueSettingTest('ltr', defaultValue);
invalidValueSettingTest('inter-ideograph', defaultValue);
invalidValueSettingTest('inter-cluster', defaultValue);
invalidValueSettingTest('kashida', defaultValue);

computedValueSettingTest('inherit', 'auto');
computedValueSettingTest('initial', 'auto');
