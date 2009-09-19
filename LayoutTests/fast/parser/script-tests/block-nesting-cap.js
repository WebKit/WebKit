description('Test that the HTML parser does not allow the nesting depth of "block-level" elements to exceed 4096.');

var depth = 4100;
var markup = "";
var i;
for (i = 0; i < depth; ++i)
    markup += "<div id='d" + i + "'>";
var doc = document.implementation.createHTMLDocument();
doc.body.innerHTML = markup;

var d4094 = doc.getElementById("d4094");
var d4095 = doc.getElementById("d4095");
var d4096 = doc.getElementById("d4096");

shouldBe("d4096.parentNode === d4094", "true");
shouldBe("d4095.parentNode === d4094", "true");
shouldBe("d4096.previousSibling === d4095", "true");

var successfullyParsed = true;
