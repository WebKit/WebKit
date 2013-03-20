function testElementStyle(propertyJS, propertyCSS, value)
{
    shouldBe("e.style." + propertyJS, "'" + value + "'");
    shouldBe("e.style.getPropertyValue('" + propertyCSS + "')", "'" + value + "'");
}

function testComputedStyle(propertyJS, propertyCSS, value)
{
    computedStyle = window.getComputedStyle(e, null);
    shouldBe("computedStyle." + propertyJS, "'" + value + "'");
    shouldBe("computedStyle.getPropertyValue('" + propertyCSS + "')", "'" + value + "'");
}

function valueSettingTest(value, expectedValue, computedValue)
{
    debug("Value '" + value + "':");
    e.style.textIndent = value;
    testElementStyle("textIndent", "text-indent", expectedValue);
    testComputedStyle("textIndent", "text-indent", computedValue);
    debug('');
}

function invalidValueSettingTest(value, defaultValue)
{
    debug("Invalid value test - '" + value + "':");
    e.style.textIndent = value;
    testElementStyle("textIndent", "text-indent", defaultValue);
    testComputedStyle("textIndent", "text-indent", defaultValue);
    debug('');
}

description("This tests check that text-indent parses properly the properties from CSS3 Text.");

e = document.getElementById('test');

debug("Test the initial value:");
testComputedStyle("textIndent", "text-indent", '0px');
debug('');

valueSettingTest('10em', '10em', '100px');
valueSettingTest('20ex', '20ex', '200px');
valueSettingTest('50%', '50%', '50%');
valueSettingTest('calc(10px + 20px)', 'calc(10px + 20px)', '30px');
valueSettingTest('10em -webkit-each-line', '10em -webkit-each-line', '100px -webkit-each-line');
valueSettingTest('-webkit-each-line 10em', '10em -webkit-each-line', '100px -webkit-each-line');
valueSettingTest('20ex -webkit-each-line', '20ex -webkit-each-line', '200px -webkit-each-line');
valueSettingTest('-webkit-each-line 20ex', '20ex -webkit-each-line', '200px -webkit-each-line');
valueSettingTest('30% -webkit-each-line', '30% -webkit-each-line', '30% -webkit-each-line');
valueSettingTest('-webkit-each-line 30%', '30% -webkit-each-line', '30% -webkit-each-line');
valueSettingTest('calc(10px + 20px) -webkit-each-line', 'calc(10px + 20px) -webkit-each-line', '30px -webkit-each-line');
valueSettingTest('-webkit-each-line calc(10px + 20px)', 'calc(10px + 20px) -webkit-each-line', '30px -webkit-each-line');
debug('');

defaultValue = '0px'
e.style.textIndent = defaultValue;
invalidValueSettingTest('10m', defaultValue);
invalidValueSettingTest('10m -webkit-each-line', defaultValue);
invalidValueSettingTest('-webkit-each-line 10m', defaultValue);
invalidValueSettingTest('10em 10em', defaultValue);
invalidValueSettingTest('-webkit-each-line', defaultValue);
invalidValueSettingTest('10em -webkit-each-line 10em', defaultValue);
invalidValueSettingTest('-webkit-each-line 10em -webkit-each-line', defaultValue);
