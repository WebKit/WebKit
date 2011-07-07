description("Test fallback to 'to' animation if user specifies 'by' animation on path. You should see a green 100x100 path and only PASS messages");
createSVGTestCase();
// FIXME: We should move to animatePathSegList, once it is implemented.

// Setup test document
var path = createSVGElement("path");
path.setAttribute("id", "path");
path.setAttribute("d", "M 40 40 L 60 40 L 60 60 L 40 60 z");
path.setAttribute("fill", "green");
path.setAttribute("onclick", "executeTest()");

var animate = createSVGElement("animate");
animate.setAttribute("id", "animation");
animate.setAttribute("attributeName", "d");
animate.setAttribute("from", "M 40 40 L 60 40 L 60 60 L 40 60 z");
animate.setAttribute("by", "M 0 0 L 100 0 L 100 100 L 0 100 z");
animate.setAttribute("begin", "click");
animate.setAttribute("dur", "4s");
path.appendChild(animate);
rootSVGElement.appendChild(path);

// Setup animation test
function sample1() {
    // Check initial/end conditions
    shouldBe("path.pathSegList.getItem(0).x", "40");
    shouldBe("path.pathSegList.getItem(0).y", "40");
}

function sample2() {
    shouldBeCloseEnough("path.pathSegList.getItem(0).x", "20", 0.01);
    shouldBeCloseEnough("path.pathSegList.getItem(0).y", "20", 0.01);
}

function sample3() {
    shouldBeCloseEnough("path.pathSegList.getItem(0).x", "0", 0.01);
    shouldBeCloseEnough("path.pathSegList.getItem(0).y", "0", 0.01);
}

function executeTest() {
    const expectedValues = [
        // [animationId, time, elementId, sampleCallback]
        ["animation", 0.0,    "path", sample1],
        ["animation", 2.0,    "path", sample2],
        ["animation", 3.9999, "path", sample3],
        ["animation", 4.0 ,   "path", sample1]
    ];

    runAnimationTest(expectedValues);
}

// Begin test async
window.setTimeout("triggerUpdate(50, 50)", 0);
var successfullyParsed = true;
