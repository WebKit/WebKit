description("A copy of the corresponding W3C-SVG-1.1 test, which dumps the animation at certain times");
embedSVGTestCase("../W3C-SVG-1.1/animate-elem-03-t.svg");

function parseFontSizeFromString() {
    fontSizeString = getComputedStyle(text).fontSize;
    return fontSizeString.replace(/px$/, "");
}

// Setup animation test
function sample1() {
    shouldBeCloseEnough("parseFontSizeFromString()", "40");
    expectFillColor(text, 0, 0, 255);
}

function sample2() {
    shouldBeCloseEnough("parseFontSizeFromString()", "60");
    expectFillColor(text, 0, 59, 128);
}

function sample3() {
    shouldBeCloseEnough("parseFontSizeFromString()", "80");
    expectFillColor(text, 0, 119, 0);
}

function executeTest() {
    text = rootSVGElement.ownerDocument.getElementsByTagName("text")[2];

    const expectedValues = [
        // [animationId, time, sampleCallback]
        ["an1", 0.0,  sample1],
        ["an1", 3.0,  sample2],
        ["an1", 6.0,  sample3],
        ["an1", 60.0, sample3]
    ];

    runAnimationTest(expectedValues);
}

window.animationStartsImmediately = true;
var successfullyParsed = true;
