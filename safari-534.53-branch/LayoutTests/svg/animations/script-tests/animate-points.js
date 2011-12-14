// FIXME: This test will become useful once we have basic animVal support. For now it's just testing the SVG animation test infrastructure
description("Tests if points of a polygon are animateable.");
createSVGTestCase();

// Setup test document
var poly = createSVGElement("polygon");
poly.setAttribute("id", "poly");
poly.setAttribute("fill", "green");
poly.setAttribute("points", "0,0 200,0 200,200 0,200");
poly.setAttribute("onclick", "executeTest()");

var animate = createSVGElement("animate");
animate.setAttribute("id", "animation");
animate.setAttribute("attributeName", "points");
animate.setAttribute("from", "0,0 200,0 200,200 0,200");
animate.setAttribute("to", "0,0 100,0 100,100 0,100");
animate.setAttribute("begin", "click");
animate.setAttribute("dur", "4s");
poly.appendChild(animate);
rootSVGElement.appendChild(poly);

// Setup animation test
function sample1() {
    // Check initial/end conditions
    shouldBe("poly.points.getItem(2).x", "200");
    shouldBe("poly.points.getItem(2).y", "200");
    //shouldBe("poly.animatedPoints.getItem(2).x", "200");
    //shouldBe("poly.animatedPoints.getItem(2).y", "200");
}

function sample2() {
    // FIXME: Add animVal support. Animates baseVal at the moment.
    // shouldBe("rect.width.animVal.value", "150");
    // shouldBe("rect.width.baseVal.value", "200");

    // Check half-time conditions
    shouldBe("poly.points.getItem(2).x", "150");
    shouldBe("poly.points.getItem(2).y", "150");
    //shouldBe("poly.animatedPoints.getItem(2).x", "150");
    //shouldBe("poly.animatedPoints.getItem(2).y", "150");
}

function sample3() {
    // FIXME: Add animVal support. Animates baseVal at the moment.
    // shouldBe("rect.width.animVal.value", "100");
    // shouldBe("rect.width.baseVal.value", "200");

    // Check just before-end conditions
    var ok = isCloseEnough(poly.points.getItem(2).x, 100, 0.01);
    if (ok)
        testPassed("poly.points.getItem(2).x is almost 100, just before-end");
    else
        testFailed("poly.points.getItem(2).x is NOT almost 100, as expected");
}

function executeTest() {
    const expectedValues = [
        // [animationId, time, elementId, sampleCallback]
        ["animation", 0.0,    "poly", sample1],
        ["animation", 2.0,    "poly", sample2],
        ["animation", 3.9999, "poly", sample3],
        ["animation", 4.0 ,   "poly", sample1]
    ];

    runAnimationTest(expectedValues);
}

// Begin test async
window.setTimeout("triggerUpdate(15, 30)", 0);
var successfullyParsed = true;
