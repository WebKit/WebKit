const createPortal = test => {
    const portal = document.createElement("div");
    portal.className = "portal";
    document.body.appendChild(portal);
    test.add_cleanup(() => portal.remove());
    return portal;
};

const appendModel = (portal, asset) => {
    const model = document.createElement("model");
    portal.appendChild(model);
    const source = document.createElement("source");
    source.src = `../model-element/resources/${asset}`;
    model.appendChild(source);
    return model;
};
