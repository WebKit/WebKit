description("This removes an animation element while the animation is running");
embedSVGTestCase("resources/remove-animation-element-while-animation-is-running.svg");

// Setup animation test
function sample1() {
    shouldBeCloseEnough("rect1.x.animVal.value", "50");
    shouldBe("rect1.x.baseVal.value", "0");

    shouldBeCloseEnough("rect2.x.animVal.value", "50");
    shouldBe("rect2.x.baseVal.value", "0");
}

function sample2() {
    shouldBeCloseEnough("rect1.x.animVal.value", "100");
    shouldBe("rect1.x.baseVal.value", "0");

    shouldBeCloseEnough("rect2.x.animVal.value", "100");
    shouldBe("rect2.x.baseVal.value", "0");

    // Remove the animation element animating rect1
    // The effect is that rect1 is now reset to the initial state, before any animation was applied to it.
    // Compatible with FF. In Opera it shows a repainting bug currently (two rects are visible!).
    var an1 = rootSVGElement.ownerDocument.getElementById("an1");
    an1.parentNode.removeChild(an1);
}

function sample3() {
    shouldBe("rect1.x.animVal.value", "0");
    shouldBe("rect1.x.baseVal.value", "0");

    shouldBeCloseEnough("rect2.x.animVal.value", "100");
    shouldBe("rect2.x.baseVal.value", "0");
}

function sample4() {
    shouldBe("rect1.x.animVal.value", "0");
    shouldBe("rect1.x.baseVal.value", "0");

    shouldBe("rect2.x.animVal.value", "0");
    shouldBe("rect2.x.baseVal.value", "0");
}

function executeTest() {
    var rects = rootSVGElement.ownerDocument.getElementsByTagName("rect");
    rect1 = rects[0];
    rect2 = rects[1];

    const expectedValues = [
        // [animationId, time, sampleCallback]
        ["an1", 0.0,   sample1],
        ["an1", 1.0,   sample2],
        ["an2", 1.001, sample3],
        ["an2", 2.001, sample4],
        ["an2", 60.0,  sample4]
    ];

    runAnimationTest(expectedValues);
}

window.animationStartsImmediately = true;
var successfullyParsed = true;
