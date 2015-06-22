function shouldBeAccessibleByCursor(accessibilityElement, displayName)
{
    console.assert(window.accessibilityController);
    var x = accessibilityElement.x + accessibilityElement.width / 2;
    var y = accessibilityElement.y + accessibilityElement.height / 2;
    var hitElement = accessibilityController.elementAtPoint(x, y);
    if (accessibilityElement.isEqual(hitElement))
        testPassed(displayName + " can be hit.");
    else {
        testFailed("should have hit " + displayName + " at (" + x + ", " + y + "). Hit element " + hitElement + ":");
        if (hitElement)
            debug(hitElement.allAttributes());
    }
}
