function dumpAccessibilityTree(accessibilityObject, stopElement, indent, allAttributesIfNeeded, getValueFromTitle) {
    var str = "";
    var i = 0;

    for (i = 0; i < indent; i++)
        str += "    ";
    str += accessibilityObject.role;
    str += " " + (getValueFromTitle === true ? accessibilityObject.title : accessibilityObject.stringValue);
    str += allAttributesIfNeeded && accessibilityObject.role == '' ? accessibilityObject.allAttributes() : '';
    str += "\n";

    document.getElementById("tree").innerText += str;

    if (stopElement && stopElement.isEqual(accessibilityObject))
        return;

    var count = accessibilityObject.childrenCount;
    for (i = 0; i < count; ++i) {
        if (!dumpAccessibilityTree(accessibilityObject.childAtIndex(i), stopElement, indent + 1, allAttributesIfNeeded, getValueFromTitle))
            return false;
    }

    return true;
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
    if (accessibilityController.platformName == "atk")
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
    if (accessibilityController.platformName == "atk")
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
    return accessibilityController.platformName == "atk" ? "AXRole: AXComboBox" : "AXRole: AXPopUpButton";
}

function platformRoleForStaticText() {
    return accessibilityController.platformName == "atk" ? "AXRole: AXStatic" : "AXRole: AXStaticText";
}
