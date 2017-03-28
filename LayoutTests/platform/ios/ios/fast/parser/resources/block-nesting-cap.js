description('Test that the HTML parser does not allow the nesting depth of "block-level" elements to exceed 768.');

var depth = 772;
var markup = "";
var i;
for (i = 0; i < depth; ++i)
    markup += "<div id='d" + i + "'>";
var doc = document.implementation.createHTMLDocument();
doc.body.innerHTML = markup;

var d766 = doc.getElementById("d766");
var d767 = doc.getElementById("d767");
var d768 = doc.getElementById("d768");

shouldBe("d768.parentNode === d766", "true");
shouldBe("d768.parentNode === d766", "true");
shouldBe("d768.previousSibling === d767", "true");

var successfullyParsed = true;
