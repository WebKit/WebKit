description("This test that the DOM attribute event handlers are avaiable on the only elements and documents, and not all other nodes such as attributes and text nodes.");

var properties = ["onabort", "onblur", "onchange", "onclick", "oncontextmenu", "ondblclick", "ondrag", "ondragend", "ondragenter", "ondragleave", "ondragover", "ondragstart", "ondrop", "onerror", "onfocus", "oninput", "onkeydown", "onkeypress", "onkeyup", "onload", "onmousedown", "onmousemove", "onmouseout", "onmouseover", "onmouseup", "onmousewheel", "onscroll", "onselect", "onsubmit", "onbeforecut", "oncut", "onbeforecopy", "oncopy", "onbeforepaste", "onpaste", "onreset", "onresize", "onsearch", "onselectstart", "onunload"];

debug("Test Element");
var element = document.createElement("div");
for (var i = 0; i < properties.length; ++i) {
    shouldBeTrue("'" + properties[i] + "' in element");
}

debug("\nTest Document");
for (var i = 0; i < properties.length; ++i) {
    shouldBeTrue("'" + properties[i] + "' in document");
}

debug("\nTest Text Node");
var textNode = document.createTextNode("text");
for (var i = 0; i < properties.length; ++i) {
    shouldBeFalse("'" + properties[i] + "' in textNode");
}

debug("\nTest Attribute");
var attribute = document.createAttribute("attr");
for (var i = 0; i < properties.length; ++i) {
    shouldBeFalse("'" + properties[i] + "' in attribute");
}

successfullyParsed = true;
