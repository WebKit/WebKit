function checkDidSameOriginChildWindowLoadAndNotifyDone(childWindow)
{
    checkDidSameOriginChildWindowLoad(childWindow, () => {
        if (window.testRunner)
            testRunner.notifyDone();
    });
}

function checkDidSameOriginChildWindowLoad(childWindow, callback)
{
    function checkDidLoad() {
        if (childWindow.document.location.origin !== document.location.origin)
            return;
        // Child window did load
        window.clearInterval(intervalID);
        callback()
    }
    intervalID = window.setInterval(checkDidLoad, 10);
}
