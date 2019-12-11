function testWaitUntilNonPromiseParameters()
{
    var event = new ExtendableEvent('ExtendableEvent', {});
    try {
        event.waitUntil(new Request(''));
        return 'Should throw';
    } catch (e) {
        return e.name === 'InvalidStateError' ? 'PASS' : 'Got exception ' + e;
    }
}

function testExtendableEvent()
{
    if (new ExtendableEvent('ExtendableEvent').type !== 'ExtendableEvent')
        return 'Type of ExtendableEvent should be ExtendableEvent';
    if (new ExtendableEvent('ExtendableEvent', {}).type !== 'ExtendableEvent')
        return 'Type of ExtendableEvent should be ExtendableEvent';
    if (new ExtendableEvent('ExtendableEvent', {}).cancelable !== false)
        return 'Default ExtendableEvent.cancelable should be false';
    if (new ExtendableEvent('ExtendableEvent', {}).bubbles !== false)
        return 'Default ExtendableEvent.bubbles should be false';
    if (new ExtendableEvent('ExtendableEvent', {cancelable: false}).cancelable !== false)
        return 'ExtendableEvent.cancelable should be false';
    return "PASS";
}

async function doTest(event)
{
    try {
        var result = event.data + " is an unknown test";
        if (event.data === "waitUntil-non-promise-parameters")
            result = testWaitUntilNonPromiseParameters();
        else if (event.data === "extendable-event")
            result = testExtendableEvent();

        event.source.postMessage(result);
    } catch (e) {
        event.source.postMessage("Exception: " + e.message);
    }
}

self.addEventListener("message", doTest);
