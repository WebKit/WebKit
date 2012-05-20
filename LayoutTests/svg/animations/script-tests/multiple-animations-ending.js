description("This checks the effect on multiple animations ending on one target");
embedSVGTestCase("resources/multiple-animations-ending.svg");

// Setup animation test
function sample1() {
    shouldBeCloseEnough("rect1.x.animVal.value", "0");
    shouldBe("rect1.x.baseVal.value", "0");

    shouldBeCloseEnough("rect2.x.animVal.value", "100");
    shouldBe("rect2.x.baseVal.value", "200");

    shouldBeCloseEnough("rect3.x.animVal.value", "200");
    shouldBe("rect3.x.baseVal.value", "0");

    shouldBeCloseEnough("rect4.x.animVal.value", "10");
    shouldBe("rect4.x.baseVal.value", "0");
}

function sample2() {
    shouldBeCloseEnough("rect1.x.animVal.value", "50");
    shouldBe("rect1.x.baseVal.value", "0");

    shouldBeCloseEnough("rect2.x.animVal.value", "80");
    shouldBe("rect2.x.baseVal.value", "200");

    shouldBeCloseEnough("rect3.x.animVal.value", "210");
    shouldBe("rect3.x.baseVal.value", "0");

    shouldBeCloseEnough("rect4.x.animVal.value", "30");
    shouldBe("rect4.x.baseVal.value", "0");
}

function sample3() {
    shouldBeCloseEnough("rect1.x.animVal.value", "50");
    shouldBe("rect1.x.baseVal.value", "0");

    shouldBeCloseEnough("rect2.x.animVal.value", "80");
    shouldBe("rect2.x.baseVal.value", "200");

    shouldBeCloseEnough("rect3.x.animVal.value", "210");
    shouldBe("rect3.x.baseVal.value", "0");

    shouldBeCloseEnough("rect4.x.animVal.value", "30");
    shouldBe("rect4.x.baseVal.value", "0");
}

function sample4() {
    shouldBeCloseEnough("rect1.x.animVal.value", "50");
    shouldBe("rect1.x.baseVal.value", "0");

    shouldBeCloseEnough("rect2.x.animVal.value", "80");
    shouldBe("rect2.x.baseVal.value", "200");

    shouldBeCloseEnough("rect3.x.animVal.value", "210");
    shouldBe("rect3.x.baseVal.value", "0");

    shouldBeCloseEnough("rect4.x.animVal.value", "30");
    shouldBe("rect4.x.baseVal.value", "0");
}

function sample5() {
    shouldBeCloseEnough("rect1.x.animVal.value", "100");
    shouldBe("rect1.x.baseVal.value", "0");

    shouldBeCloseEnough("rect2.x.animVal.value", "60");
    shouldBe("rect2.x.baseVal.value", "200");

    shouldBeCloseEnough("rect3.x.animVal.value", "220");
    shouldBe("rect3.x.baseVal.value", "0");

    shouldBeCloseEnough("rect4.x.animVal.value", "30");
    shouldBe("rect4.x.baseVal.value", "0");
}

function sample6() {
    shouldBeCloseEnough("rect1.x.animVal.value", "0");
    shouldBe("rect1.x.baseVal.value", "0");

    shouldBeCloseEnough("rect2.x.animVal.value", "0");
    shouldBe("rect2.x.baseVal.value", "200");

    shouldBeCloseEnough("rect3.x.animVal.value", "0");
    shouldBe("rect3.x.baseVal.value", "0");

    shouldBeCloseEnough("rect4.x.animVal.value", "60");
    shouldBe("rect4.x.baseVal.value", "0");
}

function sample7() {
    shouldBeCloseEnough("rect1.x.animVal.value", "0");
    shouldBe("rect1.x.baseVal.value", "0");

    shouldBeCloseEnough("rect2.x.animVal.value", "0");
    shouldBe("rect2.x.baseVal.value", "200");

    shouldBeCloseEnough("rect3.x.animVal.value", "0");
    shouldBe("rect3.x.baseVal.value", "0");

    shouldBeCloseEnough("rect4.x.animVal.value", "60");
    shouldBe("rect4.x.baseVal.value", "0");
}

function sample8() {
    shouldBeCloseEnough("rect1.x.animVal.value", "0");
    shouldBe("rect1.x.baseVal.value", "0");

    shouldBeCloseEnough("rect2.x.animVal.value", "5");
    shouldBe("rect2.x.baseVal.value", "200");

    shouldBeCloseEnough("rect3.x.animVal.value", "5");
    shouldBe("rect3.x.baseVal.value", "0");

    shouldBeCloseEnough("rect4.x.animVal.value", "80");
    shouldBe("rect4.x.baseVal.value", "0");
}

