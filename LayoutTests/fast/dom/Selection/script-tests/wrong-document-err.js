description("Test that collapse() and selectAllChildren() throw WRONG_DOCUMENT_ERR if the node is in another document.");

var sel = window.getSelection();
var iframe = document.createElement("iframe");
document.body.appendChild(iframe);
var iframe_doc = iframe.contentDocument;
var div = iframe_doc.createElement('div');
iframe_doc.body.appendChild(div);
var externalTextNode = iframe_doc.createTextNode("abcdef");
div.appendChild(externalTextNode);

var internalTextNode = iframe_doc.createTextNode("abcdef");
document.body.appendChild(internalTextNode);

shouldThrow("sel.collapse(externalTextNode, 0)", "'Error: WRONG_DOCUMENT_ERR: DOM Exception 4'");
shouldThrow("sel.selectAllChildren(externalTextNode)", "'Error: WRONG_DOCUMENT_ERR: DOM Exception 4'");
shouldBe("sel.collapse(internalTextNode, 0)", "undefined");
shouldBe("sel.selectAllChildren(internalTextNode)", "undefined");

document.body.removeChild(iframe);
document.body.removeChild(internalTextNode);

var successfullyParsed = true;
