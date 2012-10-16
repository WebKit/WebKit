window.jsTestIsAsync = true;
if (window.internals)
    internals.settings.setEnableMockPagePopup(true);

var popupWindow = null;

function valueForEntry(element) {
    if (!element)
        return null;
    var value = element.dataset.value;
    if (typeof value === "string")
        return value;
    var action = element.dataset.action;
    if (typeof action === "string")
        return "@" + action;
    return null;
}

function highlightedEntry() {
    return valueForEntry(popupWindow.document.activeElement);
}

function entryValues() {
    var elements = popupWindow.document.getElementsByClassName("suggestion-list-entry");
    var values = [];
    for (var i = 0; i < elements.length; ++i)
        values.push(valueForEntry(elements[i]));
    return values;
}

var popupOpenCallback = null;
function openPicker(input, callback) {
    input.offsetTop; // Force to lay out
    sendKey(input, "Down", false, true);
    popupWindow = document.getElementById('mock-page-popup').contentWindow;
    if (typeof callback === "function") {
        popupOpenCallback = callback;
        popupWindow.addEventListener("resize", popupOpenCallbackWrapper, false);
    }
}

function popupOpenCallbackWrapper() {
    popupWindow.removeEventListener("resize", popupOpenCallbackWrapper);
    popupOpenCallback();
}

function sendKey(input, keyName, ctrlKey, altKey) {
    var event = document.createEvent('KeyboardEvent');
    event.initKeyboardEvent('keydown', true, true, document.defaultView, keyName, 0, ctrlKey, altKey);
    input.dispatchEvent(event);
}
