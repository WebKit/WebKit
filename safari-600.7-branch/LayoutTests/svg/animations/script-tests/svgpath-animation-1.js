description("Test 'by' animation on path. You should see a green 100x100 path and only PASS messages");
createSVGTestCase();

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
animate.setAttribute("fill", "freeze");
animate.setAttribute("dur", "4s");
path.appendChild(animate);
rootSVGElement.appendChild(path);

// Setup animation test
function checkBaseVal() {
    shouldBe("path.pathSegList.numberOfItems", "5");
    shouldBeEqualToString("path.pathSegList.getItem(0).pathSegTypeAsLetter", "M");
    shouldBe("path.pathSegList.getItem(0).x", "40");
    shouldBe("path.pathSegList.getItem(0).y", "40");
    shouldBeEqualToString("path.pathSegList.getItem(1).pathSegTypeAsLetter", "L");
    shouldBe("path.pathSegList.getItem(1).x", "60");
    shouldBe("path.pathSegList.getItem(1).y", "40");
    shouldBeEqualToString("path.pathSegList.getItem(2).pathSegTypeAsLetter", "L");
    shouldBe("path.pathSegList.getItem(2).x", "60");
    shouldBe("path.pathSegList.getItem(2).y", "60");
    shouldBeEqualToString("path.pathSegList.getItem(3).pathSegTypeAsLetter", "L");
    shouldBe("path.pathSegList.getItem(3).x", "40");
    shouldBe("path.pathSegList.getItem(3).y", "60");
    shouldBeEqualToString("path.pathSegList.getItem(4).pathSegTypeAsLetter", "Z");
}

function sample1() {
    shouldBe("path.animatedPathSegList.numberOfItems", "5");
    shouldBeEqualToString("path.animatedPathSegList.getItem(0).pathSegTypeAsLetter", "M");
    shouldBeCloseEnough("path.animatedPathSegList.getItem(0).x", "40");
    shouldBeCloseEnough("path.animatedPathSegList.getItem(0).y", "40");
    shouldBeEqualToString("path.animatedPathSegList.getItem(1).pathSegTypeAsLetter", "L");
    shouldBeCloseEnough("path.animatedPathSegList.getItem(1).x", "60");
    shouldBeCloseEnough("path.animatedPathSegList.getItem(1).y", "40");
    shouldBeEqualToString("path.animatedPathSegList.getItem(2).pathSegTypeAsLetter", "L");
    shouldBeCloseEnough("path.animatedPathSegList.getItem(2).x", "60");
    shouldBeCloseEnough("path.animatedPathSegList.getItem(2).y", "60");
    shouldBeEqualToString("path.animatedPathSegList.getItem(3).pathSegTypeAsLetter", "L");
    shouldBeCloseEnough("path.animatedPathSegList.getItem(3).x", "40");
    shouldBeCloseEnough("path.animatedPathSegList.getItem(3).y", "60");
    shouldBeEqualToString("path.animatedPathSegList.getItem(4).pathSegTypeAsLetter", "Z");
    checkBaseVal();
}

function sample2() {
    shouldBe("path.animatedPathSegList.numberOfItems", "5");
    shouldBeEqualToString("path.animatedPathSegList.getItem(0).pathSegTypeAsLetter", "M");
    shouldBeCloseEnough("path.animatedPathSegList.getItem(0).x", "40");
    shouldBeCloseEnough("path.animatedPathSegList.getItem(0).y", "40");
    shouldBeEqualToString("path.animatedPathSegList.getItem(1).pathSegTypeAsLetter", "L");
    shouldBeCloseEnough("path.animatedPathSegList.getItem(1).x", "110");
    shouldBeCloseEnough("path.animatedPathSegList.getItem(1).y", "40");
    shouldBeEqualToString("path.animatedPathSegList.getItem(2).pathSegTypeAsLetter", "L");
    shouldBeCloseEnough("path.animatedPathSegList.getItem(2).x", "110");
    shouldBeCloseEnough("path.animatedPathSegList.getItem(2).y", "110");
    shouldBeEqualToString("path.animatedPathSegList.getItem(3).pathSegTypeAsLetter", "L");
    shouldBeCloseEnough("path.animatedPathSegList.getItem(3).x", "40");
    shouldBeCloseEnough("path.animatedPathSegList.getItem(3).y", "110");
    shouldBeEqualToString("path.animatedPathSegList.getItem(4).pathSegTypeAsLetter", "Z");
    checkBaseVal();
}

function sample3() {
    shouldBe("path.animatedPathSegList.numberOfItems", "5");
    shouldBeEqualToString("path.animatedPathSegList.getItem(0).pathSegTypeAsLetter", "M");
    shouldBeCloseEnough("path.animatedPathSegList.getItem(0).x", "40");
    shouldBeCloseEnough("path.animatedPathSegList.getItem(0).y", "40");
    shouldBeEqualToString("path.animatedPathSegList.getItem(1).pathSegTypeAsLetter", "L");
    shouldBeCloseEnough("path.animatedPathSegList.getItem(1).x", "160");
    shouldBeCloseEnough("path.animatedPathSegList.getItem(1).y", "40");
    shouldBeEqualToString("path.animatedPathSegList.getItem(2).pathSegTypeAsLetter", "L");
    shouldBeCloseEnough("path.animatedPathSegList.getItem(2).x", "160");
    shouldBeCloseEnough("path.animatedPathSegList.getItem(2).y", "160");
    shouldBeEqualToString("path.animatedPathSegList.getItem(3).pathSegTypeAsLetter", "L");
    shouldBeCloseEnough("path.animatedPathSegList.getItem(3).x", "40");
    shouldBeCloseEnough("path.animatedPathSegList.getItem(3).y", "160");
    shouldBeEqualToString("path.animatedPathSegList.getItem(4).pathSegTypeAsLetter", "Z");
    checkBaseVal();
}
function executeTest() {
    const expectedValues = [
        // [animationId, time, sampleCallback]
        ["animation", 0.0,   sample1],
        ["animation", 1.999, sample2],
        ["animation", 2.001, sample2],
        ["animation", 3.999, sample3],
        ["animation", 4.001, sample3]
    ];

    runAnimationTest(expectedValues);
}

var successfullyParsed = true;
