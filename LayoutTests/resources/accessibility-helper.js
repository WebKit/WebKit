function axDebug(msg)
{
    getOrCreate("console", "div").innerText += `${msg}\n`;
};

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

function visibleRange(axElement, {width, height, scrollTop}) {
    document.body.scrollTop = scrollTop;
    testRunner.setViewSize(width, height);
    return `Range with view ${width}x${height}, scrollTop ${scrollTop}: ${axElement.stringDescriptionOfAttributeValue("AXVisibleCharacterRange")}\n`;
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

function sleep(ms) {
    return new Promise(resolve => setTimeout(resolve, ms));
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

// Expect an expression to equal a value and return the result as a string.
// This is essentially the more ubiquitous `shouldBe` function from js-test,
// but returns the result as a string rather than `debug`ing to a console DOM element.
function expect(expression, expectedValue) {
    if (typeof expression !== "string")
        debug("WARN: The expression arg in bufferShouldBe() should be a string.");

    const evalExpression = `${expression} === ${expectedValue}`;
    if (eval(evalExpression))
        return `PASS: ${evalExpression}\n`;
    return `FAIL: ${expression} !== ${expectedValue}, was ${eval(expression)}\n`;
}

async function expectAsync(expression, expectedValue) {
    if (typeof expression !== "string")
        debug("WARN: The expression arg in expectAsync should be a string.");

    const evalExpression = `${expression} === ${expectedValue}`;
    await waitFor(() => {
        return eval(evalExpression);
    });
    return `PASS: ${evalExpression}\n`;
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

async function waitForFocus(id) {
    document.getElementById(id).focus();
    await waitFor(() => {
        const focusedElement = accessibilityController.focusedElement;
        return focusedElement && focusedElement.domIdentifier === id;
    });
}

function evalAndReturn(expression) {
    if (typeof expression !== "string")
        debug("FAIL: evalAndReturn() expects a string argument");

    eval(expression);
    return `${expression}\n`;
}
