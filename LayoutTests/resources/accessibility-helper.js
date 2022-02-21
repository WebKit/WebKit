// This function is necessary when printing AX attributes that are stringified with angle brackets:
//    AXChildren: <array of size 0>
// `debug` outputs to the `innerHTML` of a generated element, so these brackets must be escaped to be printed.
function debugEscaped(message) {
    debug(escapeHTML(message));
}

// Dumps the accessibility tree hierarchy for the given accessibilityObject into
// an element with id="tree", e.g., <pre id="tree"></pre>. In addition, it
// returns a two element array with the first element [0] being false if the
// traversal of the tree was stopped at the stopElement, and second element [1],
// the string representing the accessibility tree.
function dumpAccessibilityTree(accessibilityObject, stopElement, indent, allAttributesIfNeeded, getValueFromTitle, includeSubrole) {
    var str = "";
    var i = 0;

    for (i = 0; i < indent; i++)
        str += "    ";
    str += accessibilityObject.role;
    if (includeSubrole === true && accessibilityObject.subrole)
        str += " " + accessibilityObject.subrole;
    str += " " + (getValueFromTitle === true ? accessibilityObject.title : accessibilityObject.stringValue);
    str += allAttributesIfNeeded && accessibilityObject.role == '' ? accessibilityObject.allAttributes() : '';
    str += "\n";

    var outputTree = document.getElementById("tree");
    if (outputTree)
        outputTree.innerText += str;

    if (stopElement && stopElement.isEqual(accessibilityObject))
        return [false, str];

    var count = accessibilityObject.childrenCount;
    for (i = 0; i < count; ++i) {
        childRet = dumpAccessibilityTree(accessibilityObject.childAtIndex(i), stopElement, indent + 1, allAttributesIfNeeded, getValueFromTitle, includeSubrole);
        if (!childRet[0])
            return [false, str];
        str += childRet[1];
    }

    return [true, str];
}

function touchAccessibilityTree(accessibilityObject) {
    var count = accessibilityObject.childrenCount;
    for (var i = 0; i < count; ++i) {
        if (!touchAccessibilityTree(accessibilityObject.childAtIndex(i)))
            return false;
    }

    return true;
}

function platformValueForW3CName(accessibilityObject, includeSource=false) {
    var result;
    if (accessibilityController.platformName == "atspi")
        result = accessibilityObject.title
    else
        result = accessibilityObject.description

    if (!includeSource) {
        var splitResult = result.split(": ");
        return splitResult[1];
    }

    return result;
}

function platformValueForW3CDescription(accessibilityObject, includeSource=false) {
    var result;
    if (accessibilityController.platformName == "atspi")
        result = accessibilityObject.description
    else
        result = accessibilityObject.helpText;

    if (!includeSource) {
        var splitResult = result.split(": ");
        return splitResult[1];
    }

    return result;
}

function platformTextAlternatives(accessibilityObject, includeTitleUIElement=false) {
    if (!accessibilityObject)
        return "Element not exposed";

    result = "\t" + accessibilityObject.title + "\n\t" + accessibilityObject.description;
    if (accessibilityController.platformName == "mac")
       result += "\n\t" + accessibilityObject.helpText;
    if (includeTitleUIElement)
        result += "\n\tAXTitleUIElement: " + (accessibilityObject.titleUIElement() ? "non-null" : "null");
    return result;
}

function platformRoleForComboBox() {
    return accessibilityController.platformName == "atspi" ? "AXRole: AXComboBox" : "AXRole: AXPopUpButton";
}

function platformRoleForStaticText() {
    return accessibilityController.platformName == "atspi" ? "AXRole: AXStatic" : "AXRole: AXStaticText";
}

function spinnerForTextInput(accessibilityObject) {
    var index = accessibilityController.platformName == "atspi" ? 0 : 1;
    return accessibilityObject.childAtIndex(index);
}

function waitFor(condition)
{
    return new Promise((resolve, reject) => {
        let interval = setInterval(() => {
            if (condition()) {
                clearInterval(interval);
                resolve();
            }
        }, 0);
    });
}

async function waitForElementById(id) {
    let element;
    await waitFor(() => {
        element = accessibilityController.accessibleElementById(id);
        return element;
    });
    return element;
}

async function expectAsyncExpression(expression, expectedValue) {
    if (typeof expression !== "string")
        debug("WARN: The expression arg in waitForExpression() should be a string.");

    const evalExpression = `${expression} === ${expectedValue}`;
    await waitFor(() => {
        return eval(evalExpression);
    });
    debug(`PASS ${evalExpression}`);
}

