function testComputedStyle(a_value, c_value)
{
    shouldBe("window.getComputedStyle(ancestor).getPropertyCSSValue('-webkit-text-align-last').cssText",  "'" + a_value + "'");
    shouldBe("window.getComputedStyle(child).getPropertyCSSValue('-webkit-text-align-last').cssText",  "'" + c_value + "'");
    debug('');
}

function ownValueTest(a_value, c_value)
{
    debug("Value of ancestor is '" + a_value + ", while child is '" + c_value + "':");
    ancestor.style.webkitTextAlignLast = a_value;
    child.style.webkitTextAlignLast = c_value;
    testComputedStyle(a_value, c_value);
}

function inheritanceTest(a_value)
{
    debug("Value of ancestor is '" + a_value + "':");
    ancestor.style.webkitTextAlignLast = a_value;
    testComputedStyle(a_value, a_value);
}

description("This test checks that the value of -webkit-text-align-last is properly inherited to the child.");

ancestor = document.getElementById('ancestor');
child = document.getElementById('child');

inheritanceTest("start");
inheritanceTest("end");
inheritanceTest("left");
inheritanceTest("right");
inheritanceTest("center");
inheritanceTest("justify");
inheritanceTest("auto");

ownValueTest("start", "end");
ownValueTest("left", "right");
