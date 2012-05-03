description("This tests additive='sum' support on animate elements with multiple begin times");
embedSVGTestCase("resources/multiple-begin-additive-animation.svg");

// Setup animation test
function checkBaseValues() {
return;
    shouldBe("rect.x.baseVal.value", "0");
    shouldBe("rect.y.baseVal.value", "0");
}

function sample1() {
    shouldBe("rect.x.animVal.value", "0");
    shouldBeCloseEnough("rect.y.animVal.value", "100");
    checkBaseValues();
}

function sample2() {
    shouldBe("rect.x.animVal.value", "0");
    shouldBeCloseEnough("rect.y.animVal.value", "166.67");
    checkBaseValues();
}

function sample3() {
    shouldBe("rect.x.animVal.value", "0");
    shouldBeCloseEnough("rect.y.animVal.value", "366.60");
    checkBaseValues();
}

function sample4() {
    shouldBe("rect.x.animVal.value", "0");
    shouldBeCloseEnough("rect.y.animVal.value", "366.73");
    checkBaseValues();
}

function sample5() {
    shouldBe("rect.x.animVal.value", "0");
    shouldBeCloseEnough("rect.y.animVal.value", "499.93");
    checkBaseValues();
}

function sample6() {
    shouldBe("rect.x.animVal.value", "400");
    shouldBeCloseEnough("rect.y.animVal.value", "500.06");
    checkBaseValues();
}

function sample7() {
    shouldBe("rect.x.animVal.value", "400");
    shouldBeCloseEnough("rect.y.animVal.value", "566.67");
    checkBaseValues();
}

function sample8() {
    shouldBe("rect.x.animVal.value", "400");
    shouldBeCloseEnough("rect.y.animVal.value", "633.33");
    checkBaseValues();
}

function sample9() {
    shouldBe("rect.x.animVal.value", "400");
    shouldBeCloseEnough("rect.y.animVal.value", "700");
    checkBaseValues();
}

function sample10() {
    shouldBe("rect.x.animVal.value", "400");
    shouldBeCloseEnough("rect.y.animVal.value", "766.60");
    checkBaseValues();
}

function sample11() {
    shouldBe("rect.x.animVal.value", "0");
    shouldBeCloseEnough("rect.y.animVal.value", "766.67");
    checkBaseValues();
}

function sample12() {
    shouldBe("rect.x.animVal.value", "0");
    shouldBeCloseEnough("rect.y.animVal.value", "833.33");
    checkBaseValues();
}

function sample13() {
    shouldBe("rect.x.animVal.value", "0");
    shouldBeCloseEnough("rect.y.animVal.value", "900");
    checkBaseValues();
}

function executeTest() {
    rect = rootSVGElement.ownerDocument.getElementsByTagName("rect")[0];

    // All animations in the test file use the same duration, so it's not needed to list all sample points individually for an5/an6/an7/an8.
    const expectedValues = [
        // [animationId, time, sampleCallback]
        ["an1", 0.0,    sample1],
        ["an1", 1.0,    sample2],
        ["an1", 3.999,  sample3],
        ["an1", 4.001,  sample4],
        ["an1", 5.999,  sample5],
        ["an1", 6.001,  sample6],
        ["an1", 7.0,    sample7],
        ["an1", 7.999,  sample8],
        ["an1", 8.001,  sample8],
        ["an1", 9.0,    sample9],
        ["an1", 9.999,  sample10],
        ["an1", 10.001, sample11],
        ["an1", 11.0,   sample12],
        ["an1", 11.999, sample13],
        ["an1", 12.001, sample13],
        ["an1", 60.0,   sample13]
    ];

    runAnimationTest(expectedValues);
}

window.animationStartsImmediately = true;
var successfullyParsed = true;
