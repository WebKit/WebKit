function testElementStyle(type, value)
{
    if (type != null) {
        shouldBe("e.style.webkitTextDecorationStyle", "'" + value + "'");
        shouldBe("e.style.getPropertyCSSValue('-webkit-text-decoration-style').toString()", "'" + type + "'");
        shouldBe("e.style.getPropertyCSSValue('-webkit-text-decoration-style').cssText", "'" + value + "'");
    } else
        shouldBeNull("e.style.getPropertyCSSValue('-webkit-text-decoration-style')");
}

function testComputedStyleValue(type, value)
{
    computedStyle = window.getComputedStyle(e, null);
    shouldBe("computedStyle.getPropertyCSSValue('-webkit-text-decoration-style').toString()", "'" + type + "'");
    shouldBe("computedStyle.getPropertyCSSValue('-webkit-text-decoration-style').cssText", "'" + value + "'");
    shouldBe("computedStyle.webkitTextDecorationStyle", "'" + value + "'");
}

function testValue(value, elementValue, elementStyle, computedValue, computedStyle)
{
    if (value != null)
        e.style.webkitTextDecorationStyle = value;
    testElementStyle(elementStyle, elementValue);
    testComputedStyleValue(computedStyle, computedValue);
    debug('');
}

description("Test to make sure -webkit-text-decoration-style property returns CSSPrimitiveValue properly.")

var testContainer = document.createElement("div");
testContainer.contentEditable = true;
document.body.appendChild(testContainer);

testContainer.innerHTML = '<div id="test-parent" style="-webkit-text-decoration-style: dashed !important;">hello <span id="test-ancestor">world</span></div>';
debug("Ancestor should not inherit 'dashed' value from parent (fallback to initial 'solid' value):")
e = document.getElementById('test-ancestor');
testValue(null, "", null, "solid", "[object CSSPrimitiveValue]");

debug("Parent should cointain 'dashed':");
e = document.getElementById('test-parent');
testValue(null, "dashed", "[object CSSPrimitiveValue]", "dashed", "[object CSSPrimitiveValue]");

testContainer.innerHTML = '<div id="test-js">test</div>';
debug("JavaScript setter tests for valid, initial, invalid and blank values:");
e = document.getElementById('test-js');
shouldBeNull("e.style.getPropertyCSSValue('-webkit-text-decoration-style')");

debug("\nValid value 'solid':");
testValue("solid", "solid", "[object CSSPrimitiveValue]", "solid", "[object CSSPrimitiveValue]");

debug("Valid value 'double':");
testValue("double", "double", "[object CSSPrimitiveValue]", "double", "[object CSSPrimitiveValue]");

debug("Valid value 'dotted':");
testValue("dotted", "dotted", "[object CSSPrimitiveValue]", "dotted", "[object CSSPrimitiveValue]");

debug("Valid value 'dashed':");
testValue("dashed", "dashed", "[object CSSPrimitiveValue]", "dashed", "[object CSSPrimitiveValue]");

debug("Valid value 'wavy':");
testValue("wavy", "wavy", "[object CSSPrimitiveValue]", "wavy", "[object CSSPrimitiveValue]");

debug("Initial value:");
testValue("initial", "initial", "[object CSSValue]", "solid", "[object CSSPrimitiveValue]");

debug("Invalid value (this property accepts one value at a time only):");
testValue("double dotted", "initial", "[object CSSValue]", "solid", "[object CSSPrimitiveValue]");

debug("Invalid value (ie. 'unknown'):");
testValue("unknown", "initial", "[object CSSValue]", "solid", "[object CSSPrimitiveValue]");

debug("Empty value (resets the property):");
testValue("", "", null, "solid", "[object CSSPrimitiveValue]");

document.body.removeChild(testContainer);
