function focusedElementDescription()
{
    var element = accessibilityController.focusedElement;
    return element.helpText + ', ' +  element.valueDescription + ', intValue:' + element.intValue + ', range:'+ element.minValue + '-' + element.maxValue;
}

function checkFocusedElementAXAttributes(expected) {
    shouldBeEqualToString('focusedElementDescription()', expected);
}
