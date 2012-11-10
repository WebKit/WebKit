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

description("Test to make sure text-decoration property returns values properly.")

var testContainer = document.createElement("div");
testContainer.contentEditable = true;
document.body.appendChild(testContainer);

testContainer.innerHTML = '<div id="test">hello world</div>';
debug("Initial value:");
e = document.getElementById('test');
testElementStyle("textDecoration", "text-decoration", null, '');
testComputedStyle("textDecoration", "text-decoration", "[object CSSPrimitiveValue]", "none");
debug('');

debug("Initial value (explicit):");
e.style.textDecoration = 'initial';
testElementStyle("textDecoration", "text-decoration", "[object CSSValue]", "initial");
testComputedStyle("textDecoration", "text-decoration", "[object CSSPrimitiveValue]", "none");
debug('');

debug("Value 'none':");
e.style.textDecoration = 'none';
testElementStyle("textDecoration", "text-decoration", "[object CSSPrimitiveValue]", "none");
testComputedStyle("textDecoration", "text-decoration", "[object CSSPrimitiveValue]", "none");
debug('');

debug("Value 'underline':");
e.style.textDecoration = 'underline';
testElementStyle("textDecoration", "text-decoration", "[object CSSValueList]", "underline");
testComputedStyle("textDecoration", "text-decoration", "[object CSSValueList]", "underline");
debug('');

debug("Value 'overline':");
e.style.textDecoration = 'overline';
testElementStyle("textDecoration", "text-decoration", "[object CSSValueList]", "overline");
testComputedStyle("textDecoration", "text-decoration", "[object CSSValueList]", "overline");
debug('');

debug("Value 'line-through':");
e.style.textDecoration = 'line-through';
testElementStyle("textDecoration", "text-decoration", "[object CSSValueList]", "line-through");
testComputedStyle("textDecoration", "text-decoration", "[object CSSValueList]", "line-through");
debug('');

debug("Value 'underline overline line-through':");
e.style.textDecoration = 'underline overline line-through';
testElementStyle("textDecoration", "text-decoration", "[object CSSValueList]", "underline overline line-through");
testComputedStyle("textDecoration", "text-decoration", "[object CSSValueList]", "underline overline line-through");
debug('');

debug("Value 'blink' (valid but ignored):");
e.style.textDecoration = 'blink';
testElementStyle("textDecoration", "text-decoration", "[object CSSValueList]", "blink");
testComputedStyle("textDecoration", "text-decoration", "[object CSSPrimitiveValue]", "none");
debug('');

debug("Value '':");
e.style.textDecoration = '';
testElementStyle("textDecoration", "text-decoration", null, '');
testComputedStyle("textDecoration", "text-decoration", "[object CSSPrimitiveValue]", "none");
debug('');

testContainer.innerHTML = '<div id="test-parent" style="text-decoration: underline;">hello <span id="test-ancestor" style="text-decoration: inherit;">world</span></div>';
debug("Parent gets 'underline' value:");
e = document.getElementById('test-parent');
testElementStyle("textDecoration", "text-decoration", "[object CSSValueList]", "underline");
testComputedStyle("textDecoration", "text-decoration", "[object CSSValueList]", "underline");
debug('');

debug("Ancestor should explicitly inherit value from parent when 'inherit' value is used:");
e = document.getElementById('test-ancestor');
testElementStyle("textDecoration", "text-decoration", "[object CSSValue]", "inherit");
testComputedStyle("textDecoration", "text-decoration", "[object CSSValueList]", "underline");
debug('');

document.body.removeChild(testContainer);
