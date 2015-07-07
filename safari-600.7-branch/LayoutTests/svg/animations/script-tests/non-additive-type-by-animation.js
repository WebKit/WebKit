description("This by animation for all non-additive property types - should have no effect.");
embedSVGTestCase("resources/non-additive-type-by-animation.svg");

// Setup animation test
function sample() {
    shouldBe("feConvolveMatrix.preserveAlpha.animVal", "false");
    shouldBe("filter.filterUnits.animVal", "SVGUnitTypes.SVG_UNIT_TYPE_OBJECTBOUNDINGBOX");
    shouldBe("svg.preserveAspectRatio.animVal.align", "SVGPreserveAspectRatio.SVG_PRESERVEASPECTRATIO_NONE");
    shouldBe("svg.preserveAspectRatio.animVal.meetOrSlice", "SVGPreserveAspectRatio.SVG_MEETORSLICE_MEET");
    shouldBeEqualToString("feConvolveMatrix.result.animVal", "");

    shouldBe("feConvolveMatrix.preserveAlpha.baseVal", "false");
    shouldBe("filter.filterUnits.baseVal", "SVGUnitTypes.SVG_UNIT_TYPE_OBJECTBOUNDINGBOX");
    shouldBe("svg.preserveAspectRatio.baseVal.align", "SVGPreserveAspectRatio.SVG_PRESERVEASPECTRATIO_NONE");
    shouldBe("svg.preserveAspectRatio.baseVal.meetOrSlice", "SVGPreserveAspectRatio.SVG_MEETORSLICE_MEET");
    shouldBeEqualToString("feConvolveMatrix.result.baseVal", "");
}

function executeTest() {
    filter = rootSVGElement.ownerDocument.getElementsByTagName("filter")[0];
    feConvolveMatrix = rootSVGElement.ownerDocument.getElementsByTagName("feConvolveMatrix")[0];
    svg = rootSVGElement.ownerDocument.getElementsByTagName("svg")[0];

    const expectedValues = [
        // [animationId, time, sampleCallback]
        ["an1", 0.0,   sample],
        ["an1", 1.999, sample],
        ["an1", 2.001, sample],
        ["an1", 3.999, sample],
        ["an1", 4.001, sample]
    ];

    runAnimationTest(expectedValues);
}

window.animationStartsImmediately = true;
var successfullyParsed = true;
