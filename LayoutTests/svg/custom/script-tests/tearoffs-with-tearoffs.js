description("Tests whether SVG tearoff objects with tearoffs update properly.");

var svgDoc = document.implementation.createDocument("http://www.w3.org/2000/svg", "svg", null);
var transform = svgDoc.documentElement.createSVGTransform();

shouldBe("transform.matrix.a", "1");
transform.matrix.a = 2;
shouldBe("transform.matrix.a", "2");

var successfullyParsed = true;
