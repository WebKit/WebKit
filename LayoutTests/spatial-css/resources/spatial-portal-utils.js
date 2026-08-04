const createPortal = (test, { width, height, portalTransform } = {}) => {
    const portal = document.createElement("div");
    portal.className = "portal";
    if (width)
        portal.style.width = width;
    if (height)
        portal.style.height = height;
    if (portalTransform)
        portal.style.portalTransform = portalTransform;
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
