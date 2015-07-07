description("This tests values animation and additive='sum'");
embedSVGTestCase("resources/additive-values-width-animation.svg");

// Setup animation test
function sample1() {
    shouldBeCloseEnough("rect.width.animVal.value", "10");
    shouldBe("rect.width.baseVal.value", "10");
}

function sample2() {
    shouldBeCloseEnough("rect.width.animVal.value", "60");
    shouldBe("rect.width.baseVal.value", "10");
}

function sample3() {
    shouldBeCloseEnough("rect.width.animVal.value", "93.3");
    shouldBe("rect.width.baseVal.value", "10");
}

function changeBaseVal() {
    // At 5s, only change the baseVal.
    rect.width.baseVal.value = 60;
}

function sample4() {
    shouldBeCloseEnough("rect.width.animVal.value", "143.33");
    shouldBe("rect.width.baseVal.value", "60");
}

function sample5() {
    shouldBeCloseEnough("rect.width.animVal.value", "160");
    shouldBe("rect.width.baseVal.value", "60");
}

function executeTest() {
    rect = rootSVGElement.ownerDocument.getElementsByTagName("rect")[0];

    // All animations in the test file use the same duration, so it's not needed to list all sample points individually for an5/an6/an7/an8.
    const expectedValues = [
        // [animationId, time, sampleCallback]
        ["an1", 0.0,   sample1],
        ["an1", 3.0,   sample2],
        ["an1", 4.999, sample3],
        ["an1", 5.0,   changeBaseVal],
        ["an1", 5.001, sample4],
        ["an1", 6.001, sample5],
        ["an1", 60.0,  sample5]
    ];

    runAnimationTest(expectedValues);
}

window.animationStartsImmediately = true;
var successfullyParsed = true;
