description("This tests scripting baseVal while animation is running");
embedSVGTestCase("resources/change-baseVal-while-animating-fill-remove.svg");

// Setup animation test
function sample1() {
    shouldBeCloseEnough("rect.width.animVal.value", "10");
    shouldBe("rect.width.baseVal.value", "10");
}

function sample2() {
    shouldBeCloseEnough("rect.width.animVal.value", "30");
    shouldBe("rect.width.baseVal.value", "10");

    rect.setAttribute("width", "100");
}

function sample3() {
    shouldBeCloseEnough("rect.width.animVal.value", "30");
    shouldBe("rect.width.baseVal.value", "100");
}

function sample4() {
    shouldBeCloseEnough("rect.width.animVal.value", "50");
    shouldBe("rect.width.baseVal.value", "100");
}

function sample5() {
    shouldBe("rect.width.animVal.value", "100");
    shouldBe("rect.width.baseVal.value", "100");
}

function executeTest() {
    rect = rootSVGElement.ownerDocument.getElementsByTagName("rect")[0];

    // All animations in the test file use the same duration, so it's not needed to list all sample points individually for an5/an6/an7/an8.
    const expectedValues = [
        // [animationId, time, sampleCallback]
        ["an1", 0.0,   sample1],
        ["an1", 2.0,   sample2],
        ["an1", 2.001, sample3],
        ["an1", 3.999, sample4],
        ["an1", 4.001, sample5],
        ["an1", 60.0,  sample5]
    ];

    runAnimationTest(expectedValues);
}

window.animationStartsImmediately = true;
var successfullyParsed = true;
