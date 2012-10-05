window.jsTestIsAsync = true;
if (window.internals)
    internals.settings.setEnableMockPagePopup(true);

var popupWindow = null;

function highlightedEntry() {
    var activeElement = popupWindow.document.activeElement;
    if (!activeElement)
        return null;
    var value = activeElement.dataset.value;
    if (typeof value === "string")
        return value;
    var action = activeElement.dataset.action;
    if (typeof action === "string")
        return "@" + action;
    return null;
}

function openPicker(input) {
    input.offsetTop; // Force to lay out
    sendKey(input, "Down", false, true);
    popupWindow = document.getElementById('mock-page-popup').contentWindow;
}

function sendKey(input, keyName, ctrlKey, altKey) {
    var event = document.createEvent('KeyboardEvent');
    event.initKeyboardEvent('keydown', true, true, document.defaultView, keyName, 0, ctrlKey, altKey);
    input.dispatchEvent(event);
}
