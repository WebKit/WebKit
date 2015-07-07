description("This tests scripting a CSS property while animation is running");
embedSVGTestCase("resources/change-css-property-while-animating-fill-remove.svg");

// Setup animation test
function sample1() {
    shouldBeCloseEnough("getComputedStyle(rect).getPropertyCSSValue('opacity').getFloatValue(CSSPrimitiveValue.CSS_NUMBER)", "0");
}

function sample2() {
    shouldBeCloseEnough("getComputedStyle(rect).getPropertyCSSValue('opacity').getFloatValue(CSSPrimitiveValue.CSS_NUMBER)", "0.25");
    rect.setAttribute("opacity", "1");
}

function sample3() {
    shouldBeCloseEnough("getComputedStyle(rect).getPropertyCSSValue('opacity').getFloatValue(CSSPrimitiveValue.CSS_NUMBER)", "0.25");
}

function sample4() {
    shouldBeCloseEnough("getComputedStyle(rect).getPropertyCSSValue('opacity').getFloatValue(CSSPrimitiveValue.CSS_NUMBER)", "0.5");
}

function sample5() {
    shouldBeCloseEnough("getComputedStyle(rect).getPropertyCSSValue('opacity').getFloatValue(CSSPrimitiveValue.CSS_NUMBER)", "1");
}

function executeTest() {
    rect = rootSVGElement.ownerDocument.getElementsByTagName("rect")[0];

    const expectedValues = [
        // [animationId, time, sampleCallback]
        ["an1", 0.0,   sample1],
        ["an1", 2.0,   sample2],
        ["an1", 2.001, sample3],
        ["an1", 3.999, sample4],
        ["an1", 4.001, sample5],
        ["an1", 60.0,  sample5]
    ];

    runAnimationTest(expectedValues);
}

window.animationStartsImmediately = true;
var successfullyParsed = true;
