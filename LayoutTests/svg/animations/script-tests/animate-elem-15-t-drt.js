description("A copy of the corresponding W3C-SVG-1.1 test, which dumps the animation at certain times");
embedSVGTestCase("../W3C-SVG-1.1/animate-elem-15-t.svg");

// Setup animation test
function sample1() { // From 0s to 2.25s
    shouldBeCloseEnough("rect.width.animVal.value", "300");
    shouldBe("rect.width.baseVal.value", "300");
}

function sample2() { // From 2.25s to 4.5s
    shouldBeCloseEnough("rect.width.animVal.value", "232.5");
    shouldBe("rect.width.baseVal.value", "300");
}

function sample3() { // From 4.5s to 9s
    shouldBeCloseEnough("rect.width.animVal.value", "165");
    shouldBe("rect.width.baseVal.value", "300");
}

function sample4() { // From 9s to 9s
    shouldBeCloseEnough("rect.width.animVal.value", "30");
    shouldBe("rect.width.baseVal.value", "300");
}

function executeTest() {
    rect = rootSVGElement.ownerDocument.getElementsByTagName("rect")[0];

    // Sampling according to: keyTimes="0;.25;.5;1" begin="0s" dur="9s" 
    const expectedValues = [
        // [animationId, time, sampleCallback]
        ["an1", 0.0,    sample1],
        ["an1", 2.249,  sample2],
        ["an1", 2.251,  sample2],
        ["an1", 4.499,  sample3],
        ["an1", 4.501,  sample3],
        ["an1", 8.999,  sample4],
        ["an1", 9.001,  sample4],
        ["an1", 60.0,   sample4]
    ];

    runAnimationTest(expectedValues);
}

window.animationStartsImmediately = true;
var successfullyParsed = true;
