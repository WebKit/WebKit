const presentationMode = element => internals.volumetricScenePresentationMode(element);

// Presentation is confirmed by the UI process, so every transition settles asynchronously.
const waitForPresentationMode = (test, element, expected) =>
    test.step_wait(() => presentationMode(element) === expected, `presentation mode becomes "${expected}"`);

// requestVolumetricScene() consumes transient activation, which each call needs afresh.
const requestVolumetricScene = element => {
    let promise;
    internals.withUserGesture(() => { promise = element.requestVolumetricScene(); });
    return promise;
};

// Matches the .portal rule each test carries, the same way the spatial-css tests establish a portal.
const createPortal = test => {
    const portal = document.createElement("div");
    portal.className = "portal";
    document.body.appendChild(portal);
    test.add_cleanup(() => portal.remove());
    return portal;
};

const appendModel = (parent, asset = "cube.usdz") => {
    const model = document.createElement("model");
    const source = document.createElement("source");
    source.src = `../resources/${asset}`;
    model.appendChild(source);
    parent.appendChild(model);
    return model;
};

// A portal only has a player once it hosts a loaded child, which is what a scene can be opened for.
const createPortalWithReadyModel = async test => {
    const portal = createPortal(test);
    const model = appendModel(portal);
    await model.ready;
    return [portal, model];
};
