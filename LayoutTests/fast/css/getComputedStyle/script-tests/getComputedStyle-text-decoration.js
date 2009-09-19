description("Test to make sure text-decoration property returns CSSValueList properly.")

var testContainer = document.createElement("div");
testContainer.contentEditable = true;
document.body.appendChild(testContainer);

function expect(desc, actual, expected)
{
    if (actual == expected)
        testPassed(desc);
    else
        testPassed(desc+" EXPECTED:"+expected+" ACTUAL:"+actual);
}

testContainer.innerHTML = '<div id="test" style="text-decoration: underline    \n line-through;">hello</div>';

e = document.getElementById('test');
expect('text decoration should be CSSValueList', e.style.getPropertyCSSValue('text-decoration'), "[object CSSValueList]");
expect('text decoration should be separated by a single space', e.style.textDecoration, "underline line-through");
computedStyle = window.getComputedStyle(e, null);
expect('computed style of text decoration should be CSSValueList', computedStyle.getPropertyCSSValue('text-decoration'), "[object CSSValueList]");
expect('computed style of text decoration should be separated by a single space', computedStyle.getPropertyCSSValue('text-decoration').cssText, "underline line-through");

document.body.removeChild(testContainer);

var successfullyParsed = true;
