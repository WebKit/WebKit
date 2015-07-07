description("This verifies several attributeTypes combiniations");
embedSVGTestCase("resources/attributeTypes.svg");

// Setup animation test
function sample1() {
    shouldBeCloseEnough("rect1.width.animVal.value", "10");
    shouldBe("rect1.width.baseVal.value", "10");
    expectFillColor(rect1, 0, 128, 0);

    shouldBe("rect2.width.animVal.value", "100");
    shouldBe("rect2.width.baseVal.value", "100");
    expectFillColor(rect2, 0, 128, 0);

    shouldBe("rect3.width.animVal.value", "100");
    shouldBe("rect3.width.baseVal.value", "100");
    expectFillColor(rect3, 255, 0, 0);
    shouldBeEqualToString("rect3.getAttribute('fill')", "red");

    shouldBe("rect4.width.animVal.value", "100");
    shouldBe("rect4.width.baseVal.value", "100");
    expectFillColor(rect4, 255, 0, 0);
    shouldBeEqualToString("rect4.getAttribute('fill')", "red");
}

function sample2() {
    shouldBeCloseEnough("rect1.width.animVal.value", "55");
    shouldBe("rect1.width.baseVal.value", "10");
    expectFillColor(rect1, 0, 128, 0);

    shouldBe("rect2.width.animVal.value", "100");
    shouldBe("rect2.width.baseVal.value", "100");
    expectFillColor(rect2, 0, 128, 0);

    shouldBe("rect3.width.animVal.value", "100");
    shouldBe("rect3.width.baseVal.value", "100");
    expectFillColor(rect3, 128, 64, 0);
    shouldBeEqualToString("rect3.getAttribute('fill')", "red");

    shouldBe("rect4.width.animVal.value", "100");
    shouldBe("rect4.width.baseVal.value", "100");
    expectFillColor(rect4, 128, 64, 0);
    shouldBeEqualToString("rect4.getAttribute('fill')", "red");
}

function sample3() {
    shouldBeCloseEnough("rect1.width.animVal.value", "100");
    shouldBe("rect1.width.baseVal.value", "10");
    expectFillColor(rect1, 0, 128, 0);

    shouldBe("rect2.width.animVal.value", "100");
    shouldBe("rect2.width.baseVal.value", "100");
    expectFillColor(rect2, 0, 128, 0);

    shouldBe("rect3.width.animVal.value", "100");
    shouldBe("rect3.width.baseVal.value", "100");
    expectFillColor(rect3, 0, 128, 0);
    shouldBeEqualToString("rect3.getAttribute('fill')", "red");

    shouldBe("rect4.width.animVal.value", "100");
    shouldBe("rect4.width.baseVal.value", "100");
    expectFillColor(rect4, 0, 128, 0);
    shouldBeEqualToString("rect4.getAttribute('fill')", "red");
}

function executeTest() {
    rects = rootSVGElement.ownerDocument.getElementsByTagName("rect");
    rect1 = rects[0]; 
    rect2 = rects[1]; 
    rect3 = rects[2]; 
    rect4 = rects[3]; 

    // All animations in the test file use the same duration, so it's not needed to list all sample points individually for an5/an6/an7/an8.
    const expectedValues = [
        // [animationId, time, sampleCallback]
        ["an1", 0.0,  sample1],
        ["an1", 2.0,  sample2],
        ["an1", 4.0,  sample3],
        ["an1", 60.0, sample3],
    ];

    runAnimationTest(expectedValues);
}

window.animationStartsImmediately = true;
var successfullyParsed = true;
