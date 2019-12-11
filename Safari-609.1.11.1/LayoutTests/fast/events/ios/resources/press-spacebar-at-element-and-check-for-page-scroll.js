window.jsTestIsAsync = true;

let g_testElement;
let g_testContainer;
let g_activationEventName;
let checkForScrollTimerId;

function done()
{
    document.body.removeChild(g_testContainer);
    finishJSTest();
}

function handleInteraction(event)
{
    function checkForScrollAndDone() {
        window.removeEventListener("scroll", handleScroll);
        shouldBeZero("window.scrollY");
        done();
    }
    // FIXME: Wait up to 100ms for a possible scroll to happen. Note that we will
    // end the test as soon as a scroll happens.
    checkForScrollTimerId = window.setTimeout(checkForScrollAndDone, 100);
}

function handleScroll()
{
    testFailed("Page should not have scrolled.");

    window.clearTimeout(checkForScrollTimerId);
    g_testElement.removeEventListener(g_activationEventName, handleInteraction);
    done();
}

function checkActivatingElementUsingSpacebarDoesNotScrollPage(testContainer, testElement, activationEventName)
{
    g_testContainer = testContainer;
    g_testElement = testElement;
    g_activationEventName = activationEventName;

    function handleFocus(event) {
        console.assert(event.target == g_testElement);
        window.addEventListener("scroll", handleScroll, { once: true });
        g_testElement.addEventListener(g_activationEventName, handleInteraction, { once: true });
        if (window.testRunner)
            UIHelper.keyDown(" ");
    }
    g_testElement.addEventListener("focus", handleFocus, { once: true });
    if (window.testRunner)
        UIHelper.keyDown("\t", ["altKey"]);
}
