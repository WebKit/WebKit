description("Test path animation where coordinate modes of start and end differ. You should see PASS messages");
createSVGTestCase();
// FIXME: We should move to animatedPathSegList, once it is implemented.

// Setup test document
var path = createSVGElement("path");
path.setAttribute("id", "path");
path.setAttribute("d", "M -30 -30 q 30 0 30 30 t -30 30 z");
path.setAttribute("fill", "green");
path.setAttribute("onclick", "executeTest()");
path.setAttribute("transform", "translate(50, 50)");

var animate = createSVGElement("animate");
animate.setAttribute("id", "animation");
animate.setAttribute("attributeName", "d");
animate.setAttribute("from", "M -30 -30 q 30 0 30 30 t -30 30 z");
animate.setAttribute("to", "M -30 -30 Q 30 -30 30 0 T -30 30 Z");
animate.setAttribute("begin", "click");
animate.setAttribute("dur", "4s");
path.appendChild(animate);
rootSVGElement.appendChild(path);

// Setup animation test
function sample1() {
    // Check initial/end conditions
    shouldBe("path.pathSegList.getItem(0).pathSegTypeAsLetter", "'M'");
    shouldBe("path.pathSegList.getItem(0).x", "-30");
    shouldBe("path.pathSegList.getItem(0).y", "-30");
    shouldBe("path.pathSegList.getItem(1).pathSegTypeAsLetter", "'q'");
    shouldBe("path.pathSegList.getItem(1).x", "30");
    shouldBe("path.pathSegList.getItem(1).y", "30");
    shouldBe("path.pathSegList.getItem(1).x1", "30");
    shouldBe("path.pathSegList.getItem(1).y1", "0");
    shouldBe("path.pathSegList.getItem(2).pathSegTypeAsLetter", "'t'");
    shouldBe("path.pathSegList.getItem(2).x", "-30");
    shouldBe("path.pathSegList.getItem(2).y", "30");
}

function sample2() {
    shouldBe("path.pathSegList.getItem(0).pathSegTypeAsLetter", "'M'");
    shouldBeCloseEnough("path.pathSegList.getItem(0).x", "-30", 0.01);
    shouldBeCloseEnough("path.pathSegList.getItem(0).y", "-30", 0.01);
    shouldBe("path.pathSegList.getItem(1).pathSegTypeAsLetter", "'q'");
    shouldBeCloseEnough("path.pathSegList.getItem(1).x", "37.5", 0.01);
    shouldBeCloseEnough("path.pathSegList.getItem(1).y", "30", 0.01);
    shouldBeCloseEnough("path.pathSegList.getItem(1).x1", "37.5", 0.01);
    shouldBeCloseEnough("path.pathSegList.getItem(1).y1", "0", 0.01);
    shouldBe("path.pathSegList.getItem(2).pathSegTypeAsLetter", "'t'");
    shouldBeCloseEnough("path.pathSegList.getItem(2).x", "-37.5", 0.01);
    shouldBeCloseEnough("path.pathSegList.getItem(2).y", "30", 0.01);
}

function sample3() {
    shouldBe("path.pathSegList.getItem(0).pathSegTypeAsLetter", "'M'");
    shouldBeCloseEnough("path.pathSegList.getItem(0).x", "-30", 0.01);
    shouldBeCloseEnough("path.pathSegList.getItem(0).y", "-30", 0.01);
    shouldBe("path.pathSegList.getItem(1).pathSegTypeAsLetter", "'Q'");
    shouldBeCloseEnough("path.pathSegList.getItem(1).x", "22.5", 0.01);
    shouldBeCloseEnough("path.pathSegList.getItem(1).y", "0", 0.01);
    shouldBeCloseEnough("path.pathSegList.getItem(1).x1", "22.5", 0.01);
    shouldBeCloseEnough("path.pathSegList.getItem(1).y1", "-30", 0.01);
    shouldBe("path.pathSegList.getItem(2).pathSegTypeAsLetter", "'T'");
    shouldBeCloseEnough("path.pathSegList.getItem(2).x", "-30", 0.01);
    shouldBeCloseEnough("path.pathSegList.getItem(2).y", "30", 0.01);
}

function sample4() {
    shouldBe("path.pathSegList.getItem(0).pathSegTypeAsLetter", "'M'");
    shouldBeCloseEnough("path.pathSegList.getItem(0).x", "-30", 0.01);
    shouldBeCloseEnough("path.pathSegList.getItem(0).y", "-30", 0.01);
    shouldBe("path.pathSegList.getItem(1).pathSegTypeAsLetter", "'Q'");
    shouldBeCloseEnough("path.pathSegList.getItem(1).x", "30", 0.01);
    shouldBeCloseEnough("path.pathSegList.getItem(1).y", "0", 0.01);
    shouldBeCloseEnough("path.pathSegList.getItem(1).x1", "30", 0.01);
    shouldBeCloseEnough("path.pathSegList.getItem(1).y1", "-30", 0.01);
    shouldBe("path.pathSegList.getItem(2).pathSegTypeAsLetter", "'T'");
    shouldBeCloseEnough("path.pathSegList.getItem(2).x", "-30", 0.01);
    shouldBeCloseEnough("path.pathSegList.getItem(2).y", "30", 0.01);
}

function executeTest() {
    const expectedValues = [
        // [animationId, time, elementId, sampleCallback]
        ["animation", 0.0,    "path", sample1],
        ["animation", 1.0,    "path", sample2],
        ["animation", 3.0,    "path", sample3],
        ["animation", 3.9999, "path", sample4],
        ["animation", 4.0 ,   "path", sample1]
    ];

    runAnimationTest(expectedValues);
}

// Begin test async
window.setTimeout("triggerUpdate(50, 50)", 0);
var successfullyParsed = true;
