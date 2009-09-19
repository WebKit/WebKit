description("This tests what event handler attributes are available on what objects.");

var elementAndDocumentProperties = [
    "onabort", "onblur", "onchange", "onclick", "oncontextmenu", "ondblclick", "ondrag", "ondragend",
    "ondragenter", "ondragleave", "ondragover", "ondragstart", "ondrop", "onerror", "onfocus", "oninput",
    "onkeydown", "onkeypress", "onkeyup", "onload", "onmousedown", "onmousemove", "onmouseout",
    "onmouseover", "onmouseup", "onmousewheel", "onscroll", "onselect", "onsubmit",

    // Not implemented yet
    // "oncanplay", "oncanplaythrough", "ondurationchange", "onemptied", "onended", "onformchange",
    // "onforminput", "oninvalid", "onloadeddata", "onloadedmetadata", "onloadstart", "onpause",
    // "onplay", "onplaying", "onprogress", "onratechange", "onreadystatechange", "onseeked", "onseeking",
    // "onshow", "onstalled", "onsuspend", "ontimeupdate", "onvolumechange", "onwaiting",

    // WebKit extensions
    "onbeforecut", "oncut", "onbeforecopy", "oncopy", "onbeforepaste", "onpaste", "onreset", "onsearch",
    "onselectstart"
];

var bodyAndFrameSetProperties = [
    "onbeforeunload", "onmessage", "onoffline", "ononline", "onresize", "onstorage", "onunload", "onblur",
    "onerror", "onfocus", "onload",

    // Not implemented yet.
    // "onafterprint", "onbeforeprint", "onhashchange", "onpopstate", "onredo", "onundo"
];


debug("Test Element");
var element = document.createElement("div");
for (var i = 0; i < elementAndDocumentProperties.length; ++i) {
    shouldBeTrue("'" + elementAndDocumentProperties[i] + "' in element");
}

debug("\nTest Document");
for (var i = 0; i < elementAndDocumentProperties.length; ++i) {
    shouldBeTrue("'" + elementAndDocumentProperties[i] + "' in document");
}

debug("\nTest Window");
for (var i = 0; i < elementAndDocumentProperties.length; ++i) {
    shouldBeTrue("'" + elementAndDocumentProperties[i] + "' in window");
}

debug("\nTest Text Node");
var textNode = document.createTextNode("text");
for (var i = 0; i < elementAndDocumentProperties.length; ++i) {
    shouldBeFalse("'" + elementAndDocumentProperties[i] + "' in textNode");
}

debug("\nTest Attribute");
var attribute = document.createAttribute("attr");
for (var i = 0; i < elementAndDocumentProperties.length; ++i) {
    shouldBeFalse("'" + elementAndDocumentProperties[i] + "' in attribute");
}

debug("\nTest HTMLBodyElement");
var body = document.body;
for (var i = 0; i < elementAndDocumentProperties.length; ++i) {
    shouldBeTrue("'" + elementAndDocumentProperties[i] + "' in body");
}
for (var i = 0; i < bodyAndFrameSetProperties.length; ++i) {
    shouldBeTrue("'" + bodyAndFrameSetProperties[i] + "' in body");
}

debug("\nTest HTMLFrameSetElement");
var frameSet = document.createElement("frameset");
for (var i = 0; i < elementAndDocumentProperties.length; ++i) {
    shouldBeTrue("'" + elementAndDocumentProperties[i] + "' in frameSet");
}
for (var i = 0; i < bodyAndFrameSetProperties.length; ++i) {
    shouldBeTrue("'" + bodyAndFrameSetProperties[i] + "' in frameSet");
}

successfullyParsed = true;
