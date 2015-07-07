description('Test that the HTML parser does not allow the nesting depth of elements to exceed 512.');

var depth = 514;
var markup = "";
var i;
for (i = 0; i < depth; ++i)
    markup += "<div id='d" + i + "'>";
var doc = document.implementation.createHTMLDocument();
doc.body.innerHTML = markup;

var d510 = doc.getElementById("d510");
var d511 = doc.getElementById("d511");
var d512 = doc.getElementById("d512");

shouldBe("d512.parentNode === d510", "true");
shouldBe("d511.parentNode === d510", "true");
shouldBe("d512.previousSibling === d511", "true");
