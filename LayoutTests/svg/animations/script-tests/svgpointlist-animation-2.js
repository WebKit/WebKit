description("Tests from-by animation of points on polygons.");
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
animate.setAttribute("by", "0,0 100,0 100,100 0,100");
animate.setAttribute("begin", "click");
animate.setAttribute("dur", "4s");
poly.appendChild(animate);
rootSVGElement.appendChild(poly);

// Setup animation test
function sample1() {
    // Check initial/end conditions
    shouldBeCloseEnough("poly.animatedPoints.getItem(2).x", "200");
    shouldBeCloseEnough("poly.animatedPoints.getItem(2).y", "200");

    shouldBe("poly.points.getItem(2).x", "200");
    shouldBe("poly.points.getItem(2).y", "200");
}

function sample2() {
    // Check half-time conditions
    shouldBeCloseEnough("poly.animatedPoints.getItem(2).x", "250");
    shouldBeCloseEnough("poly.animatedPoints.getItem(2).y", "250");

    shouldBe("poly.points.getItem(2).x", "200");
    shouldBe("poly.points.getItem(2).y", "200");
}

function sample3() {
    // Check just before-end conditions
    shouldBeCloseEnough("poly.animatedPoints.getItem(2).x", "300");
    shouldBeCloseEnough("poly.animatedPoints.getItem(2).y", "300");

    shouldBe("poly.points.getItem(2).x", "200");
    shouldBe("poly.points.getItem(2).y", "200");
}

function executeTest() {
    const expectedValues = [
        // [animationId, time, sampleCallback]
        ["animation", 0.0,   sample1],
        ["animation", 2.0,   sample2],
        ["animation", 3.999, sample3],
        ["animation", 4.001, sample1]
    ];

    runAnimationTest(expectedValues);
}

var successfullyParsed = true;
