description("A copy of the corresponding W3C-SVG-1.1 test, which dumps the animation at certain times");
embedSVGTestCase("../W3C-SVG-1.1/animate-elem-03-t.svg");

// Setup animation test
function sample1() {
    shouldBeEqualToString("getComputedStyle(text).fontSize", "40px");
    shouldBeEqualToString("getComputedStyle(text).fill", "#0000ff");
}

function sample2() {
    shouldBeEqualToString("getComputedStyle(text).fontSize", "60px");
    shouldBeEqualToString("getComputedStyle(text).fill", "#003b80");
}

function sample3() {
    shouldBeEqualToString("getComputedStyle(text).fontSize", "80px");
    shouldBeEqualToString("getComputedStyle(text).fill", "#007700");
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
