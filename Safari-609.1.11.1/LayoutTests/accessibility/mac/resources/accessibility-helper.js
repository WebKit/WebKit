function clearSelectionAndFocusOnWebArea() {
    webArea = accessibilityController.rootElement.childAtIndex(0);
    webArea.resetSelectedTextMarkerRange();
    setCaretBrowsingEnabled(webArea, false);
    shouldBe("webArea.role", "'AXRole: AXWebArea'");
    shouldBe("caretBrowsingEnabled(webArea)", "false");
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
