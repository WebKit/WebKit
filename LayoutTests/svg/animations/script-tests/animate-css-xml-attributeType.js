description("Tests that XML and CSS attributeTypes can be switched between.");
createSVGTestCase();

// Setup test document
var rect = createSVGElement("rect");
rect.setAttribute("id", "rect");
rect.setAttribute("x", "100");
rect.setAttribute("width", "100");
rect.setAttribute("height", "100");
rect.setAttribute("fill", "green");
rect.setAttribute("onclick", "executeTest()");

var set = createSVGElement("set");
set.setAttribute("id", "set");
set.setAttribute("attributeName", "x");
set.setAttribute("attributeType", "XML");
set.setAttribute("to", "300");
set.setAttribute("begin", "click");
rect.appendChild(set);
rootSVGElement.appendChild(rect);

// Setup animation test
function sample1() {
    shouldBeCloseEnough("rect.x.animVal.value", "100");
    shouldBe("rect.x.baseVal.value", "100");
}

function sample2() {
    shouldBeCloseEnough("rect.x.animVal.value", "300");
    // change the animationType to CSS which is invalid.
    set.setAttribute("attributeType", "CSS");
}

function sample3() {
    // verify that the animation resets.
    shouldBeCloseEnough("rect.x.animVal.value", "100");
    // change the animation to a CSS animatable value.
    set.setAttribute("attributeName", "opacity");
    set.setAttribute("to", "0.8");
}

function sample4() {
    shouldBeCloseEnough("getComputedStyle(rect).getPropertyCSSValue('opacity').getFloatValue(CSSPrimitiveValue.CSS_NUMBER)", "0.8");
    // change the animation to a non-CSS animatable value.
    set.setAttribute("attributeName", "x");
    set.setAttribute("to", "200");
}

function sample5() {
    // verify that the animation does not run.
    shouldBeCloseEnough("rect.x.animVal.value", "100");
    shouldBeCloseEnough("getComputedStyle(rect).getPropertyCSSValue('opacity').getFloatValue(CSSPrimitiveValue.CSS_NUMBER)", "1.0");
    // change the animationType to XML which is valid.
    set.setAttribute("attributeType", "XML");
}

function sample6() {
    shouldBeCloseEnough("rect.x.animVal.value", "200");
    shouldBe("rect.x.baseVal.value", "100");
}

function executeTest() {
    const expectedValues = [
        // [animationId, time, sampleCallback]
        ["set", 0.0, sample1],
        ["set", 0.5, sample2],
        ["set", 1.0, sample3],
        ["set", 1.5, sample4],
        ["set", 2.0, sample5],
        ["set", 2.5, sample6]
    ];

    runAnimationTest(expectedValues);
}

window.clickX = 150;
var successfullyParsed = true;
