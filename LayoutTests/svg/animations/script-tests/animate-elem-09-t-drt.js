description("A copy of the corresponding W3C-SVG-1.1 test, which dumps the animation at certain times");
embedSVGTestCase("../W3C-SVG-1.1/animate-elem-09-t.svg");

// Setup animation test
function sample1() {
    shouldBe("rect1.height.animVal.value", "210");
    shouldBe("rect1.height.baseVal.value", "210");
    expectTranslationMatrix("rootSVGElement.getTransformToElement(rect2)", "-250", "-250");
}

function sample2() {
    shouldBe("rect1.height.animVal.value", "177");
    shouldBe("rect1.height.baseVal.value", "210");
    expectTranslationMatrix("rootSVGElement.getTransformToElement(rect2)", "-250", "-217");
}

function sample3() {
    shouldBe("rect1.height.animVal.value", "121");
    shouldBe("rect1.height.baseVal.value", "210");
    expectTranslationMatrix("rootSVGElement.getTransformToElement(rect2)", "-250", "-161");
}

function sample4() {
    shouldBe("rect1.height.animVal.value", "10");
    shouldBe("rect1.height.baseVal.value", "210");
    expectTranslationMatrix("rootSVGElement.getTransformToElement(rect2)", "-250", "-50");
}

function executeTest() {
    var rects = rootSVGElement.ownerDocument.getElementsByTagName("rect");
    rect1 = rects[0];
    rect2 = rects[1];

    const expectedValues = [
        // [animationId, time, sampleCallback]
        ["an1", 0.0,   sample1],
        ["an1", 2.666, sample2],
        ["an1", 2.667, sample2],
        ["an1", 5.332, sample3],
        ["an1", 5.333, sample3],
        ["an1", 7.999, sample4],
        ["an1", 8.001, sample4],
        ["an1", 60.0,  sample4]
    ];

    runAnimationTest(expectedValues);
}

window.animationStartsImmediately = true;
var successfullyParsed = true;
