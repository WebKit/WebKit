
const isDebug = false;

function logDebug(callback)
{
    if (isDebug)
        console.log(callback());
}

function pause(duration) {
    return new Promise(resolve => {
        setTimeout(() => {
            resolve();
        }, duration);
    });
}

function dispatchMouseActions(actions)
{
    if (!window.eventSender)
        return Promise.reject(new Error("window.eventSender is undefined."));

    return new Promise(resolve => {
        setTimeout(async () => {
            eventSender.dragMode = false;

            for (let action of actions) {
                switch (action.type) {
                case "pointerMove":
                    let origin = { x: 0, y: 0 };
                    if (action.origin instanceof Element) {
                        const bounds = action.origin.getBoundingClientRect();
                        logDebug(() => `${action.origin.id} [${bounds.left}, ${bounds.top}, ${bounds.width}, ${bounds.height}]`);
                        origin.x = bounds.left + 1;
                        origin.y = bounds.top + 1;
                    }
                    logDebug(() => `eventSender.mouseMoveTo(${action.x + origin.x}, ${action.y + origin.y})`);
                    eventSender.mouseMoveTo(action.x + origin.x, action.y + origin.y);
                    break;
                case "pointerDown":
                    logDebug(() => `eventSender.mouseDown()`);
                    eventSender.mouseDown(action.button);
                    break;
                case "pointerUp":
                    logDebug(() => `eventSender.mouseUp()`);
                    eventSender.mouseUp(action.button);
                    break;
                case "pause":
                    logDebug(() => `pause(${action.duration})`);
                    await pause(action.duration);
                    break;
                default:
                    return Promise.reject(new Error(`Unknown action type "${action.type}".`));
                }
            }
            resolve();
        });
    });
}

function dispatchTouchActions(actions, options = { insertPauseAfterPointerUp: false })
{
    if (!window.testRunner || typeof window.testRunner.runUIScript !== "function")
        return Promise.reject(new Error("window.testRunner.runUIScript() is undefined."));

    let x = 0;
    let y = 0;
    let timeOffset = 0;
    let pointerDown = false;
    let id = 1;

    const events = [];
    for (let action of actions) {
        const touch = {
            inputType : "finger",
            id,
            x : action.x,
            y : action.y,
            pressure : 0
        };

        const command = {
            inputType : "hand",
            coordinateSpace : "content",
            touches : [touch],
            timeOffset
        };

        let timeOffsetIncrease = 0;

        switch (action.type) {
        case "pointerMove":
            touch.phase = "moved";
            if (action.origin instanceof Element) {
                const bounds = action.origin.getBoundingClientRect();
                touch.x += bounds.left + 1;
                touch.y += bounds.top + 1;
            }
            break;
        case "pointerDown":
            pointerDown = true;
            touch.phase = "began";
            if (action.x === undefined)
                touch.x = x;
            if (action.y === undefined)
                touch.y = y;
            break;
        case "pointerUp":
            pointerDown = false;
            touch.phase = "ended";
            touch.x = x;
            touch.y = y;
            id++;
            // We need to add a pause after a pointer up to ensure that a subsequent tap may be recognized as such.
            if (options.insertPauseAfterPointerUp)
                timeOffsetIncrease = 0.5;
            break;
        case "pause":
            timeOffsetIncrease = action.duration / 1000;
            break;
        default:
            return Promise.reject(new Error(`Unknown action type "${action.type}".`));
        }

        if (action.type !== "pause") {
            x = touch.x;
            y = touch.y;
        }

        if (!pointerDown && touch.phase == "moved")
            continue;

        timeOffset += timeOffsetIncrease;

        if (action.type !== "pause")
            events.push(command);
    }

    const stream = JSON.stringify({ events });
    logDebug(() => stream);

    return new Promise(resolve => testRunner.runUIScript(`(function() {
        (function() { uiController.sendEventStream('${stream}') })();
        uiController.uiScriptComplete();
    })();`, resolve));
}

if (window.test_driver_internal === undefined)
    window.test_driver_internal = { };

window.test_driver_internal.send_keys = function(element, keys)
{
    if (!window.eventSender)
        return Promise.reject(new Error("window.eventSender is undefined."));

    // https://seleniumhq.github.io/selenium/docs/api/py/webdriver/selenium.webdriver.common.keys.html
    // FIXME: Add more cases.
    // FIXME: Add the support for modifier keys.
    const SeleniumCharCodeToEventSenderKey = {
        0xE003: 'delete',
        0xE004: '\t',
        0xE00D: ' ',
        0xE012: 'leftArrow',
        0xE013: 'upArrow',
        0xE014: 'rightArrow',
        0xE015: 'downArrow',
    };

    function convertSeleniumKeyCode(key)
    {
        const code = key.charCodeAt(0);
        return SeleniumCharCodeToEventSenderKey[code] || key;
    }

    const keyList = [];
    for (const key of keys)
        keyList.push(convertSeleniumKeyCode(key));

    if (testRunner.isIOSFamily && testRunner.isWebKit2) {
        return new Promise((resolve) => {
            testRunner.runUIScript(`
                const keyList = JSON.parse('${JSON.stringify(keyList)}');
                for (const key of keyList)
                    uiController.keyDown(key);`, resolve);
        });
    }

    for (const key of keyList)
        eventSender.keyDown(key);
    return Promise.resolve();
}

window.test_driver_internal.click = function (element, coords)
{
    if (!window.testRunner)
        return Promise.resolve();

    if (!window.eventSender)
        return Promise.reject(new Error("window.eventSender is undefined."));

    if (testRunner.isIOSFamily && testRunner.isWebKit2) {
        return new Promise((resolve) => {
            testRunner.runUIScript(`
                uiController.singleTapAtPoint(${coords.x}, ${coords.y}, function() {
                    uiController.uiScriptComplete();
                });`, resolve);
        });
    }

    eventSender.mouseMoveTo(coords.x, coords.y);
    eventSender.mouseDown();
    eventSender.mouseUp();

    return Promise.resolve();
}

window.test_driver_internal.action_sequence = function(sources)
{
    // https://w3c.github.io/webdriver/#processing-actions    

    let noneSource;
    let pointerSource;
    for (let source of sources) {
        switch (source.type) {
        case "none":
            noneSource = source;
            break;
        case "pointer":
            pointerSource = source;
            break;
        default:
            return Promise.reject(new Error(`Unknown source type "${action.type}".`));
        }
    }

    if (!pointerSource)
        return Promise.reject(new Error(`Unknown pointer type pointer type "${action.parameters.pointerType}".`));

    const pointerType = pointerSource.parameters.pointerType;
    if (pointerType !== "mouse" && pointerType !== "touch")
        return Promise.reject(new Error(`Unknown pointer type "${pointerType}".`));

    // If we have a "none" source, let's inject any pause with non-zero durations into the pointer source
    // after the matching action in the pointer source.
    if (noneSource) {
        let injectedActions = 0;
        noneSource.actions.forEach((action, index) => {
            if (action.duration > 0) {
                pointerSource.actions.splice(index + injectedActions + 1, 0, action);
                injectedActions++;
            }
        });
    }

    logDebug(() => JSON.stringify(pointerSource));

    if (pointerType === "touch")
        return dispatchTouchActions(pointerSource.actions);
    if (testRunner.isIOSFamily && "createTouch" in document)
        return dispatchTouchActions(pointerSource.actions, { insertPauseAfterPointerUp: true });
    if (pointerType === "mouse")
        return dispatchMouseActions(pointerSource.actions);
};
