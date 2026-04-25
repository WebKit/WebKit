function failTest(errorMessage) {
    if (window.testRunner) {
        document.body.innerText = errorMessage;
        testRunner.notifyDone();
    } else {
        console.error(errorMessage);
    }
}

function passTest() {
    if (window.testRunner) {
        document.body.innerText = "PASS";
        testRunner.notifyDone();
    } else
        console.log("PASS");
}

function setupTestCase(options = {}) {
    if (window.testRunner) {
        testRunner.waitUntilDone();
        testRunner.dumpAsText();
        if (options.disableConsoleLog)
            console.log = () => {};

        if (!window.internals) {
            failTest("FAIL: this test case requires internals");
        } else {
            try {
                window.internals.getFrameDamageHistory();
            } catch (error) {
                failTest(`FAIL: ${error.name} - ${error.message}`);
            }
        }
    }
}

var failure = null;

function assert(condition, failureMessage) {
    if (failure)
        return false;
    if (!condition) {
        failure = failureMessage;
        return false;
    }
    return true;
}

function assertEq(actual, expected, failureMessage) {
    return assert(actual == expected, `${failureMessage}, expected: ${expected} but got: ${actual}`);
}

function assertGt(actual, threshold, failureMessage) {
    return assert(actual > threshold, `${failureMessage}, ${actual} is not greater than ${threshold}`);
}

function assertRectsEq(damageRects, expectedRects) {
    const rectCompareFunction = (a, b) => {
        for (var i = 0; i < 4; i++) {
            if (a[i] != b[i])
                return a[i] - b[i];
        }
        return 0;
    };
    damageRects.sort(rectCompareFunction);
    expectedRects.sort(rectCompareFunction);
    const damageRectsStr = JSON.stringify(damageRects);
    const expectedRectsStr =  JSON.stringify(expectedRects);
    return assert(
        damageRectsStr == expectedRectsStr,
        `damage rects mismatch, expected: ${expectedRectsStr} but got: ${damageRectsStr}`
    );
}

function assertContains(containerRect, allegedContainee) {
    const containerRectStr = JSON.stringify(containerRect);
    const allegedContaineeStr =  JSON.stringify(allegedContainee);
    return assert(
        contains(containerRect, allegedContainee),
        `${containerRectStr} does not contain ${allegedContaineeStr}`
    );
}

function processAnimationFrameSequence(options, callbackSequence, callbackIndex) {
    if (options.skipFirstFrameToEnsureInitialPaintingDone)
        callbackSequence.unshift(() => {});
    if (callbackSequence.length <= callbackIndex) {
        passTest();
        return;
    }
    requestAnimationFrame(() => {
        console.log("Processing requestAnimationFrame callback #" + callbackIndex);
        callbackSequence[callbackIndex]();
        if (failure) {
            failTest(`FAIL: ${failure}`);
            return;
        }
        processAnimationFrameSequence({}, callbackSequence, callbackIndex + 1);
    });
}

function allFramesDamages() {
    const damageDetails = window.internals.getFrameDamageHistory();
    return _simplifyDamages(damageDetails);
}

function latestFrameDamage() {
    var damages = allFramesDamages();
    if (damages.length == 0)
        return null;
    return damages.at(-1);
}

function log(entity) {
    console.log(JSON.stringify(entity));
}

function createNewElement(elementName, lambda = (el) => {}) {
    var newElement = document.createElement(elementName);
    lambda(newElement);
    return newElement;
}

function createNewElementWithClass(elementName, className, lambda = (el) => {}) {
    var newElement = document.createElement(elementName);
    newElement.className = className;
    lambda(newElement);
    return newElement;
}

function spawnNewElement(elementName, lambda = (el) => {}) {
    var newElement = createNewElement(elementName, lambda);
    document.body.appendChild(newElement);
    return newElement;
}

function spawnNewElementWithClass(elementName, className, lambda = (el) => {}) {
    var newElement = createNewElementWithClass(elementName, className, lambda);
    document.body.appendChild(newElement);
    return newElement;
}

async function takeCanvasSnapshotAsBlobURL(canvas) {
    return new Promise(resolve =>
        canvas.toBlob(blob => resolve(URL.createObjectURL(blob)))
    );
}

function contains(containerRect, allegedContainee) {
    return (
        containerRect[0] <= allegedContainee[0]
        && containerRect[1] <= allegedContainee[1]
            && ((containerRect[0] + containerRect[2]) >= (allegedContainee[0] + allegedContainee[2]))
            && ((containerRect[1] + containerRect[3]) >= (allegedContainee[1] + allegedContainee[3]))
    );
}

function _simplifyDamages(damages) {
    return damages.map(damage => _simplifyDamage(damage));
}

function _simplifyDamage(damage) {
    var obj = {
        bounds: _simplifyDamageRect(damage.bounds),
        rects: damage.rects.map(r => _simplifyDamageRect(r)),
    };
    obj.toStr = function () {
        return JSON.stringify(this);
    };
    return obj;
}

function _simplifyDamageRect(damageRect) {
    return [damageRect.x, damageRect.y, damageRect.width, damageRect.height];
}
