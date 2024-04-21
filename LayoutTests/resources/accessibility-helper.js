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

// Dumps the AX table by using the table cellForColumnAndRow API (which is what some ATs use).
function dumpAXTable(axElement, options) {
    let output = "";
    const rowCount = axElement.rowCount;
    const columnCount = axElement.columnCount;

    for (let row = 0; row < rowCount; row++) {
        for (let column = 0; column < columnCount; column++)
            output += `#${axElement.domIdentifier} cellForColumnAndRow(${column}, ${row}).domIdentifier is ${axElement.cellForColumnAndRow(column, row).domIdentifier}\n`;
    }
    return output;
}

// Dumps the result of a traversal via the UI element search API (which is what some ATs use).
// `options` is an object with these keys:
//   * `excludeRoles`: Array of strings representing roles you don't want to include in the output.
//                     Case insensitive, partial match is fine, e.g. "scrollbar" will exclude "AXScrollBar".
function dumpAXSearchTraversal(axElement, options = { }) {
    let output = "";
    let searchResult = null;
    while (true) {
        searchResult = axElement.uiElementForSearchPredicate(searchResult, true, "AXAnyTypeSearchKey", "", false);
        if (!searchResult)
            break;

        const role = searchResult.role;

        let excluded = false;
        if (Array.isArray(options?.excludeRoles)) {
            for (const excludedRole of options.excludeRoles) {
                if (role.toLowerCase().includes(excludedRole.toLowerCase())) {
                    excluded = true;
                    break;
                }
            }
        }

        if (excluded)
            continue;

        const id = searchResult.domIdentifier;
        let resultDescription = `${id ? `#${id} ` : ""}${role}`;
        if (role.includes("StaticText"))
            resultDescription += ` ${accessibilityController.platformName === "ios" ? searchResult.description : searchResult.stringValue}`;

        output += `\n{${resultDescription}}\n`;
    }
    return output;
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
    str += " " + (getValueFromTitle === true ? accessibilityObject.title : accessibilityController.platformName === "ios" ? accessibilityObject.description : accessibilityObject.stringValue);
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

    if (accessibilityController.platformName === "ios")
        return `\t${accessibilityObject.description}`;

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
        debug("WARN: The expression arg in expect() should be a string.");

    const evalExpression = `${expression} === ${expectedValue}`;
    if (eval(evalExpression))
        return `PASS: ${evalExpression}\n`;
    return `FAIL: ${expression} !== ${expectedValue}, was ${eval(expression)}\n`;
}

function expectRectWithVariance(expression, x, y, width, height, allowedVariance) {
    if (typeof expression !== "string")
        debug("WARN: The expression arg in expectRectWithVariance() must be a string.");
    if (typeof x !== "number" || typeof y !== "number" || typeof width !== "number" || typeof height !== "number" || typeof allowedVariance !== "number")
        debug("WARN: The x, y, width, height, and allowedVariance arguments in expectRectWithVariance must be numbers.");

    const expectedRect = `(x: ${x}, y: ${y}, w: ${width}, h: ${height})`;

    const result = eval(expression);
    const parsedResult = result
        .replaceAll(/{|}/g, '') // Eliminate curly braces because they break parseFloat.
        .split(/[ ,]+/) // Split on whitespace and commas.
        .map(token => parseFloat(token))
        .filter(float => !isNaN(float));

    if (parsedResult.length !== 4)
        debug(`FAIL: Expression ${expression} didn't produce a string result with four numbers (was ${result}).\n`);
    else if (Math.abs(x - parsedResult[0])      > allowedVariance ||
             Math.abs(y - parsedResult[1])      > allowedVariance ||
             Math.abs(width - parsedResult[2])  > allowedVariance ||
             Math.abs(height - parsedResult[3]) > allowedVariance)
        return `FAIL: ${expression} varied more than allowed variance ${allowedVariance}. Was: ${result}, expected ${expectedRect}\n`;
    else
        return `PASS: ${expression} was ${allowedVariance === 0 ? "equal" : "equal or approximately equal"} to ${expectedRect}.\n`;
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
    let focusedElement;
    await waitFor(() => {
        focusedElement = accessibilityController.focusedElement;
        return focusedElement && focusedElement.domIdentifier === id;
    });
    return focusedElement;
}

function evalAndReturn(expression) {
    if (typeof expression !== "string")
        debug("FAIL: evalAndReturn() expects a string argument");

    eval(expression);
    return `${expression}\n`;
}

function logActiveElementAndSelectedChildren(axElement) {
    output += `    activeElement: ${axElement.activeElement.domIdentifier}\n`;
    var children = axElement.selectedChildren();
    output += "    selectedChildren: [ ";
    for (let i = 0; i < children.length; ++i) {
        if (i > 0)
            output += ", ";
        output += children[i].domIdentifier;
    }
    output += " ]\n";
}

function resetActiveElementAndSelectedChildren(id) {
    Array.from(document.getElementById(id).children).forEach((child) => {
        child.removeAttribute("aria-activedescendant");
        child.removeAttribute("aria-selected");
    });
}
