
window.UIHelper = class UIHelper {
    static isIOS()
    {
        return navigator.userAgent.includes('iPhone');
    }

    static activateAt(x, y)
    {
        if (!testRunner.runUIScript || !this.isIOS()) {
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
        return promise.then(
            function () { testRunner.notifyDone(); },
            function (error) { testRunner.notifyDone(); return Promise.reject(error); });
    }
}
