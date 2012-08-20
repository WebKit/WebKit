function testElementStyle(propertyJS, propertyCSS, type, value)
{
    if (type != null) {
        shouldBe("e.style." + propertyJS, "'" + value + "'");
        shouldBe("e.style.getPropertyCSSValue('" + propertyCSS + "').toString()", "'" + type + "'");
        shouldBe("e.style.getPropertyCSSValue('" + propertyCSS + "').cssText", "'" + value + "'");
    } else
        shouldBeNull("e.style.getPropertyCSSValue('" + propertyCSS + "')");
}

function testComputedStyle(propertyJS, propertyCSS, type, value)
{
    computedStyle = window.getComputedStyle(e, null);
    shouldBe("computedStyle." + propertyJS, "'" + value + "'");
    shouldBe("computedStyle.getPropertyCSSValue('" + propertyCSS + "').toString()", "'" + type + "'");
    shouldBe("computedStyle.getPropertyCSSValue('" + propertyCSS + "').cssText", "'" + value + "'");
}

description("Test to make sure -webkit-text-decoration-line property returns values properly.")

var testContainer = document.createElement("div");
testContainer.contentEditable = true;
document.body.appendChild(testContainer);

testContainer.innerHTML = '<div id="test">hello world</div>';
debug("Initial value:");
e = document.getElementById('test');
testElementStyle("webkitTextDecorationLine", "-webkit-text-decoration-line", null, '');
testComputedStyle("webkitTextDecorationLine", "-webkit-text-decoration-line", "[object CSSPrimitiveValue]", "none");
debug('');

debug("Initial value (explicit):");
e.style.webkitTextDecorationLine = 'initial';
testElementStyle("webkitTextDecorationLine", "-webkit-text-decoration-line", "[object CSSValue]", "initial");
testComputedStyle("webkitTextDecorationLine", "-webkit-text-decoration-line", "[object CSSPrimitiveValue]", "none");
debug('');

debug("Value 'none':");
e.style.webkitTextDecorationLine = 'none';
testElementStyle("webkitTextDecorationLine", "-webkit-text-decoration-line", "[object CSSValue]", "initial");
testComputedStyle("webkitTextDecorationLine", "-webkit-text-decoration-line", "[object CSSPrimitiveValue]", "none");
debug('');

debug("Value 'underline':");
e.style.webkitTextDecorationLine = 'underline';
testElementStyle("webkitTextDecorationLine", "-webkit-text-decoration-line", "[object CSSValueList]", "underline");
testComputedStyle("webkitTextDecorationLine", "-webkit-text-decoration-line", "[object CSSValueList]", "underline");
debug('');

debug("Value 'overline':");
e.style.webkitTextDecorationLine = 'overline';
testElementStyle("webkitTextDecorationLine", "-webkit-text-decoration-line", "[object CSSValueList]", "overline");
testComputedStyle("webkitTextDecorationLine", "-webkit-text-decoration-line", "[object CSSValueList]", "overline");
debug('');

debug("Value 'line-through':");
e.style.webkitTextDecorationLine = 'line-through';
testElementStyle("webkitTextDecorationLine", "-webkit-text-decoration-line", "[object CSSValueList]", "line-through");
testComputedStyle("webkitTextDecorationLine", "-webkit-text-decoration-line", "[object CSSValueList]", "line-through");
debug('');

debug("Value 'underline overline line-through':");
e.style.webkitTextDecorationLine = 'underline overline line-through';
testElementStyle("webkitTextDecorationLine", "-webkit-text-decoration-line", "[object CSSValueList]", "underline overline line-through");
testComputedStyle("webkitTextDecorationLine", "-webkit-text-decoration-line", "[object CSSValueList]", "underline overline line-through");
debug('');

debug("Value 'blink' (invalid, last valid value is used):");
e.style.webkitTextDecorationLine = 'blink';
testElementStyle("webkitTextDecorationLine", "-webkit-text-decoration-line", "[object CSSValueList]", "underline overline line-through");
testComputedStyle("webkitTextDecorationLine", "-webkit-text-decoration-line", "[object CSSValueList]", "underline overline line-through");
debug('');

debug("Value '':");
e.style.webkitTextDecorationLine = '';
testElementStyle("webkitTextDecorationLine", "-webkit-text-decoration-line", null, '');
testComputedStyle("webkitTextDecorationLine", "-webkit-text-decoration-line", "[object CSSPrimitiveValue]", "none");
debug('');

testContainer.innerHTML = '<div id="test-parent" style="-webkit-text-decoration-line: underline;">hello <span id="test-ancestor" style="-webkit-text-decoration-line: inherit;">world</span></div>';
debug("Parent gets 'underline' value:");
e = document.getElementById('test-parent');
testElementStyle("webkitTextDecorationLine", "-webkit-text-decoration-line", "[object CSSValueList]", "underline");
testComputedStyle("webkitTextDecorationLine", "-webkit-text-decoration-line", "[object CSSValueList]", "underline");
debug('');

debug("Ancestor should explicitly inherit value from parent when 'inherit' value is used:");
e = document.getElementById('test-ancestor');
testElementStyle("webkitTextDecorationLine", "-webkit-text-decoration-line", "[object CSSValue]", "inherit");
testComputedStyle("webkitTextDecorationLine", "-webkit-text-decoration-line", "[object CSSValueList]", "underline");
debug('');

document.body.removeChild(testContainer);