function sample9() {
    shouldBeCloseEnough("rect1.x.animVal.value", "200");
    shouldBe("rect1.x.baseVal.value", "0");

    shouldBeCloseEnough("rect2.x.animVal.value", "5");
    shouldBe("rect2.x.baseVal.value", "200");

    shouldBeCloseEnough("rect3.x.animVal.value", "5");
    shouldBe("rect3.x.baseVal.value", "0");

    shouldBeCloseEnough("rect4.x.animVal.value", "80");
    shouldBe("rect4.x.baseVal.value", "0");
}

function sample10() {
    shouldBeCloseEnough("rect1.x.animVal.value", "200");
    shouldBe("rect1.x.baseVal.value", "0");

    shouldBeCloseEnough("rect2.x.animVal.value", "5");
    shouldBe("rect2.x.baseVal.value", "200");

    shouldBeCloseEnough("rect3.x.animVal.value", "5");
    shouldBe("rect3.x.baseVal.value", "0");

    shouldBeCloseEnough("rect4.x.animVal.value", "80");
    shouldBe("rect4.x.baseVal.value", "0");
}

function sample11() {
    shouldBeCloseEnough("rect1.x.animVal.value", "225");
    shouldBe("rect1.x.baseVal.value", "0");

    shouldBeCloseEnough("rect2.x.animVal.value", "10");
    shouldBe("rect2.x.baseVal.value", "200");

    shouldBeCloseEnough("rect3.x.animVal.value", "5");
    shouldBe("rect3.x.baseVal.value", "0");

    shouldBeCloseEnough("rect4.x.animVal.value", "80");
    shouldBe("rect4.x.baseVal.value", "0");
}

function sample12() {
    shouldBeCloseEnough("rect1.x.animVal.value", "225");
    shouldBe("rect1.x.baseVal.value", "0");

    shouldBeCloseEnough("rect2.x.animVal.value", "20");
    shouldBe("rect2.x.baseVal.value", "200");

    shouldBeCloseEnough("rect3.x.animVal.value", "5");
    shouldBe("rect3.x.baseVal.value", "0");

    shouldBeCloseEnough("rect4.x.animVal.value", "110");
    shouldBe("rect4.x.baseVal.value", "0");
}

function sample13() {
    shouldBeCloseEnough("rect1.x.animVal.value", "225");
    shouldBe("rect1.x.baseVal.value", "0");

    shouldBeCloseEnough("rect2.x.animVal.value", "20");
    shouldBe("rect2.x.baseVal.value", "200");

    shouldBeCloseEnough("rect3.x.animVal.value", "5");
    shouldBe("rect3.x.baseVal.value", "0");

    shouldBeCloseEnough("rect4.x.animVal.value", "110");
    shouldBe("rect4.x.baseVal.value", "0");
}

function sample14() {
    shouldBeCloseEnough("rect1.x.animVal.value", "250");
    shouldBe("rect1.x.baseVal.value", "0");

    shouldBeCloseEnough("rect2.x.animVal.value", "0");
    shouldBe("rect2.x.baseVal.value", "200");

    shouldBeCloseEnough("rect3.x.animVal.value", "5");
    shouldBe("rect3.x.baseVal.value", "0");

    shouldBeCloseEnough("rect4.x.animVal.value", "130");
    shouldBe("rect4.x.baseVal.value", "0");
}

function sample15() {
    shouldBeCloseEnough("rect1.x.animVal.value", "50");
    shouldBe("rect1.x.baseVal.value", "0");

    shouldBeCloseEnough("rect2.x.animVal.value", "200");
    shouldBe("rect2.x.baseVal.value", "200");

    shouldBeCloseEnough("rect3.x.animVal.value", "5");
    shouldBe("rect3.x.baseVal.value", "0");

    shouldBeCloseEnough("rect4.x.animVal.value", "130");
    shouldBe("rect4.x.baseVal.value", "0");
}

function sample16() {
    shouldBeCloseEnough("rect1.x.animVal.value", "50");
    shouldBe("rect1.x.baseVal.value", "0");

    shouldBeCloseEnough("rect2.x.animVal.value", "200");
    shouldBe("rect2.x.baseVal.value", "200");

    shouldBeCloseEnough("rect3.x.animVal.value", "5");
    shouldBe("rect3.x.baseVal.value", "0");

    shouldBeCloseEnough("rect4.x.animVal.value", "130");
    shouldBe("rect4.x.baseVal.value", "0");
}

function sample17() {
    shouldBeCloseEnough("rect1.x.animVal.value", "0");
    shouldBe("rect1.x.baseVal.value", "0");

    shouldBeCloseEnough("rect2.x.animVal.value", "200");
    shouldBe("rect2.x.baseVal.value", "200");

    shouldBeCloseEnough("rect3.x.animVal.value", "5");
    shouldBe("rect3.x.baseVal.value", "0");

    shouldBeCloseEnough("rect4.x.animVal.value", "130");
    shouldBe("rect4.x.baseVal.value", "0");
}

