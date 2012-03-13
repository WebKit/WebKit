description("This checks the effect on multiple animations on one target");
embedSVGTestCase("resources/multiple-animations-fill-freeze.svg");

// Setup animation test
function sample1() {
    shouldBeCloseEnough("rect1.x.animVal.value", "0");
    shouldBe("rect1.x.baseVal.value", "0");

    shouldBeCloseEnough("rect2.x.animVal.value", "0");
    shouldBe("rect2.x.baseVal.value", "0");

    shouldBeCloseEnough("rect3.x.animVal.value", "0");
    shouldBe("rect3.x.baseVal.value", "0");
}

function sample2() {
    shouldBeCloseEnough("rect1.x.animVal.value", "50");
    shouldBe("rect1.x.baseVal.value", "0");

    shouldBeCloseEnough("rect2.x.animVal.value", "50");
    shouldBe("rect2.x.baseVal.value", "0");

    shouldBeCloseEnough("rect3.x.animVal.value", "50");
    shouldBe("rect3.x.baseVal.value", "0");
}

function sample3() {
    shouldBeCloseEnough("rect1.x.animVal.value", "100");
    shouldBe("rect1.x.baseVal.value", "0");

    shouldBeCloseEnough("rect2.x.animVal.value", "100");
    shouldBe("rect2.x.baseVal.value", "0");

    shouldBeCloseEnough("rect3.x.animVal.value", "100");
    shouldBe("rect3.x.baseVal.value", "0");
}

function sample4() {
    shouldBeCloseEnough("rect1.x.animVal.value", "100");
    shouldBe("rect1.x.baseVal.value", "0");

    shouldBeCloseEnough("rect2.x.animVal.value", "0");
    shouldBe("rect2.x.baseVal.value", "0");

    shouldBeCloseEnough("rect3.x.animVal.value", "100");
    shouldBe("rect3.x.baseVal.value", "0");
}

function sample5() {
    shouldBeCloseEnough("rect1.x.animVal.value", "150");
    shouldBe("rect1.x.baseVal.value", "0");

    shouldBeCloseEnough("rect2.x.animVal.value", "150");
    shouldBe("rect2.x.baseVal.value", "0");

    shouldBeCloseEnough("rect3.x.animVal.value", "150");
    shouldBe("rect3.x.baseVal.value", "0");
}

function sample6() {
    shouldBeCloseEnough("rect1.x.animVal.value", "200");
    shouldBe("rect1.x.baseVal.value", "0");

    shouldBeCloseEnough("rect2.x.animVal.value", "200");
    shouldBe("rect2.x.baseVal.value", "0");

    shouldBeCloseEnough("rect3.x.animVal.value", "200");
    shouldBe("rect3.x.baseVal.value", "0");
}

function sample7() {
    shouldBeCloseEnough("rect1.x.animVal.value", "250");
    shouldBe("rect1.x.baseVal.value", "0");

    shouldBeCloseEnough("rect2.x.animVal.value", "250");
    shouldBe("rect2.x.baseVal.value", "0");

    shouldBeCloseEnough("rect3.x.animVal.value", "250");
    shouldBe("rect3.x.baseVal.value", "0");
}

function sample8() {
    shouldBe("rect1.x.animVal.value", "250");
    shouldBe("rect1.x.baseVal.value", "0");

    shouldBe("rect2.x.animVal.value", "250");
    shouldBe("rect2.x.baseVal.value", "0");

    shouldBe("rect3.x.animVal.value", "100");
    shouldBe("rect3.x.baseVal.value", "0");
}

function executeTest() {
    var rects = rootSVGElement.ownerDocument.getElementsByTagName("rect");
    rect1 = rects[0];
    rect2 = rects[1];
    rect3 = rects[2];

    const expectedValues = [
        // [animationId, time, sampleCallback]
        ["an1", 0.0,   sample1],
        ["an1", 1.0,   sample2],
        ["an1", 1.999, sample3],
        ["an1", 2.001, sample4],
        ["an1", 3.0,   sample4],
        ["an1", 3.999, sample4],
        ["an1", 4.0,   sample5],
        ["an1", 5.0,   sample6],
        ["an1", 5.999, sample7],
        ["an1", 6.001, sample8],
        ["an1", 60.0,  sample8]
    ];

    runAnimationTest(expectedValues);
}

window.animationStartsImmediately = true;
var successfullyParsed = true;
