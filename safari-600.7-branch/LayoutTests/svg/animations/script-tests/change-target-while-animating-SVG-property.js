description("This changes the target of an animation while its running");
embedSVGTestCase("resources/change-target-while-animating-SVG-property.svg");

// Setup animation test
function sample1() {
    shouldBeCloseEnough("rect1.width.animVal.value", "150");
    shouldBe("rect1.width.baseVal.value", "150");

    shouldBeCloseEnough("rect2.width.animVal.value", "150");
    shouldBe("rect2.width.baseVal.value", "150");
}

function sample2() {
    shouldBeCloseEnough("rect1.width.animVal.value", "100");
    shouldBe("rect1.width.baseVal.value", "150");

    shouldBeCloseEnough("rect2.width.animVal.value", "150");
    shouldBe("rect2.width.baseVal.value", "150");

    // Switch to new target while animation is running.
    // The effect is that rect1 is now reset to the initial state, before any animation was applied to it.
    // Compatible with FF. In Opera it only works when not driving the timeline using setCurrentTime.
    rootSVGElement.ownerDocument.getElementById("an1").setAttributeNS(xlinkNS, "xlink:href", "#target2");
}

function sample3() {
    shouldBeCloseEnough("rect1.width.animVal.value", "150");
    shouldBe("rect1.width.baseVal.value", "150");

    shouldBeCloseEnough("rect2.width.animVal.value", "100");
    shouldBe("rect2.width.baseVal.value", "150");
}

function sample4() {
    shouldBeCloseEnough("rect1.width.animVal.value", "150");
    shouldBe("rect1.width.baseVal.value", "150");

    shouldBeCloseEnough("rect2.width.animVal.value", "50");
    shouldBe("rect2.width.baseVal.value", "150");
}

function sample5() {
    shouldBe("rect1.width.animVal.value", "150");
    shouldBe("rect1.width.baseVal.value", "150");

    shouldBe("rect2.width.animVal.value", "50");
    shouldBe("rect2.width.baseVal.value", "150");
}

function executeTest() {
    var rects = rootSVGElement.ownerDocument.getElementsByTagName("rect");
    rect1 = rects[0];
    rect2 = rects[1];

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
