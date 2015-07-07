description("This test forces use shadow tree recreation while an animating is running");
embedSVGTestCase("resources/force-use-shadow-tree-recreation-while-animating.svg");

// Setup animation test
function sample1() {
    shouldBeCloseEnough("rect.width.animVal.value", "10");
    shouldBe("rect.width.baseVal.value", "10");
}

function sample2() {
    shouldBeCloseEnough("rect.width.animVal.value", "55");
    shouldBe("rect.width.baseVal.value", "10");
}

function forceUseShadowTreeRecreation() {
    rect.setAttribute("fill", "green");
}

function sample3() {
    shouldBeCloseEnough("rect.width.animVal.value", "100");
    shouldBe("rect.width.baseVal.value", "10");
}

function executeTest() {
    rect = rootSVGElement.ownerDocument.getElementsByTagName("rect")[0];

    const expectedValues = [
        // [animationId, time, sampleCallback]
        ["an1", 0.0,   sample1],
        ["an1", 1.999, sample2],
        ["an1", 2.0,   forceUseShadowTreeRecreation],
        ["an1", 2.001, sample2],
        ["an1", 4.0,   sample3],
        ["an1", 60.0,  sample3],
    ];

    runAnimationTest(expectedValues);
}

window.animationStartsImmediately = true;
var successfullyParsed = true;