function sample18() {
    shouldBeCloseEnough("rect1.x.animVal.value", "250");
    shouldBe("rect1.x.baseVal.value", "0");

    shouldBeCloseEnough("rect2.x.animVal.value", "200");
    shouldBe("rect2.x.baseVal.value", "200");

    shouldBeCloseEnough("rect3.x.animVal.value", "250");
    shouldBe("rect3.x.baseVal.value", "0");

    shouldBeCloseEnough("rect4.x.animVal.value", "160");
    shouldBe("rect4.x.baseVal.value", "0");
}

function sample19() {
    shouldBeCloseEnough("rect1.x.animVal.value", "250");
    shouldBe("rect1.x.baseVal.value", "0");

    shouldBeCloseEnough("rect2.x.animVal.value", "200");
    shouldBe("rect2.x.baseVal.value", "200");

    shouldBeCloseEnough("rect3.x.animVal.value", "250");
    shouldBe("rect3.x.baseVal.value", "0");

    shouldBeCloseEnough("rect4.x.animVal.value", "160");
    shouldBe("rect4.x.baseVal.value", "0");
}

function sample20() {
    shouldBeCloseEnough("rect1.x.animVal.value", "250");
    shouldBe("rect1.x.baseVal.value", "0");

    shouldBeCloseEnough("rect2.x.animVal.value", "200");
    shouldBe("rect2.x.baseVal.value", "200");

    shouldBeCloseEnough("rect3.x.animVal.value", "200");
    shouldBe("rect3.x.baseVal.value", "0");

    shouldBeCloseEnough("rect4.x.animVal.value", "180");
    shouldBe("rect4.x.baseVal.value", "0");
}

function sample21() {
    shouldBeCloseEnough("rect1.x.animVal.value", "250");
    shouldBe("rect1.x.baseVal.value", "0");

    shouldBeCloseEnough("rect2.x.animVal.value", "200");
    shouldBe("rect2.x.baseVal.value", "200");

    shouldBeCloseEnough("rect3.x.animVal.value", "200");
    shouldBe("rect3.x.baseVal.value", "0");

    shouldBeCloseEnough("rect4.x.animVal.value", "180");
    shouldBe("rect4.x.baseVal.value", "0");
}

function sample22() {
    shouldBeCloseEnough("rect1.x.animVal.value", "250");
    shouldBe("rect1.x.baseVal.value", "0");

    shouldBeCloseEnough("rect2.x.animVal.value", "200");
    shouldBe("rect2.x.baseVal.value", "200");

    shouldBeCloseEnough("rect3.x.animVal.value", "150");
    shouldBe("rect3.x.baseVal.value", "0");

    shouldBeCloseEnough("rect4.x.animVal.value", "180");
    shouldBe("rect4.x.baseVal.value", "0");
}

function sample23() {
    shouldBeCloseEnough("rect1.x.animVal.value", "250");
    shouldBe("rect1.x.baseVal.value", "0");

    shouldBeCloseEnough("rect2.x.animVal.value", "200");
    shouldBe("rect2.x.baseVal.value", "200");

    shouldBeCloseEnough("rect3.x.animVal.value", "150");
    shouldBe("rect3.x.baseVal.value", "0");

    shouldBeCloseEnough("rect4.x.animVal.value", "180");
    shouldBe("rect4.x.baseVal.value", "0");
}

function executeTest() {
    var rects = rootSVGElement.ownerDocument.getElementsByTagName("rect");
    rect1 = rects[0];
    rect2 = rects[1];
    rect3 = rects[2];
    rect4 = rects[3];

    const expectedValues = [
        // [animationId, time, sampleCallback]
        ["an1", 0.0,   sample1],
        ["an1", 0.499, sample2],
        ["an1", 0.5,   sample3],
        ["an1", 0.501, sample4],
        ["an1", 0.999, sample5],
        ["an1", 1.0,   sample6],
        ["an1", 1.001, sample7],
        ["an1", 1.499, sample8],
        ["an1", 1.5,   sample9],
        ["an1", 1.501, sample10],
        ["an1", 1.999, sample11],
        ["an1", 2.0,   sample12],
        ["an1", 2.001, sample13],
        ["an1", 2.499, sample14],
        ["an1", 2.5,   sample15],
        ["an1", 2.501, sample16],
        ["an1", 2.999, sample17],
        ["an1", 3.0,   sample18],
        ["an1", 3.001, sample19],
        ["an1", 3.499, sample20],
        ["an1", 3.5,   sample21],
        ["an1", 4.0,   sample22],
        ["an1", 9.0,   sample23]
    ];

    runAnimationTest(expectedValues);
}

window.animationStartsImmediately = true;
var successfullyParsed = true;
