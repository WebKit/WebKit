function clearSelectionAndFocusOnWebArea() {
    webArea = accessibilityController.rootElement.childAtIndex(0);
    webArea.resetSelectedTextMarkerRange();
    setCaretBrowsingEnabled(webArea, false);
    output += expect("webArea.role", "'AXRole: AXWebArea'");
    output += expect("caretBrowsingEnabled(webArea)", "false");
    return webArea;
}

function elementAtStartMarkerOfSelectedTextMarkerRange(webArea) {
    var range = webArea.selectedTextMarkerRange();
    var start = webArea.startTextMarkerForTextMarkerRange(range);
    var element = webArea.accessibilityElementForTextMarker(start);
    return element;
}

function caretBrowsingEnabled(webArea) {
    return webArea.boolAttributeValue("AXCaretBrowsingEnabled");
}

function setCaretBrowsingEnabled(webArea, value) {
    webArea.setBoolAttributeValue("AXCaretBrowsingEnabled", value);
}

function selectTextInNode(nodeId) {
    var node = document.getElementById(nodeId);

    if (document.body.createTextRange) {
        const range = document.body.createTextRange();
        range.moveToElementText(node);
        range.select();
    } else if (window.getSelection) {
        const selection = window.getSelection();
        const range = document.createRange();
        range.selectNodeContents(node);
        selection.removeAllRanges();
        selection.addRange(range);
    } else {
        console.warn("Could not select text in node: Unsupported browser.");
    }
}
