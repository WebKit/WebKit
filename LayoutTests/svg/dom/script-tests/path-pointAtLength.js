description("This tests getPointAtLength of SVG path.");

var pathElement = document.createElementNS("http://www.w3.org/2000/svg", "path");

function pointAtLengthOfPath(string) {
    pathElement.setAttributeNS(null, "d", string);

    var point = pathElement.getPointAtLength(700);
    return "(" + Math.round(point.x) + ", " + Math.round(point.y) + ")";
}

shouldBe("pointAtLengthOfPath('M0,20 L400,20 L640,20')", "'(640, 20)'");
shouldBe("pointAtLengthOfPath('M0,20 L400,20 L640,20 z')", "'(580, 20)'");
shouldBe("pointAtLengthOfPath('M0,20 L400,20 z M 320,20 L640,20')", "'(100, 20)'");
shouldBe("pointAtLengthOfPath('M0,20 L20,40')", "'(20, 40)'");

var successfullyParsed = true;
