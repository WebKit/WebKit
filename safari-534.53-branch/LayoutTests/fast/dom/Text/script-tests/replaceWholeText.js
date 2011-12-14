description("Test wholeText and replaceWholeText")

var para = document.createElement('p');
para.appendChild(document.createTextNode('A'));
var textB = document.createTextNode('B');
para.appendChild(textB);
para.appendChild(document.createElement('p'));
para.appendChild(document.createTextNode('C'));

shouldBe("textB.wholeText", "'AB'");
shouldBe("para.textContent", "'ABC'");
textB.replaceWholeText("XYZ");
shouldBe("textB.wholeText", "'XYZ'");
shouldBe("para.textContent", "'XYZC'");

var successfullyParsed = true;
