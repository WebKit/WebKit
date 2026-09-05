const createPortal = (test, { width, height, portalTransform, portalAction } = {}) => {
    const portal = document.createElement("div");
    portal.className = "portal";
    if (width)
        portal.style.width = width;
    if (height)
        portal.style.height = height;
    if (portalTransform)
        portal.style.portalTransform = portalTransform;
    if (portalAction)
        portal.style.portalAction = portalAction;
    document.body.appendChild(portal);
    test.add_cleanup(() => portal.remove());
    return portal;
};

const appendModel = (portal, asset, stageMode) => {
    const model = document.createElement("model");
    if (stageMode)
        model.stageMode = stageMode;
    portal.appendChild(model);
    const source = document.createElement("source");
    source.src = `../model-element/resources/${asset}`;
    model.appendChild(source);
    return model;
};

// Wraps the internals values back into a DOMMatrix.
const resolvedPortalTransform = portal => {
    const values = internals.spatialPortalResolvedTransform(portal);
    return values ? new DOMMatrixReadOnly(values) : null;
};

const portalTransformsApproxEqual = (a, b) => {
    if (!a || !b)
        return false;

    const aValues = a.toFloat64Array();
    const bValues = b.toFloat64Array();
    return aValues.every((value, index) => Math.abs(value - bValues[index]) <= epsilon);
};

const portalTransformScaleIsUnit = transform => {
    return !!transform
        && Math.abs(transform.m11 - 1) < epsilon
        && Math.abs(transform.m22 - 1) < epsilon
        && Math.abs(transform.m33 - 1) < epsilon;
};

const portalTransformIsResolved = transform => !!transform;
const portalTransformIsCleared = transform => transform === null;

const resolvedTransformForPortal = async (test, asset, portalTransform, portalOptions = { }) => {
    const portal = createPortal(test, { ...portalOptions, portalTransform });
    const model = appendModel(portal, asset);
    await model.ready;
    return await waitForPortalTransform(portal, portalTransformIsResolved, `portal-transform: ${portalTransform ?? "auto"}`);
};

async function waitFor(predicate, description, timeout = 5000) {
    const startTime = Date.now();

    while (!predicate()) {
        if (Date.now() - startTime > timeout)
            throw new Error(`Timeout waiting for ${description}`);

        await sleepForSeconds(0.05);
    }
}

async function waitForPortalTransform(portal, predicate, description, timeout = 5000) {
    const startTime = Date.now();

    while (true) {
        const transform = resolvedPortalTransform(portal);
        if (predicate(transform))
            return transform;

        if (Date.now() - startTime > timeout)
            throw new Error(`Timeout waiting for the portal's resolved transform: ${description}`);

        await sleepForSeconds(0.1);
    }
}

async function waitForEntityTransform(model, predicate, description, timeout = 5000) {
    const startTime = Date.now();

    while (true) {
        const transform = model.entityTransform;
        if (predicate(transform))
            return transform;

        if (Date.now() - startTime > timeout)
            throw new Error(`Timeout waiting for the child's entity transform: ${description}`);

        await sleepForSeconds(0.1);
    }
}

// The 3D matrix assertions reject a 2D argument outright, and DOMMatrix stays 2D until an operation touches z.
const as3d = matrix => new DOMMatrixReadOnly(matrix.toFloat64Array());

