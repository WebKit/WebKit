function checkDidSameOriginChildWindowLoadAndNotifyDone(childWindow)
{
    checkDidSameOriginChildWindowLoad(childWindow, () => {
        if (window.testRunner)
            testRunner.notifyDone();
    });
}

function checkDidSameOriginChildWindowLoad(childWindow, callback)
{
    childWindow.onload = callback;
}
