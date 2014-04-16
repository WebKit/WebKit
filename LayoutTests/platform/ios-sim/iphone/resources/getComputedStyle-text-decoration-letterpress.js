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

description("Test for custom letterpress text-decoration style.")

var testContainer = document.createElement("div");
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

debug("Value '-webkit-letterpress':");
e.style.textDecoration = '-webkit-letterpress';
testElementStyle("textDecoration", "text-decoration", "[object CSSValueList]", "-webkit-letterpress");
testComputedStyle("textDecoration", "text-decoration", "[object CSSValueList]", "-webkit-letterpress");
debug('');

debug("Value 'underline -webkit-letterpress line-through':");
e.style.textDecoration = 'underline -webkit-letterpress line-through';
testElementStyle("textDecoration", "text-decoration", "[object CSSValueList]", "underline -webkit-letterpress line-through");
testComputedStyle("textDecoration", "text-decoration", "[object CSSValueList]", "underline line-through -webkit-letterpress");
debug('');

document.body.removeChild(testContainer);
