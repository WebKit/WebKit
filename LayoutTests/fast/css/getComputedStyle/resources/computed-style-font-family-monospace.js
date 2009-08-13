description("Test that only exactly 'font-family: monospace;' causes use of fixed-width font default size.  All other font family and font family combinations should use the standard default size.")

var testSpan = document.createElement("span");
testSpan.innerHTML = "test";
document.body.appendChild(testSpan);

function fontSizeForFamilies(fontFamilies)
{
    testSpan.style.fontFamily = fontFamilies;
    return window.getComputedStyle(testSpan, null).fontSize;
}

// Only the exact font-family "monospace" should result in using the fixed-width font default size.
// This matches FireFox 3.x behavior.
shouldBeEqualToString("fontSizeForFamilies('monospace')", '13px');

// Everything else should use the standard font size:
shouldBeEqualToString("fontSizeForFamilies('monospace, times')", '16px');
shouldBeEqualToString("fontSizeForFamilies('times, monospace')", '16px');
shouldBeEqualToString("fontSizeForFamilies('foo')", '16px');
shouldBeEqualToString("fontSizeForFamilies('foo, monospace')", '16px');

document.body.removeChild(testSpan);

var successfullyParsed = true;
