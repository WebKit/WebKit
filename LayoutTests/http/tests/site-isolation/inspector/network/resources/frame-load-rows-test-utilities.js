TestPage.registerInitializer(function() {

// Records the resources the Network table would list, from the same model events it uses.
window.FrameLoadRowRecorder = class FrameLoadRowRecorder
{
    constructor()
    {
        this._rows = new Set;

        WI.Frame.addEventListener(WI.Frame.Event.ResourceWasAdded, this._handleResourceWasAdded, this);
        WI.Frame.addEventListener(WI.Frame.Event.MainResourceDidChange, this._handleMainResourceDidChange, this);
        WI.Frame.addEventListener(WI.Frame.Event.ChildFrameWasAdded, this._handleChildFrameWasAdded, this);
    }

    // Public

    stop()
    {
        WI.Frame.removeEventListener(WI.Frame.Event.ResourceWasAdded, this._handleResourceWasAdded, this);
        WI.Frame.removeEventListener(WI.Frame.Event.MainResourceDidChange, this._handleMainResourceDidChange, this);
        WI.Frame.removeEventListener(WI.Frame.Event.ChildFrameWasAdded, this._handleChildFrameWasAdded, this);
    }

    rowsForURL(url)
    {
        return Array.from(this._rows).filter((resource) => resource.url === url);
    }

    rowsForURLPart(urlPart)
    {
        return Array.from(this._rows).filter((resource) => resource.url.includes(urlPart));
    }

    // Adds the rows the table already lists for loads that happened before recording started, as
    // NetworkTableContentView._populateWithInitialResourcesIfNeeded does.
    seedFromFrameTree(frame)
    {
        this._rows.add(frame.provisionalMainResource || frame.mainResource);
        for (let resource of frame.resourceCollection)
            this._rows.add(resource);
        for (let childFrame of frame.childFrameCollection)
            this.seedFromFrameTree(childFrame);
    }

    // Private

    _handleResourceWasAdded(event)
    {
        this._rows.add(event.data.resource);
    }

    _handleMainResourceDidChange(event)
    {
        let frame = event.target;
        if (frame.isMainFrame())
            this._rows.clear();
        this._rows.add(frame.mainResource);
    }

    _handleChildFrameWasAdded(event)
    {
        let frame = event.data.childFrame;
        this._rows.add(frame.provisionalMainResource || frame.mainResource);
    }
};

// How long the helpers below wait for the frontend model to catch up before reporting a failure.
window.awaitTimeout = 5000;

// For waits that include a process launch or a navigation, which can be slow under load.
window.awaitLaunchTimeout = 10000;

window.pollUntil = async function(condition, timeout = awaitTimeout) {
    let deadline = Date.now() + timeout;
    while (true) {
        let result = condition();
        if (result || Date.now() > deadline)
            return result || null;
        await new Promise((resolve) => setTimeout(resolve, 20));
    }
};

// Resolves to "pending" after the timeout, so a load that never finishes fails an assertion instead of timing out.
window.awaitResourceSettled = function(resource, timeout = awaitTimeout) {
    if (resource.finished)
        return Promise.resolve("finished");
    if (resource.failed)
        return Promise.resolve("failed");
    return Promise.race([
        resource.awaitEvent(WI.Resource.Event.LoadingDidFinish).then(() => "finished"),
        resource.awaitEvent(WI.Resource.Event.LoadingDidFail).then(() => "failed"),
        new Promise((resolve) => setTimeout(() => resolve("pending"), timeout)),
    ]);
};

// WI.networkManager.frames also lists frames whose load has not committed yet.
window.awaitChildFrame = function(predicate, timeout) {
    return pollUntil(() => Array.from(WI.networkManager.mainFrame?.childFrameCollection || []).find(predicate), timeout);
};

window.awaitSubresource = function(frame, urlPart) {
    return pollUntil(() => Array.from(frame.resourceCollection).find((resource) => resource.url.includes(urlPart)));
};

window.expectSingleFinishedResource = async function(recorder, resource, label) {
    let state = await awaitResourceSettled(resource);
    InspectorTest.expectEqual(recorder.rowsForURL(resource.url).length, 1, `${label} should be listed once.`);
    InspectorTest.expectEqual(state, "finished", `${label} should finish loading.`);
};

window.expectSingleFinishedDocument = async function(recorder, frame, label) {
    let resource = frame.mainResource;
    await expectSingleFinishedResource(recorder, resource, label);
    InspectorTest.expectThat(!!resource.requestIdentifier, `${label} should carry the request identifier of its load.`);
    InspectorTest.expectThat(!isNaN(resource.statusCode), `${label} should have a response.`);
};

window.expectSingleFinishedSubresource = async function(recorder, frame, urlPart, label) {
    let resource = await awaitSubresource(frame, urlPart);
    InspectorTest.expectThat(!!resource, `${label} should be added to the child frame.`);
    if (resource)
        await expectSingleFinishedResource(recorder, resource, label);
};

window.expectNoAboutBlankRow = function(recorder) {
    InspectorTest.expectEqual(recorder.rowsForURL("about:blank").length, 0, "No about:blank placeholder row should be listed.");
};

});
