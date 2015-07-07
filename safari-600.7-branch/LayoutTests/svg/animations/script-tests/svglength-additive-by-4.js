description("This tests by-animations adding to previous underlying values");
embedSVGTestCase("resources/svglength-additive-by-4.svg");

// Setup animation test
function sample1() {
    shouldBeCloseEnough("rect.width.animVal.value", "10");
    shouldBe("rect.width.baseVal.value", "10");
}

function sample2() {
    shouldBeCloseEnough("rect.width.animVal.value", "30");
    shouldBe("rect.width.baseVal.value", "10");
}

function sample3() {
    shouldBeCloseEnough("rect.width.animVal.value", "50");
    shouldBe("rect.width.baseVal.value", "10");
}

function sample4() {
    shouldBeCloseEnough("rect.width.animVal.value", "10");
    shouldBe("rect.width.baseVal.value", "10");
}

function sample5() {
    shouldBeCloseEnough("rect.width.animVal.value", "55");
    shouldBe("rect.width.baseVal.value", "10");
}

function sample6() {
    shouldBeCloseEnough("rect.width.animVal.value", "100");
    shouldBe("rect.width.baseVal.value", "10");
}

function executeTest() {
    rect = rootSVGElement.ownerDocument.getElementsByTagName("rect")[0];

    const expectedValues = [
        // [animationId, time, sampleCallback]
        ["an1", 0.0,   sample1],
        ["an1", 2.0,   sample2],
        ["an1", 3.999, sample3],
        ["an1", 4.001, sample4],
        ["an1", 7.0,   sample5],
        ["an1", 9.0,   sample6],
        ["an1", 60.0,  sample6]
    ];

    runAnimationTest(expectedValues);
}

window.animationStartsImmediately = true;
var successfullyParsed = true;
