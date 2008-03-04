var svgNS = "http://www.w3.org/2000/svg";

var svgRoot = document.createElementNS(svgNS, "svg");
document.documentElement.appendChild(svgRoot);

var svgText = document.createElementNS(svgNS, "text");
svgText.style.fontFamily = "Ahem";
svgText.style.fontSize = "20px";
svgText.appendChild(document.createTextNode("abc"));
svgRoot.appendChild(svgText);

shouldThrow("svgText.getSubStringLength(-1, 2)");
shouldThrow("svgText.getSubStringLength(-1, 0)");
shouldThrow("svgText.getSubStringLength(1, 3)");
shouldThrow("svgText.getSubStringLength(0, 4)");
shouldThrow("svgText.getSubStringLength(3, 0)");

shouldBe("svgText.getSubStringLength(0, 0)", "0");
shouldBe("svgText.getSubStringLength(2, 0)", "0");

shouldBe("svgText.getSubStringLength(0, 1)", "20");
shouldBe("svgText.getSubStringLength(1, 1)", "20");
shouldBe("svgText.getSubStringLength(2, 1)", "20");
shouldBe("svgText.getSubStringLength(0, 3)", "60");

// We throw on negative values, unlike SVG 1.1, in agreement with Acid3.
shouldThrow("svgText.getSubStringLength(1, -1)");
shouldThrow("svgText.getSubStringLength(2, -1)");
shouldThrow("svgText.getSubStringLength(3, -1)");
shouldThrow("svgText.getSubStringLength(3, -3)");

// cleanup
document.documentElement.removeChild(svgRoot);

var successfullyParsed = true;
