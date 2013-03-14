function testElementStyle(type, value)
{
    if (type != null) {
        shouldBe("e.style.webkitTextDecorationColor", "'" + value + "'");
        shouldBe("e.style.getPropertyCSSValue('-webkit-text-decoration-color').toString()", "'" + type + "'");
        shouldBe("e.style.getPropertyCSSValue('-webkit-text-decoration-color').cssText", "'" + value + "'");
    } else
        shouldBeNull("e.style.getPropertyCSSValue('-webkit-text-decoration-color')");
}

function testComputedStyleValue(type, value)
{
    computedStyle = window.getComputedStyle(e, null);
    shouldBe("computedStyle.getPropertyCSSValue('-webkit-text-decoration-color').toString()", "'" + type + "'");
    shouldBe("computedStyle.getPropertyCSSValue('-webkit-text-decoration-color').cssText", "'" + value + "'");
    shouldBe("computedStyle.webkitTextDecorationColor", "'" + value + "'");
}

function testValue(value, elementValue, elementStyle, computedValue, computedStyle)
{
    if (value != null)
        e.style.webkitTextDecorationColor = value;
    testElementStyle(elementStyle, elementValue);
    testComputedStyleValue(computedStyle, computedValue);
    debug('');
}

description("Test to make sure -webkit-text-decoration-color property returns CSSPrimitiveValue properly.")

var testContainer = document.createElement("div");
testContainer.contentEditable = true;
document.body.appendChild(testContainer);

testContainer.innerHTML = '<div id="test-parent" style="-webkit-text-decoration-color: green !important;">hello <span id="test-ancestor">world</span></div>';
debug("Ancestor should not inherit 'green' value from parent (fallback to initial value):")
e = document.getElementById('test-ancestor');
testValue(null, "", null, "rgb(0, 0, 0)", "[object CSSPrimitiveValue]");

debug("Parent should contain 'green':");
e = document.getElementById('test-parent');
testValue(null, "green", "[object CSSPrimitiveValue]", "rgb(0, 128, 0)", "[object CSSPrimitiveValue]");

testContainer.innerHTML = '<div id="test-js">test</div>';
debug("JavaScript setter tests for valid, initial, invalid and blank values:");
e = document.getElementById('test-js');
shouldBeNull("e.style.getPropertyCSSValue('-webkit-text-decoration-color')");

debug("\nValid value 'blue':");
testValue("blue", "blue", "[object CSSPrimitiveValue]", "rgb(0, 0, 255)", "[object CSSPrimitiveValue]");

debug("Valid value '#FFFFFF':");
testValue("#FFFFFF", "rgb(255, 255, 255)", "[object CSSPrimitiveValue]", "rgb(255, 255, 255)", "[object CSSPrimitiveValue]");

debug("Valid value 'rgb(0, 255, 0)':");
testValue("rgb(0, 255, 0)", "rgb(0, 255, 0)", "[object CSSPrimitiveValue]", "rgb(0, 255, 0)", "[object CSSPrimitiveValue]");

debug("Valid value 'rgba(100, 100, 100, 0.5)':");
testValue("rgba(100, 100, 100, 0.5)", "rgba(100, 100, 100, 0.498039)", "[object CSSPrimitiveValue]", "rgba(100, 100, 100, 0.498039)", "[object CSSPrimitiveValue]");

debug("Valid value 'hsl(240, 100%, 50%)':");
testValue("hsl(240, 100%, 50%)", "rgb(0, 0, 255)", "[object CSSPrimitiveValue]", "rgb(0, 0, 255)", "[object CSSPrimitiveValue]");

debug("Valid value 'hsla(240, 100%, 50%, 0.5)':");
testValue("hsla(240, 100%, 50%, 0.5)", "rgba(0, 0, 255, 0.498039)", "[object CSSPrimitiveValue]", "rgba(0, 0, 255, 0.498039)", "[object CSSPrimitiveValue]");

debug("Initial value:");
testValue("initial", "initial", "[object CSSValue]", "rgb(0, 0, 0)", "[object CSSPrimitiveValue]");

debug("Invalid value (ie. 'unknown'):");
testValue("unknown", "initial", "[object CSSValue]", "rgb(0, 0, 0)", "[object CSSPrimitiveValue]");

debug("Empty value (resets the property):");
testValue("", "", null, "rgb(0, 0, 0)", "[object CSSPrimitiveValue]");

debug("Empty value with different 'currentColor' initial value (green):")
e.style.color = 'green';
testValue("", "", null, "rgb(0, 128, 0)", "[object CSSPrimitiveValue]");

document.body.removeChild(testContainer);
