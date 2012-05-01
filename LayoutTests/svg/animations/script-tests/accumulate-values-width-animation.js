description("This tests values animation and accumulate='sum'");
embedSVGTestCase("resources/accumulate-values-width-animation.svg");

// Setup animation test
function sample1() {
    shouldBeCloseEnough("rect.width.animVal.value", "0");
    shouldBe("rect.width.baseVal.value", "20");
}

function sample2() {
    shouldBeCloseEnough("rect.width.animVal.value", "30");
    shouldBe("rect.width.baseVal.value", "20");
}

function sample3() {
    shouldBeCloseEnough("rect.width.animVal.value", "20");
    shouldBe("rect.width.baseVal.value", "20");
}

function sample4() {
    shouldBeCloseEnough("rect.width.animVal.value", "50");
    shouldBe("rect.width.baseVal.value", "20");
}

function sample5() {
    shouldBeCloseEnough("rect.width.animVal.value", "40");
    shouldBe("rect.width.baseVal.value", "20");
}

function sample6() {
    shouldBeCloseEnough("rect.width.animVal.value", "70");
    shouldBe("rect.width.baseVal.value", "20");
}

function sample7() {
    shouldBeCloseEnough("rect.width.animVal.value", "60");
    shouldBe("rect.width.baseVal.value", "20");
}

function sample8() {
    shouldBeCloseEnough("rect.width.animVal.value", "90");
    shouldBe("rect.width.baseVal.value", "20");
}

function sample9() {
    shouldBeCloseEnough("rect.width.animVal.value", "80");
    shouldBe("rect.width.baseVal.value", "20");
}

function sample10() {
    shouldBeCloseEnough("rect.width.animVal.value", "110");
    shouldBe("rect.width.baseVal.value", "20");
}

function sample11() {
    shouldBeCloseEnough("rect.width.animVal.value", "100");
    shouldBe("rect.width.baseVal.value", "20");
}

function executeTest() {
    rect = rootSVGElement.ownerDocument.getElementsByTagName("rect")[0];

    const expectedValues = [
        // [animationId, time, sampleCallback]
        ["an1", 0.0,    sample1],
        ["an1", 1.0,    sample2],
        ["an1", 1.999,  sample3],
        ["an1", 2.001,  sample3],
        ["an1", 3.0,    sample4],
        ["an1", 3.999,  sample5],
        ["an1", 4.001,  sample5],
        ["an1", 5.0,    sample6],
        ["an1", 5.999,  sample7],
        ["an1", 6.001,  sample7],
        ["an1", 7.0,    sample8],
        ["an1", 7.999,  sample9],
        ["an1", 8.001,  sample9],
        ["an1", 9.0,    sample10],
        ["an1", 9.999,  sample11],
        ["an1", 10.001, sample11],
        ["an1", 11.0,   sample11],
        ["an1", 60.0,   sample11]
    ];

    runAnimationTest(expectedValues);
}

window.animationStartsImmediately = true;
var successfullyParsed = true;
