
async function _waitForOrTriggerPasteMenu(x, y, proceedWithPaste, shouldTap) {
    return new Promise(resolve => {
        testRunner.runUIScript(`
            (() => {
                doneCount = 0;
                function checkDone() {
                    if (++doneCount === (${shouldTap} ? 3 : 2))
                        uiController.uiScriptComplete();
                }

                uiController.didHideMenuCallback = checkDone;
                uiController.didShowMenuCallback = () => {
                    if (${proceedWithPaste}) {
                        const rect = uiController.rectForMenuAction("Paste");
                        uiController.singleTapAtPoint(rect.left + rect.width / 2, rect.top + rect.height / 2, checkDone);
                    } else {
                        uiController.resignFirstResponder();
                        checkDone();
                    }
                };

                if (${shouldTap})
                    uiController.singleTapAtPoint(${x}, ${y}, checkDone);
            })()`, resolve);
    });
}

async function triggerPasteMenuAfterTapAt(x, y, proceedWithPaste = true) {
    return _waitForOrTriggerPasteMenu(x, y, proceedWithPaste, true);
}

async function waitForPasteMenu(proceedWithPaste = true) {
    return _waitForOrTriggerPasteMenu(null, null, proceedWithPaste, false);
}
