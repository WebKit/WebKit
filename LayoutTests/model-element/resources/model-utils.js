
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

function sleepForSeconds(duration) {
    return new Promise(resolve => {
        setTimeout(resolve, duration * 1000);
    });
}

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

const assert_points_are_equal = (actual, expected) => {
    assert_equals(actual.x, expected.x, "x is not matched");
    assert_equals(actual.y, expected.y, "y is not matched");
    assert_equals(actual.z, expected.z, "z is not matched");
    assert_equals(actual.w, expected.w, "w is not matched");
};

const assert_3d_matrix_is_identity = (actual, isIdentity) => {
    assert_true(actual instanceof DOMMatrixReadOnly);
    assert_false(actual.is2D, "is2D");
    if (isIdentity)
        assert_true(actual.isIdentity, "isIdentity");
    else
        assert_false(actual.isIdentity, "isIdentity");
};

const assert_3d_matrix_equals = (actual, expected) => {
    assert_true(actual instanceof DOMMatrixReadOnly);
    assert_true(expected instanceof DOMMatrixReadOnly);

    assert_false(actual.is2D, "is2D");
    assert_false(expected.is2D, "is2D");
    let actualArray = actual.toFloat64Array();
    let expectedArray = expected.toFloat64Array();
    assert_equals(actualArray.length, expectedArray.length);
    for (var i = 0; i < actualArray.length; i++) {
        assert_equals(actualArray[i], expectedArray[i], "matrix array should match at index " + i);
    }
};

const assert_3d_matrix_not_equals = (actual, expected) => {
    assert_true(actual instanceof DOMMatrixReadOnly);
    assert_true(expected instanceof DOMMatrixReadOnly);

    assert_false(actual.is2D, "is2D");
    assert_false(expected.is2D, "is2D");
    let actualArray = actual.toFloat64Array();
    let expectedArray = expected.toFloat64Array();
    assert_equals(actualArray.length, expectedArray.length);
    var allEqual = true;
    for (var i = 0; i < actualArray.length; i++) {
        if (actualArray[i] != expectedArray[i]) {
            allEqual = false;
            break;
        }
    }
    assert_false(allEqual, "matrix arrays should not match");
};

const assert_3d_matrix_approx_equals = (actual, expected) => {
    assert_true(actual instanceof DOMMatrixReadOnly);
    assert_true(expected instanceof DOMMatrixReadOnly);

    assert_false(actual.is2D, "is2D");
    assert_false(expected.is2D, "is2D");
    let actualArray = actual.toFloat64Array();
    let expectedArray = expected.toFloat64Array();
    assert_equals(actualArray.length, expectedArray.length);
    for (var i = 0; i < actualArray.length; i++) {
        assert_approx_equals(actualArray[i], expectedArray[i], epsilon, "matrix array should match at index " + i);
    }
};
