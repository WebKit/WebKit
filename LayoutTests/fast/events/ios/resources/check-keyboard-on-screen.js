async function checkKeyboardOnScreen()
{
    if (await UIHelper.isShowingKeyboard())
        testPassed("Keyboard is on screen.");
    else
        testFailed("Keyboard should be on screen, but is not.");
}

async function checkKeyboardNotOnScreen()
{
    if (await UIHelper.isShowingKeyboard())
        testFailed("Keyboard should not be on screen, but it is.");
    else
        testPassed("Keyboard is not on screen.");
}
