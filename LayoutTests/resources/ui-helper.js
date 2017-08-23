
window.UIHelper = class UIHelper {
    static isIOS()
    {
        return navigator.userAgent.includes('iPhone');
    }

    static isWebKit2()
    {
        return window.testRunner.isWebKit2;
    }

    static tapAt(x, y)
    {
        console.assert(this.isIOS());

        if (!this.isWebKit2()) {
            eventSender.addTouchPoint(x, y);
            eventSender.touchStart();
            eventSender.releaseTouchPoint(0);
            eventSender.touchEnd();
            return Promise.resolve();
        }

        return new Promise((resolve) => {
            testRunner.runUIScript(`
                uiController.singleTapAtPoint(${x}, ${y}, function() {
                    uiController.uiScriptComplete('Done');
                });`, resolve);
        });
    }

    static activateAt(x, y)
    {
        if (!this.isWebKit2() || !this.isIOS()) {
            eventSender.mouseMoveTo(x, y);
            eventSender.mouseDown();
            eventSender.mouseUp();
            return Promise.resolve();
        }

        return new Promise((resolve) => {
            testRunner.runUIScript(`
                uiController.singleTapAtPoint(${x}, ${y}, function() {
                    uiController.uiScriptComplete('Done');
                });`, resolve);
        });
    }

    static ensurePresentationUpdate()
    {
        if (!this.isWebKit2()) {
            testRunner.display();
            return Promise.resolve();
        }

        return new Promise(resolve => {
            testRunner.runUIScript(`
                uiController.doAfterPresentationUpdate(function() {
                    uiController.uiScriptComplete('Done');
                });`, resolve);
        });
    }

    static activateAndWaitForInputSessionAt(x, y)
    {
        if (!this.isWebKit2() || !this.isIOS())
            return this.activateAt(x, y);

        return new Promise(resolve => {
            testRunner.runUIScript(`
                (function() {
                    uiController.didShowKeyboardCallback = function() {
                        uiController.uiScriptComplete("Done");
                    };
                    uiController.singleTapAtPoint(${x}, ${y}, function() { });
                })()`, resolve);
        });
    }

    static getUICaretRect()
    {
        if (!this.isWebKit2() || !this.isIOS())
            return Promise.resolve();

        return new Promise(resolve => {
            testRunner.runUIScript(`(function() {
                uiController.doAfterNextStablePresentationUpdate(function() {
                    uiController.uiScriptComplete(JSON.stringify(uiController.textSelectionCaretRect));
                });
            })()`, jsonString => {
                resolve(JSON.parse(jsonString));
            });
        });
    }

    static getUISelectionRects()
    {
        if (!this.isWebKit2() || !this.isIOS())
            return Promise.resolve();

        return new Promise(resolve => {
            testRunner.runUIScript(`(function() {
                uiController.doAfterNextStablePresentationUpdate(function() {
                    uiController.uiScriptComplete(JSON.stringify(uiController.selectionRangeViewRects));
                });
            })()`, jsonString => {
                resolve(JSON.parse(jsonString));
            });
        });
    }

    static wait(promise)
    {
        testRunner.waitUntilDone();
        if (window.finishJSTest)
            window.jsTestIsAsync = true;

        let finish = () => {
            if (window.finishJSTest)
                finishJSTest();
            else
                testRunner.notifyDone();
        }

        return promise.then(finish, finish);
    }
}
