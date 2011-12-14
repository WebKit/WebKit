description('Test that the HTML parser does not allow the nesting depth of any elements to exceed MAX_DOM_TREE_DEPTH.');

var depth = 5100;
var markup = "";
for (var i = 3; i < depth; ++i)
    markup += "<span id='s" + i + "'>";
var doc = document.implementation.createHTMLDocument();
doc.body.innerHTML = markup;

var s1 = doc.getElementById("s5001");
var s2 = doc.getElementById("s5002");
var s3 = doc.getElementById("s5003");

shouldBeTrue("s3.parentNode === s1");
shouldBeTrue("s2.parentNode === s1");
shouldBeTrue("s3.previousSibling === s2");

var successfullyParsed = true;
