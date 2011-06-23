description('Test that the HTML parser does not allow the nesting depth of "block-level" elements to exceed 2048.');

var depth = 2100;
var markup = "";
var i;
for (i = 0; i < depth; ++i)
    markup += "<div id='d" + i + "'>";
var doc = document.implementation.createHTMLDocument();
doc.body.innerHTML = markup;

var d2046 = doc.getElementById("d2046");
var d2047 = doc.getElementById("d2047");
var d2048 = doc.getElementById("d2048");

shouldBe("d2048.parentNode === d2046", "true");
shouldBe("d2047.parentNode === d2046", "true");
shouldBe("d2048.previousSibling === d2047", "true");

var successfullyParsed = true;
