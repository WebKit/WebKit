description("This tests multiple additive='sum' animations running at the same time");
embedSVGTestCase("resources/additive-from-to-width-animation.svg");

// Setup animation test
function sample1() {
    shouldBeCloseEnough("rect.width.animVal.value", "20");
    shouldBe("rect.width.baseVal.value", "10");
}

function sample2() {
    shouldBeCloseEnough("rect.width.animVal.value", "60");
    shouldBe("rect.width.baseVal.value", "10");
}

function sample3() {
    shouldBeCloseEnough("rect.width.animVal.value", "100");
    shouldBe("rect.width.baseVal.value", "10");
}

function executeTest() {
    rect = rootSVGElement.ownerDocument.getElementsByTagName("rect")[0];

    // All animations in the test file use the same duration, so it's not needed to list all sample points individually for an5/an6/an7/an8.
    const expectedValues = [
        // [animationId, time, sampleCallback]
        ["an1", 0.0,  sample1],
        ["an1", 2.0,  sample2],
        ["an1", 4.0,  sample3],
        ["an1", 60.0, sample3]
    ];

    runAnimationTest(expectedValues);
}

window.animationStartsImmediately = true;
var successfullyParsed = true;
