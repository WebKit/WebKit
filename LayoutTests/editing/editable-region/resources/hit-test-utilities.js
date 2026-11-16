async function shouldHaveEditableElementsInRect(x, y, width, height)
{
    if (await UIHelper.mayContainEditableElementsInRect(x, y, width, height))
        testPassed(`(x = ${x}, y = ${y}, width = ${width}, height = ${height}) contains editable elements.`);
    else
        testFailed(`(x = ${x}, y = ${y}, width = ${width}, height = ${height}) should contain editable elements, but did not.`);
}

async function shouldNotHaveEditableElementsInRect(x, y, width, height)
{
    if (!await UIHelper.mayContainEditableElementsInRect(x, y, width, height))
        testPassed(`(x = ${x}, y = ${y}, width = ${width}, height = ${height}) does not contain editable elements.`);
    else
        testFailed(`(x = ${x}, y = ${y}, width = ${width}, height = ${height}) should not contain editable elements, but did.`);
}

function shouldHaveEditableElementsInRectForElement(element)
{
    return shouldHaveEditableElementsInRect(element.offsetLeft, element.offsetTop, element.offsetWidth, element.offsetHeight);
}

function shouldNotHaveEditableElementsInRectForElement(element)
{
    return shouldNotHaveEditableElementsInRect(element.offsetLeft, element.offsetTop, element.offsetWidth, element.offsetHeight);
}
