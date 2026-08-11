let contextIDs = [];

function trackContextForLeaks(context)
{
    let contextID = internals.baseAudioContextIdentifier(context);
    contextIDs.push(contextID);

    // Sanity check.
    if (!internals.isBaseAudioContextAlive(contextID))
        testFailed("Test harness failure: internals.isBaseAudioContextAlive() is not working as expected!");
}

function didGCAtLeastOneContext()
{
    for (let contextID of contextIDs) {
        if (!internals.isBaseAudioContextAlive(contextID))
            return true;
    }
    return false;
}

function gcAndCheckForContextLeaks()
{
    gc();
    setTimeout(() => {
        gc();
        shouldBeTrue("didGCAtLeastOneContext()");
        finishJSTest();
    }, 0);
}
