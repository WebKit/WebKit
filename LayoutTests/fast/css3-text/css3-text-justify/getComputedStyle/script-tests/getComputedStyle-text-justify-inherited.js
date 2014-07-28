function testComputedStyle(a_value, c_value)
{
    shouldBe("window.getComputedStyle(ancestor).getPropertyCSSValue('-webkit-text-justify').cssText",  "'" + a_value + "'");
    shouldBe("window.getComputedStyle(child).getPropertyCSSValue('-webkit-text-justify').cssText",  "'" + c_value + "'");
    debug('');
}

function ownValueTest(a_value, c_value)
{
    debug("Value of ancestor is '" + a_value + ", while child is '" + c_value + "':");
    ancestor.style.webkitTextJustify = a_value;
    child.style.webkitTextJustify = c_value;
    testComputedStyle(a_value, c_value);
}

function inheritanceTest(a_value)
{
    debug("Value of ancestor is '" + a_value + "':");
    ancestor.style.webkitTextJustify = a_value;
    testComputedStyle(a_value, a_value);
}

description("This test checks that the value of -webkit-text-justify is properly inherited to the child.");

ancestor = document.getElementById('ancestor');
child = document.getElementById('child');

inheritanceTest("auto");
inheritanceTest("none");
inheritanceTest("inter-word");
inheritanceTest("distribute");

ownValueTest("inter-word", "distribute");
ownValueTest("distribute", "none");
