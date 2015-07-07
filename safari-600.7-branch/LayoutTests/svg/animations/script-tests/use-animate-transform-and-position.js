description("Test animation of use element where the instance is also animated with animate and animateTransform");
createSVGTestCase();

// Setup test document
var rect = createSVGElement("rect");
rect.setAttribute("id", "rect");
rect.setAttribute("x", "10");
rect.setAttribute("y", "10");
rect.setAttribute("width", "100");
rect.setAttribute("height", "100");
rect.setAttribute("fill", "green");
rect.setAttribute("onclick", "executeTest()");

var animate1 = createSVGElement("animate");
animate1.setAttribute("id", "animate1");
animate1.setAttribute("attributeName", "x");
animate1.setAttribute("begin", "1s");
animate1.setAttribute("dur", "10s");
animate1.setAttribute("from", "10");
animate1.setAttribute("to", "110");
rect.appendChild(animate1);
rootSVGElement.appendChild(rect);

var use = createSVGElement("use");
use.setAttribute("id", "use");
use.setAttributeNS('http://www.w3.org/1999/xlink', 'xlink:href', '#rect');
use.setAttribute("x", "0");
use.setAttribute("y", "0");
rootSVGElement.appendChild(use);

var animateTransform1 = createSVGElement("animateTransform");
animateTransform1.setAttribute("id", "animateTransform1");
animateTransform1.setAttribute("attributeName", "transform");
animateTransform1.setAttribute("type", "translate");
animateTransform1.setAttribute("begin", "0s");
animateTransform1.setAttribute("dur", "10s");
animateTransform1.setAttribute("from", "0 100");
animateTransform1.setAttribute("to", "0 200");
use.appendChild(animateTransform1);

var animate2 = createSVGElement("animate");
animate2.setAttribute("id", "animate2");
animate2.setAttribute("attributeName", "x");
animate2.setAttribute("begin", "2s");
animate2.setAttribute("dur", "10s");
animate2.setAttribute("from", "0");
animate2.setAttribute("to", "100");
use.appendChild(animate2);

var animateTransform2 = createSVGElement("animateTransform");
animateTransform2.setAttribute("id", "animateTransform2");
animateTransform2.setAttribute("attributeName", "transform");
animateTransform2.setAttribute("type", "translate");
animateTransform2.setAttribute("begin", "3s");
animateTransform2.setAttribute("dur", "10s");
animateTransform2.setAttribute("from", "0 130");
animateTransform2.setAttribute("to", "0 100");
use.appendChild(animateTransform2);

// Setup animation test
function sample1() {
    // Check initial/end conditions
    shouldBe("rect.x.animVal.value", "10");
    shouldBe("rect.x.baseVal.value", "10");
    shouldBe("rect.y.animVal.value", "10");
    shouldBe("rect.y.baseVal.value", "10");
    shouldBe("use.x.animVal.value", "0");
    shouldBe("use.x.baseVal.value", "0");
    shouldBe("use.y.animVal.value", "0");
    shouldBe("use.y.baseVal.value", "0");
    shouldBe("use.transform.baseVal.numberOfItems", "0");
}

function sample2() {
    shouldBe("rect.x.animVal.value", "15");
    shouldBe("rect.x.baseVal.value", "10");
    shouldBe("rect.y.animVal.value", "10");
    shouldBe("rect.y.baseVal.value", "10");
    shouldBe("use.x.animVal.value", "0");
    shouldBe("use.x.baseVal.value", "0");
    shouldBe("use.y.animVal.value", "0");
    shouldBe("use.y.baseVal.value", "0");
    shouldBe("use.transform.animVal.numberOfItems", "1");
    shouldBe("use.transform.baseVal.numberOfItems", "0");
    shouldBe("use.transform.animVal.getItem(0).matrix.e", "0");
    shouldBe("use.transform.animVal.getItem(0).matrix.f", "115");
}

function sample3() {
    shouldBe("rect.x.animVal.value", "25");
    shouldBe("rect.x.baseVal.value", "10");
    shouldBe("rect.y.animVal.value", "10");
    shouldBe("rect.y.baseVal.value", "10");
    shouldBe("use.x.animVal.value", "5");
    shouldBe("use.x.baseVal.value", "0");
    shouldBe("use.y.animVal.value", "0");
    shouldBe("use.y.baseVal.value", "0");
    shouldBe("use.transform.animVal.numberOfItems", "1");
    shouldBe("use.transform.baseVal.numberOfItems", "0");
    shouldBe("use.transform.animVal.getItem(0).matrix.e", "0");
    shouldBe("use.transform.animVal.getItem(0).matrix.f", "125");
}

function sample4() {
    shouldBe("rect.x.animVal.value", "35");
    shouldBe("rect.x.baseVal.value", "10");
    shouldBe("rect.y.animVal.value", "10");
    shouldBe("rect.y.baseVal.value", "10");
    shouldBeCloseEnough("use.x.animVal.value", "15");
    shouldBe("use.x.baseVal.value", "0");
    shouldBe("use.y.animVal.value", "0");
    shouldBe("use.y.baseVal.value", "0");
    shouldBe("use.transform.animVal.numberOfItems", "1");
    shouldBe("use.transform.baseVal.numberOfItems", "0");
    shouldBe("use.transform.animVal.getItem(0).matrix.e", "0");
    shouldBeCloseEnough("use.transform.animVal.getItem(0).matrix.f", "128.5");
}

function sample5() {
    shouldBe("rect.x.animVal.value", "45");
    shouldBe("rect.x.baseVal.value", "10");
    shouldBe("rect.y.animVal.value", "10");
    shouldBe("rect.y.baseVal.value", "10");
    shouldBe("use.x.animVal.value", "25");
    shouldBe("use.x.baseVal.value", "0");
    shouldBe("use.y.animVal.value", "0");
    shouldBe("use.y.baseVal.value", "0");
    shouldBe("use.transform.animVal.numberOfItems", "1");
    shouldBe("use.transform.baseVal.numberOfItems", "0");
    shouldBe("use.transform.animVal.getItem(0).matrix.e", "0");
    shouldBe("use.transform.animVal.getItem(0).matrix.f", "125.5");
}

function executeTest() {
    const expectedValues = [
        // [animationId, time, sampleCallback]
        ["animate1", 0.0,   sample1],
        ["animate1", 0.5,   sample2],
        ["animate1", 1.5,   sample3],
        ["animate1", 2.5,   sample4],
        ["animate1", 3.5,   sample5]
    ];

    runAnimationTest(expectedValues);
}

window.clickX = 50;
window.clickY = 50;
var successfullyParsed = true;
