description("A copy of the corresponding W3C-SVG-1.1 test, which dumps the animation at certain times");
embedSVGTestCase("../W3C-SVG-1.1/animate-elem-13-t.svg");

// Setup animation test
function sample1() {
    shouldBeCloseEnough("rect1.width.animVal.value", "30");
    shouldBe("rect1.width.baseVal.value", "30");
}

function sample2() {
    shouldBeCloseEnough("rect1.width.animVal.value", "165");
    shouldBe("rect1.width.baseVal.value", "30");
}

function sample3() {
    shouldBeCloseEnough("rect1.width.animVal.value", "300");
    shouldBe("rect1.width.baseVal.value", "30");
}

function executeTest() {
    var rects = rootSVGElement.ownerDocument.getElementsByTagName("rect");
    rect1 = rects[0];
    rect2 = rects[1];
    rect3 = rects[2];
    rect4 = rects[3];
    rect5 = rects[4];
    rect6 = rects[5];

    const expectedValues = [
        // [animationId, time, sampleCallback]
        ["an1", 0.0,  sample1],
        ["an1", 1.5,  sample2],
        ["an1", 3.0,  sample3],
        ["an1", 60.0, sample3]
    ];

    runAnimationTest(expectedValues);
}

window.animationStartsImmediately = true;
var successfullyParsed = true;
