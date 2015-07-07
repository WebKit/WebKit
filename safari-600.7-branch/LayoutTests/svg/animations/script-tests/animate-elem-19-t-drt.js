description("A copy of the corresponding W3C-SVG-1.1 test, which dumps the animation at certain times");
embedSVGTestCase("../W3C-SVG-1.1/animate-elem-19-t.svg");

// Setup animation test
function sample1() { // From 0s to 4s
    shouldBeCloseEnough("rect.width.animVal.value", "300");
    shouldBe("rect.width.baseVal.value", "300");
}

function sample2() { // From 4s to 6s
    shouldBeCloseEnough("rect.width.animVal.value", "255");
    shouldBe("rect.width.baseVal.value", "300");
}

function sample3() { // From 6s to 8s
    shouldBeCloseEnough("rect.width.animVal.value", "180");
    shouldBe("rect.width.baseVal.value", "300");
}

function sample4() { // From 8s to 8s
    shouldBeCloseEnough("rect.width.animVal.value", "30");
    shouldBe("rect.width.baseVal.value", "300");
}

function executeTest() {
    rect = rootSVGElement.ownerDocument.getElementsByTagName("rect")[0];

    // Sampling according to: values="300;255;180;30" keyTimes="0;.5;.75;1" begin="0s" dur="8s"
    const expectedValues = [
        // [animationId, time, sampleCallback]
        ["an1", 0.0,    sample1],
        ["an1", 3.999,  sample2],
        ["an1", 4.001,  sample2],
        ["an1", 5.999,  sample3],
        ["an1", 6.001,  sample3],
        ["an1", 7.999,  sample4],
        ["an1", 8.001,  sample4],
        ["an1", 60.0,   sample4]
    ];

    runAnimationTest(expectedValues);
}

window.animationStartsImmediately = true;
var successfullyParsed = true;
