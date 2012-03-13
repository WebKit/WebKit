description("A copy of the corresponding W3C-SVG-1.1 test, which dumps the animation at certain times");
embedSVGTestCase("../W3C-SVG-1.1/animate-elem-02-t.svg");

// Setup animation test
function sampleAfterBegin() {
    shouldBe("rect1.height.animVal.value", "200");
    shouldBe("rect1.height.baseVal.value", "20");

    shouldBe("rect2.height.animVal.value", "220");
    shouldBe("rect2.height.baseVal.value", "20");

    shouldBe("rect3.height.animVal.value", "200");
    shouldBe("rect3.height.baseVal.value", "20");
}

function sampleAfterMid() {
    shouldBe("rect1.height.animVal.value", "20");
    shouldBe("rect1.height.baseVal.value", "20");

    shouldBe("rect2.height.animVal.value", "40");
    shouldBe("rect2.height.baseVal.value", "20");

    shouldBe("rect3.height.animVal.value", "20");
    shouldBe("rect3.height.baseVal.value", "20");
}

function sampleAfterBeginOfFirstRepetition() {
    shouldBe("rect1.height.animVal.value", "200");
    shouldBe("rect1.height.baseVal.value", "20");

    shouldBe("rect2.height.animVal.value", "220");
    shouldBe("rect2.height.baseVal.value", "20");

    shouldBe("rect3.height.animVal.value", "220");
    shouldBe("rect3.height.baseVal.value", "20");
}

function sampleAfterMidOfFirstRepetition() {
    shouldBe("rect1.height.animVal.value", "20");
    shouldBe("rect1.height.baseVal.value", "20");

    shouldBe("rect2.height.animVal.value", "40");
    shouldBe("rect2.height.baseVal.value", "20");

    shouldBe("rect3.height.animVal.value", "40");
    shouldBe("rect3.height.baseVal.value", "20");
}

function executeTest() {
    rects = rootSVGElement.ownerDocument.getElementsByTagName("rect");
    rect1 = rects[1]; 
    rect2 = rects[3]; 
    rect3 = rects[5]; 
    rect4 = rects[7]; 

    // All animations in the test file use the same duration, so it's not needed to list all sample points individually for an5/an6/an7/an8.
    const expectedValues = [
        // [animationId, time, sampleCallback]
        ["an5", 0.0,   sampleAfterBegin],
        ["an5", 1.999, sampleAfterBegin],
        ["an5", 2.0,   sampleAfterMid],
        ["an5", 3.999, sampleAfterMid],
        ["an5", 4.0,   sampleAfterBeginOfFirstRepetition],
        ["an5", 5.999, sampleAfterBeginOfFirstRepetition],
        ["an5", 6.001, sampleAfterMidOfFirstRepetition],
        ["an5", 7.999, sampleAfterMidOfFirstRepetition],
        ["an5", 8.001, sampleAfterMidOfFirstRepetition],
        ["an5", 60.0,  sampleAfterMidOfFirstRepetition]
    ];

    runAnimationTest(expectedValues);
}

window.animationStartsImmediately = true;
var successfullyParsed = true;
