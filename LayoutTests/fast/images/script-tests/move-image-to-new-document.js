description("Test that when images are moved to a new document, a new load fires.");

if (window.testRunner)
    testRunner.setPluginsEnabled(false);

jsTestIsAsync = true;

var subframe = document.createElement("iframe");
document.body.appendChild(subframe);
var subdocument = subframe.contentDocument;

var types = [ "img", "input", "object", "embed" ];
var i = 0;
var element;

function imageLoadedInSecondDocument()
{
    element.removeEventListener("load", imageLoadedInSecondDocument, false);
    testPassed("Load event fired for " + element.tagName + " in both documents.");
    subdocument.body.removeChild(element);
    testNextType();
}

function imageLoadedInFirstDocument()
{
    element.removeEventListener("load", imageLoadedInFirstDocument, false);
    document.body.removeChild(element);
    subdocument.body.appendChild(element);
    element.addEventListener("load", imageLoadedInSecondDocument, false);
}

function testNextType()
{
    if (i >= types.length) {
        document.body.removeChild(subframe);
        finishJSTest();
        return;
    }
    var type = types[i++];
    element = document.createElement(type);
    if (type == "input")
        element.type = "image";
    element.data = "resources/test-load.jpg";
    element.src = "resources/test-load.jpg";
    document.body.appendChild(element);
    element.addEventListener("load", imageLoadedInFirstDocument, false);
}

testNextType();
