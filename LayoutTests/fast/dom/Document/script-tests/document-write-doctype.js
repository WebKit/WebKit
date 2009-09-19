description("Test that iframe.contentWindow.document.write() can convert a document to strict mode -- https://bugs.webkit.org/show_bug.cgi?id=24336");

// Simple recurisve "give me a string to represent this tree" function
function treeAsString(node) {
    var string = node.nodeName;
    if (node.childNodes.length) {
        string += " [";
    }
    for (var x = 0; x < node.childNodes.length; x++) {
        if (x != 0) {
            string += ", ";
        }
        string += treeAsString(node.childNodes[x]);
    }

    if (node.childNodes.length) {
        string += "]";
    }
    return string;
}

function doctypeNodeName(iframeDocument) {
    // WebKit returns "html" (matching the specified name of the doctype source)
    // Gecko returns "HTML" and IE "#comment"
    // We don't care for this test, so we use this hack.
    return iframeDocument.firstChild.nodeName;
}

var iframe = document.createElement("iframe");
document.body.appendChild(iframe);
var iframeDocument = iframe.contentWindow.document;

debug("about:blank is quirksmode by default")
shouldBeEqualToString("iframeDocument.compatMode", "BackCompat");
debug("ensure that about:blank's DOM has an html and body element")
shouldBeEqualToString("treeAsString(iframeDocument)", "#document [HTML [HEAD, BODY]]");

iframeDocument.write("<!DocType html><html><body>test</body></html>");
debug("writing a doctype as the first document.write can change the document to standards")
shouldBeEqualToString("iframeDocument.compatMode", "CSS1Compat");
debug("ensure the written DOM has an html and body element")
shouldBeEqualToString("treeAsString(iframeDocument)", "#document [" + doctypeNodeName(iframeDocument) + ", HTML [HEAD, BODY [#text]]]");

// document.open() doesn't seem to clear the document as expected in Gecko
// https://bugzilla.mozilla.org/show_bug.cgi?id=483908
// All tests after this points are likely to fail in Firefox
iframe.contentWindow.document.open();
debug("ensure that document.open clears the document but does not change the document pointer")
shouldBe("iframeDocument", "iframe.contentWindow.document");
debug("document.open should also clear the document and reset the doctype)")
shouldBeEqualToString("treeAsString(iframeDocument)", "#document");
shouldBeEqualToString("iframeDocument.compatMode", "BackCompat");

debug("document.write of \"\" should leave the document in quirksmode and add no content to the document")
iframeDocument.write("");
shouldBeEqualToString("iframeDocument.compatMode", "BackCompat");
shouldBeEqualToString("treeAsString(iframeDocument)", "#document");

debug("document.write calls can change the doctype until an &lt;html> is created")
iframeDocument.write("<!DocType html><html><body>test</body></html>");
shouldBeEqualToString("iframeDocument.compatMode", "CSS1Compat");

debug("reset the document again");
iframe.contentWindow.document.open();

debug("document.write of \"&lt;html>\" should leave the document in quirksmode and add only an HTML element, no body")
iframeDocument.write("<html>");
shouldBeEqualToString("iframeDocument.compatMode", "BackCompat");
shouldBeEqualToString("treeAsString(iframeDocument)", "#document [HTML]");

debug("any document.write calls after &lt;html> has been encountered cannot change the doctype")
iframeDocument.write("<!DocType html><html><body>test</body></html>");
shouldBeEqualToString("iframeDocument.compatMode", "BackCompat");

document.body.removeChild(iframe);

var successfullyParsed = true;
