description("This tests getTotalLength of SVG path.");

var pathElement = document.createElementNS("http://www.w3.org/2000/svg", "path");

function getTotalLength(string) {
    pathElement.setAttributeNS(null, "d", string);

    var point = pathElement.getTotalLength();
    return point;
}

shouldBe("getTotalLength('M0,20 L400,20 L640,20')", "640");
shouldBe("getTotalLength('M0,20 L400,20 L640,20 z')", "1280");
shouldBe("getTotalLength('M0,20 L400,20 z M 320,20 L640,20')", "1120");

var successfullyParsed = true;
