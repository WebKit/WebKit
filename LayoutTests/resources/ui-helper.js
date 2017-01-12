
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
