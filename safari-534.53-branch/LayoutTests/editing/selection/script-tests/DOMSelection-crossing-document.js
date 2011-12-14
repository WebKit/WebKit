description("Test to check if setBaseAndExtent guard node with null owner document (Bug 31680)");

function makeEditableDocument(id)
{
    var iframe = document.createElement("iframe");
    document.body.appendChild(iframe);
    var doc = iframe.contentDocument;
    doc.body.innerHTML = "<html><body><div id='" + id + "' contentEditable>Editable Block for " + id + ".</div></body></html>";
    return doc;
}

var foreignDoc = makeEditableDocument("target");
var foreignElement = foreignDoc.getElementById("target");
var foreignText = foreignElement.firstChild;
var foreignSel = foreignDoc.getSelection();

var mainElement = document.createElement("div");
mainElement.contentEditable = true;
mainElement.innerHTML = "Main Text";
document.body.appendChild(mainElement);
var mainSel = window.getSelection();

function clear()
{
    foreignSel.setBaseAndExtent(null, 0, null, 0);
    mainSel.setBaseAndExtent(null, 0, null, 0);
}

mainSel.setBaseAndExtent(foreignElement, 0, foreignElement, 0);
shouldBeNull("foreignSel.anchorNode");
shouldBeNull("mainSel.anchorNode");

clear();
mainSel.setPosition(foreignElement, 0);
shouldBeNull("foreignSel.anchorNode");
shouldBeNull("mainSel.anchorNode");

clear();
mainSel.extend(foreignElement, 1);
shouldBeNull("foreignSel.anchorNode");
shouldBeNull("mainSel.anchorNode");

clear();
mainSel.selectAllChildren(foreignElement, 1);
shouldBeNull("foreignSel.anchorNode");
shouldBeNull("mainSel.anchorNode");

clear();
mainSel.collapse(foreignElement, 0);
shouldBeNull("foreignSel.anchorNode");
shouldBeNull("mainSel.anchorNode");

// Should not allow elements which come from another document.
clear();
mainSel.setBaseAndExtent(mainElement, 0, foreignElement, 0);
shouldBeNull("foreignSel.anchorNode");
shouldBeNull("mainSel.anchorNode");

var successfullyParsed = true;
