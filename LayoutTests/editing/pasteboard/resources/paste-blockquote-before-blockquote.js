
function copyAndPasteNode(nodeName) {
    var range = document.createRange();

    // Test with a single blockquote
    var nodeToCopy = document.getElementById(nodeName);
    range.setStartBefore(nodeToCopy);
    range.setEndAfter(nodeToCopy);

    var sel = window.getSelection();
    sel.addRange(range);
    document.execCommand("Copy");

    var pasteDiv = document.getElementById("pasteDiv");
    sel.setPosition(pasteDiv, 0);
    document.execCommand("Paste");

    console.log(pasteDiv.children[1] == undefined
        ? "FAILED: pasting " + nodeName + " DID keep the newline within the blockquote"
        : "SUCCESS: pasting " + nodeName + " DID NOT keep the newline within the blockquote.");
}

