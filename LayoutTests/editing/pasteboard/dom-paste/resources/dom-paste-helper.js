
async function _waitForOrTriggerPasteMenu(x, y, proceedWithPaste, shouldActivate) {
    return new Promise(resolve => {
        testRunner.runUIScript(`
            (() => {
                doneCount = 0;
                function checkDone() {
                    if (++doneCount === (${shouldActivate} ? 3 : 2))
                        uiController.uiScriptComplete();
                }

                uiController.didHideMenuCallback = checkDone;

                function resolveDOMPasteRequest() {
                    if (${proceedWithPaste})
                        uiController.chooseMenuAction("Paste", checkDone);
                    else {
                        uiController.dismissMenu();
                        checkDone();
                    }
                }

                if (uiController.isShowingMenu)
                    resolveDOMPasteRequest();
                else
                    uiController.didShowMenuCallback = resolveDOMPasteRequest;

                if (${shouldActivate})
                    uiController.activateAtPoint(${x}, ${y}, checkDone);
            })()`, resolve);
    });
}

async function triggerPasteMenuAfterActivatingLocation(x, y, proceedWithPaste = true) {
    return _waitForOrTriggerPasteMenu(x, y, proceedWithPaste, true);
}

async function waitForPasteMenu(proceedWithPaste = true) {
    return _waitForOrTriggerPasteMenu(null, null, proceedWithPaste, false);
}
