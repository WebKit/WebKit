description("This test checks that the id property on an SVGElement reflects the corresponding attribute.");

var elementNames = ['g', 'tspan', 'foreignObject'];

for (var i = 0; i < elementNames.length; i++) {
    var elementName = elementNames[i];
    var element = document.createElementNS("http://www.w3.org/2000/svg", elementName);
    this[elementName] = element;

    shouldBeEqualToString(elementName + ".id", "");

    element.setAttribute("id", "abc");
    shouldBeEqualToString(elementName + ".id", "abc");

    element.id = "def";
    shouldBeEqualToString(elementName + ".getAttribute('id')", "def");
}

successfullyParsed = true;
