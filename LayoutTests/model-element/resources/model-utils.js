
const makeModel = (options = { interactive: false }) => {
    const source = document.createElement("source");
    source.src = "resources/cube.usdz";
    const model = document.createElement("model");
    if (options.interactive)
        model.setAttribute("interactive", "");
    model.appendChild(source);
    return model;
}

const bodyAvailability = async () => {
    return new Promise(resolve => {
        if (document.body)
            resolve();
        else
            window.addEventListener("DOMContentLoaded", event => resolve());
    });
};

const readyModel = async (test, options) => {
    await bodyAvailability();
    const model = document.body.appendChild(makeModel(options));
    test.add_cleanup(() => model.remove());
    await model.ready;
    return model;
};

const epsilon = 0.001;

const assert_cameras_are_equal = (actualCamera, expectedCamera, description) => {
    assert_approx_equals(actualCamera.pitch, expectedCamera.pitch, epsilon, `${description}: camera has expected pitch`);
    assert_approx_equals(actualCamera.yaw, expectedCamera.yaw, epsilon, `${description}: camera has expected yaw`);
    assert_approx_equals(actualCamera.scale, expectedCamera.scale, epsilon, `${description}: camera has expected scale`);
};

const assert_cameras_are_not_equal = (actualCamera, expectedCamera, description) => {
    const camerasMatch = Math.abs(actualCamera.pitch - expectedCamera.pitch) > epsilon &&
                         Math.abs(actualCamera.yaw - expectedCamera.yaw) > epsilon &&
                         Math.abs(actualCamera.scale - expectedCamera.scale) > epsilon;
    assert_false(camerasMatch, description);
};
