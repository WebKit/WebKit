description(
"This tests that Math.min and Math.max for doubles works correctly in the DFG JIT."
);

function doMin(a, b) {
    return Math.min(a, b);
}

function doMax(a, b) {
    return Math.max(a, b);
}

testRunner.neverInlineFunction(doMin);
testRunner.neverInlineFunction(doMax);

var count = 0;
while (!testRunner.numberOfDFGCompiles(doMin) || !testRunner.numberOfDFGCompiles(doMax)) {
    doMin(1.5, 2.5);
    doMax(1.5, 2.5);
    count++;
}

shouldBe("doMin(1.5, 2.5)", "1.5");
shouldBe("doMin(2.5, 1.5)", "1.5");
shouldBe("doMin(1.5, 1.5)", "1.5");
shouldBe("doMin(2.5, 2.5)", "2.5");

shouldBe("doMin(1.5, NaN)", "NaN");
shouldBe("doMin(2.5, NaN)", "NaN");
shouldBe("doMin(NaN, 1.5)", "NaN");
shouldBe("doMin(NaN, 2.5)", "NaN");

shouldBe("doMin(NaN, NaN)", "NaN");

shouldBe("doMax(1.5, 2.5)", "2.5");
shouldBe("doMax(2.5, 1.5)", "2.5");
shouldBe("doMax(1.5, 1.5)", "1.5");
shouldBe("doMax(2.5, 2.5)", "2.5");

shouldBe("doMax(1.5, NaN)", "NaN");
shouldBe("doMax(2.5, NaN)", "NaN");
shouldBe("doMax(NaN, 1.5)", "NaN");
shouldBe("doMax(NaN, 2.5)", "NaN");

shouldBe("doMax(NaN, NaN)", "NaN");

var successfullyParsed = true;
