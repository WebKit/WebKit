description("Test that the CSS property 'line-height' is not applied to ruby base and annotation texts.");

function getLineHeight(id)
{
    var element = document.getElementById(id);
    shouldBeNonNull(element);
    var lineHeight = window.getComputedStyle(element, null).getPropertyValue("line-height");
    return lineHeight;
}

var div = document.createElement("div");
div.innerHTML = "<p style='line-height: 300%' id='p'>The line height of this is <ruby id='r'>three times normal<rt id='t'>&quot;line-height: 48px;&quot;</rt></ruby>, but the ruby should have 'line-height: normal'.</p>";
document.body.appendChild(div);

shouldBeEqualToString("getLineHeight('p')", "48px");
shouldBeEqualToString("getLineHeight('r')", "normal");
shouldBeEqualToString("getLineHeight('t')", "normal");
