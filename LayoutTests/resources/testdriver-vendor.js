const isDebug = false;

function logDebug(msg) {
    if (isDebug)
        console.log(msg);
}

/**
 * Pause execution for the given duration.
 *
 * @param {Number} duration - Milliseconds
 * @returns Promise<undefined>
 */
function pause(duration) {
    return new Promise(resolve => {
        setTimeout(resolve, duration);
    });
}

/**
 *
 * @param {object[]} actions
 * @param {"pointerMove" | "pointerDown" | "pointerUp" | "pause"} pointerType
 * @returns Promise<undefined>
 */
async function dispatchMouseActions(actions, pointerType)
{
    await pause(0);

    eventSender.dragMode = false;

    for (let action of actions) {
        switch (action.type) {
        case "pointerMove":
            const origin = { x: 0, y: 0 };
            const actionWindow = action.origin?.ownerDocument?.defaultView;
            if (actionWindow && action.origin instanceof actionWindow.Element) {
                const bounds = action.origin.getBoundingClientRect();
                logDebug(`${action.origin.id} [${bounds.left}, ${bounds.top}, ${bounds.width}, ${bounds.height}]`);
                origin.x = bounds.left + (bounds.width / 2.0);
                origin.y = bounds.top + (bounds.height / 2.0);
            }
            logDebug(`eventSender.mouseMoveTo(${action.x + origin.x}, ${action.y + origin.y})`);
            eventSender.mouseMoveTo(action.x + origin.x, action.y + origin.y, pointerType);
            break;
        case "pointerDown":
            logDebug(`eventSender.mouseDown()`);
            eventSender.mouseDown(action.button, [], pointerType);
            break;
        case "pointerUp":
            logDebug(`eventSender.mouseUp()`);
            eventSender.mouseUp(action.button, [], pointerType);
            break;
        case "pause":
            logDebug(`pause(${action.duration})`);
            await pause(action.duration);
            break;
        default:
            throw new Error(`Unknown action type "${action.type}".`);
        }
    }
}

/**
 *
 * @param {object[]} actions
 * @param {object} options
 * @param {boolean} options.insertPauseAfterPointerUp
 * @returns Promise<undefined>
 */
async function dispatchTouchActions(actions, options = { insertPauseAfterPointerUp: false })
{
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
            const actionWindow = action.origin?.ownerDocument?.defaultView;
            if (actionWindow && action.origin instanceof actionWindow.Element) {
                const bounds = action.origin.getBoundingClientRect();
                touch.x += bounds.left + (bounds.width / 2.0);
                touch.y += bounds.top + (bounds.height / 2.0);
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
            throw new Error(`Unknown action type "${action.type}".`);
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
    logDebug(stream);

    return new Promise(resolve => testRunner.runUIScript(`(function() {
        (function() { uiController.sendEventStream('${stream}') })();
        uiController.uiScriptComplete();
    })();`, resolve));
}

function computeTickDuration(actions, sourceType)
{
    sourceToActionWithDurationMap = { "pointer": "pointerMove", "wheel": "scroll" };
    maxDuration = 0;
    for (let action of actions) {
        let duration;
        if (action.type === "pause" || action.type === sourceToActionWithDurationMap[sourceType])
            duration = action.duration;
        if (duration !== undefined) {
            if (duration > maxDuration)
                maxDuration = duration;
        }
    }
    return maxDuration;
}

async function renderingUpdate()
{
    await new Promise(requestAnimationFrame);
    await new Promise(resolve => setTimeout(resolve, 0));
}

async function waitForScrollCompletion()
{
    if (testRunner.isIOSFamily) {
        await new Promise(resolve => {
            testRunner.runUIScript(`
                (function() {
                    uiController.didEndScrollingCallback = function() {
                        uiController.uiScriptComplete();
                    }
                })()`, resolve);
        });
        return renderingUpdate();
    }

    return new Promise(resolve => {
        eventSender.callAfterScrollingCompletes(() => {
            requestAnimationFrame(resolve);
        });
    });
}

async function ensurePresentationUpdate()
{
    if (!testRunner.isWebKit2)
        return renderingUpdate();

    return new Promise(resolve => {
        testRunner.runUIScript(`
            uiController.doAfterPresentationUpdate(function() {
                uiController.uiScriptComplete();
            });`, resolve);
    });
}

/**
 *
 * @param {object[]} actions
 * @returns Promise<undefined>
 */
async function dispatchWheelActions(actions)
{
    if (!window.eventSender)
        throw new Error("window.eventSender is undefined.");

    for (let action of actions) {
        switch (action.type) {
        case "pause":
            logDebug(`pause(${action.duration})`);
            await pause(action.duration);
            break;
        case "scroll":
            // FIXME(261810): Follow tick-based event dispatch logic rather than sending scroll events instantaneously
            // https://w3c.github.io/webdriver/#dfn-perform-a-scroll
            let { x, y, origin, deltaX, deltaY, duration } = action;
            if ((x < 0) || (x > window.innerWidth) || (y < 0) || (y > window.innerHeight))
                throw new Error('Move target out of bounds');
            if (duration === undefined)
                duration = computeTickDuration(actions, "wheel");

            const originWindow = origin?.ownerDocument?.defaultView;
            if (originWindow && origin instanceof originWindow.Element) {
                const bounds = origin.getBoundingClientRect();
                logDebug(() => `${origin.id} [${bounds.left}, ${bounds.top}, ${bounds.width}, ${bounds.height}]`);
                x += bounds.left + (bounds.width / 2.0);
                y += bounds.top + (bounds.height / 2.0);
            }
            const scrollEvents = [
                {
                    type : "wheel",
                    viewX : x,
                    viewY : y,
                    deltaX : 0,
                    deltaY : -deltaY,
                    phase : "began"
                },
                {
                    type : "wheel",
                    deltaX : -deltaX,
                    deltaY : 0,
                    phase : "changed"
                },
                {
                    type : "wheel",
                    momentumPhase : "ended"
                }
            ];
            eventSender.monitorWheelEvents();
            await ensurePresentationUpdate();
            const eventStreamAsString = JSON.stringify({ events: scrollEvents });
            await new Promise(resolve => {
                testRunner.runUIScript(`
                    (function() {
                        uiController.sendEventStream(\`${eventStreamAsString}\`, () => {
                            uiController.uiScriptComplete();
                        });
                    })();
                `, resolve);
            });
            await waitForScrollCompletion();
            break;
        default:
            throw new Error(`Unrecognized wheel action type: ${action.type}`);
        }
    }
}

if (window.test_driver_internal === undefined)
    window.test_driver_internal = { };

// https://w3c.github.io/webdriver/#keyboard-actions
// FIXME: Add more cases.
const SeleniumCharCodeToEventSenderKey = {
    0xE003: { key: 'delete' },
    0XE004: { key: '\t' },
    0XE005: { key: 'clear' },
    0XE006: { key: '\r' },
    0XE007: { key: '\n' },
    0xE008: { key: 'leftShift', modifier: 'shiftKey' },
    0xE009: { key: 'leftControl', modifier: 'ctrlKey' },
    0xE00A: { key: 'leftAlt', modifier: 'altKey' },
    0XE00C: { key: 'escape' },
    0xE00D: { key: ' ' },
    0XE00E: { key: 'pageUp' },
    0XE00F: { key: 'pageDown' },
    0XE010: { key: 'end' },
    0XE011: { key: 'home' },
    0xE012: { key: 'leftArrow' },
    0xE013: { key: 'upArrow' },
    0xE014: { key: 'rightArrow' },
    0xE015: { key: 'downArrow' },
    0XE016: { key: 'insert' },
    0XE017: { key: 'delete' },
    0XE018: { key: ';' },
    0XE019: { key: '=' },
    0XE031: { key: 'F1' },
    0XE032: { key: 'F2' },
    0XE033: { key: 'F3' },
    0XE034: { key: 'F4' },
    0XE035: { key: 'F5' },
    0XE036: { key: 'F6' },
    0XE037: { key: 'F7' },
    0XE038: { key: 'F8' },
    0XE039: { key: 'F9' },
    0XE03A: { key: 'F10' },
    0XE03B: { key: 'F11' },
    0XE03C: { key: 'F12' },
    0xE03D: { key: 'leftMeta', modifier: 'metaKey' }, // a.k.a. commandKey
    0xE050: { key: 'rightShift', modifier: 'shiftKey' },
    0xE051: { key: 'rightControl', modifier: 'ctrlKey' },
    0xE052: { key: 'rightAlt', modifier: 'altKey' },
    0xE052: { key: 'rightMeta', modifier: 'metaKey' }, // a.k.a. commandKey
};

function convertSeleniumKeyCode(key)
{
    const code = key.charCodeAt(0);
    return SeleniumCharCodeToEventSenderKey[code] || { key: key };
}

/**
 *
 * @param {Element} element
 * @param {string[]} keys
 * @returns Promise<undefined>
 */
window.test_driver_internal.send_keys = async function(element, keys)
{
    element.focus();

    const keyList = [];
    const modifiers = [];
    for (const key of keys) {
        let convertedKey = convertSeleniumKeyCode(key);
        keyList.push(convertedKey.key);
        if (convertedKey.modifier)
          modifiers.push(convertedKey.modifier);
    }

    if (testRunner.isIOSFamily && testRunner.isWebKit2) {
        await new Promise((resolve) => {
            testRunner.runUIScript(`
                const keyList = JSON.parse('${JSON.stringify(keyList)}');
                for (const key of keyList)
                    uiController.keyDown(key, modifiers);`, resolve);
        });
        return;
    }

    for (const key of keyList)
        eventSender.keyDown(key, modifiers);
}

/**
 *
 * @param {Element} element
 * @param {DOMRect} coords
 * @returns Promise<undefined>
 */
window.test_driver_internal.click = async function (element, coords)
{
    if (testRunner.isIOSFamily && testRunner.isWebKit2) {
        await new Promise((resolve) => {
            testRunner.runUIScript(`
                uiController.singleTapAtPoint(${coords.x}, ${coords.y}, function() {
                    uiController.uiScriptComplete();
                });`, resolve);
        });
        return;
    }

    eventSender.mouseMoveTo(coords.x, coords.y);
    eventSender.mouseDown();
    eventSender.mouseUp();
}

/**
 *
 * @param {object[]} sources
 * @returns Promise<undefined>
 */
window.test_driver_internal.action_sequence = async function(sources)
{
    // https://w3c.github.io/webdriver/#processing-actions
    let noneSource;
    let pointerSource;
    let keySource;
    let wheelSource;
    for (let source of sources) {
        switch (source.type) {
        case "none":
            noneSource = source;
            break;
        case "pointer":
            pointerSource = source;
            break;
        case "key":
            keySource = source;
            break;
        case "wheel":
            wheelSource = source;
            break;
        default:
            throw new Error(`Unknown source type "${source.type}".`);
        }
    }

    if (keySource && pointerSource)
        throw new Error("testdriver-vendor.js for WebKit does not yet support mixing key and pointer sources");

    if (keySource) {
        let modifiersInEffect = [];
        const events = [];
        for (const action of keySource.actions) {
            switch (action.type) {
            case 'keyDown': {
                const key = convertSeleniumKeyCode(action.value);
                if (key.modifier)
                    modifiersInEffect.push(key.modifier);
                else
                    events.push({type: 'rawKeyDown', arguments: [key.key, modifiersInEffect.slice(0)]});
                break;
            }
            case 'keyUp': {
                const key = convertSeleniumKeyCode(action.value);
                if (key.modifier)
                    modifiersInEffect = modifiersInEffect.filter((modifier) => modifier != key.modifier);
                else
                    events.push({type: 'rawKeyUp', arguments: [key.key, modifiersInEffect.slice(0)]});
                break;
            }
            case 'pause':
                // FIXME: Use eventSender.leapForward.
                throw new Error('testdriver-vendor.js for WebKit does not yet support pause key action');
            default:
                throw new Error(`Unknown key action type "${action.type}" encountered in testdriver-vendor.js`);
            }
        }

        if (noneSource) {
            let injectedActions = 0;
            noneSource.actions.forEach((action, index) => {
                if (action.duration > 0) {
                    events.splice(index + injectedActions + 1, 0, {type: 'leapForward', arguments: [action.duration]});
                    injectedActions++;
                }
            });
        }

        if (testRunner.isIOSFamily && testRunner.isWebKit2) {
            return await new Promise((resolve) => {
                testRunner.runUIScript(`
                    const events = JSON.parse('${JSON.stringify(events)}');
                    for (const event of events)
                        eventSender[event.type](...event.arguments);`, resolve);
            });
        }

        for (const event of events)
            eventSender[event.type](...event.arguments);

        return;
    }

    // If we have a "none" source, let's inject any pause with non-zero durations into the pointer source
    // after the matching action in the pointer source.
    if (noneSource) {
        let injectedActions = 0;
        noneSource.actions.forEach((action, index) => {
            if (action.duration > 0) {
                if (pointerSource)
                    pointerSource.actions.splice(index + injectedActions + 1, 0, action);
                if (wheelSource)
                    wheelSource.actions.splice(index + injectedActions + 1, 0, action);
                injectedActions++;
            }
        });
    }

    if (wheelSource)
        await dispatchWheelActions(wheelSource.actions);

    if (pointerSource) {
        const { pointerType } = pointerSource.parameters;
        if (!["mouse", "touch", "pen"].includes(pointerType))
            throw new Error(`Unknown pointer type "${pointerType}".`);

        logDebug(JSON.stringify(pointerSource));

        if (pointerType === "touch")
            await dispatchTouchActions(pointerSource.actions);
        else if (testRunner.isIOSFamily && "createTouch" in document)
            await dispatchTouchActions(pointerSource.actions, { insertPauseAfterPointerUp: true });
        else if (pointerType === "mouse" || pointerType === "pen")
            await dispatchMouseActions(pointerSource.actions, pointerType);
    }
};

/**
 *
 * @param {Object} permission_params
 * @param {PermissionDescriptor} permission_params.descriptor
 * @param {PermissionState} permission_params.state
 * @returns Promise<undefined>
 */
window.test_driver_internal.set_permission = async function(permission_params)
{
    switch (permission_params.descriptor.name) {
    case "camera":
        if (window.testRunner && testRunner.setCameraPermission)
            testRunner.setCameraPermission(permission_params.state === "granted");
        break;
    case "background-fetch":
        if (window.testRunner && testRunner.setBackgroundFetchPermission)
            testRunner.setBackgroundFetchPermission(permission_params.state === "granted");
        break;
    case "geolocation":
        const granted = permission_params.state === "granted";
        testRunner.setGeolocationPermission(granted);
        if (granted) {
            await pause(10);
            window.testRunner.setMockGeolocationPosition(51.478, -0.166, 100);
        }
        break;
    case "microphone":
        if (window.testRunner && testRunner.setMicrophonePermission)
            testRunner.setMicrophonePermission(permission_params.state === "granted");
        break;
    case "screen-wake-lock":
        testRunner.setScreenWakeLockPermission(permission_params.state == "granted");
        break;
    case "storage-access":
        await testRunner.setStorageAccessPermission(permission_params.state === "granted", location.href);
        break;
    default:
        throw new Error(`Unsupported permission name "${permission_params.descriptor.name}".`);
    }
};

/**
 *
 * @param {Window?} context
 * @returns Promise<undefined>
 */
window.test_driver_internal.delete_all_cookies = async function(context=null)
{
    context = context ?? window;

    return new Promise(r => context.testRunner.removeAllCookies(r));
}

/**
 *
 * @param {Window?} context
 * @returns Promise<sequence<WebDriverCookieData>>
 */
window.test_driver_internal.get_all_cookies = async function(context=null)
{
    context = context ?? window;

    return internals.webDriverGetCookies(context.document);
}

/**
 *
 * @param DOMString name
 * @param {Window?} context
 * @returns Promise<WebDriverCookieData?>
 */
window.test_driver_internal.get_named_cookie = async function(name, context=null)
{
    context = context ?? window;

    const cookies = internals.webDriverGetCookies(context.document);
    for (const cookie of cookies) {
        if (cookie.name == name)
            return cookie;
    }
    return null;
}

window.test_driver_internal.generate_test_report = async function(message, context=null)
{
    context = context ?? window;

    context.testRunner.generateTestReport(message);
}

/**
 *
 * @param {Window?} context
 * @returns {Promise<boolean>} - if activation was consumed for the given context
 */
window.test_driver_internal.consume_user_activation = async function(context)
{
    context = context ?? window;

    return context.internals.consumeTransientActivation();
}

/**
 *
 * @param {Window?} context
 * @returns {Promise<DOMRect>}
 * @returns {Promise<void>}
 */
window.test_driver_internal.minimize_window = async function (context=null)
{
    context = context ?? window;

    if (context.document?.fullscreenElement)
        await context.document.exitFullscreen();

    const rect = internals.visualViewportRect();

    if (context.document.visibilityState === "visible") {
        await new Promise(resolve => {
            context.addEventListener("visibilitychange", resolve, { once: true });
            testRunner.setPageVisibility("hidden");
        });
    }

    return rect;
}

/**
 *
 * @param {DOMRect} rect
 * @param {Window?} context
 * @returns {Promise<void>}
 */
window.test_driver_internal.set_window_rect = async function (rect, context=null)
{
    if (!rect || typeof rect !== "object" || typeof rect.width !== "number" || typeof rect.height !== "number")
        throw new Error("Invalid rect");

    context = context ?? window;

    if (context.document?.fullscreenElement)
        await context.document.exitFullscreen();

    context.testRunner.setViewSize(rect.width, rect.height);

    if (context.document.visibilityState === "visible")
        return;

    await new Promise(resolve => {
        context.addEventListener("visibilitychange", resolve, { once: true });
        testRunner.setPageVisibility("visible");
    });
}

function waitFor(condition)
{
    return new Promise((resolve, reject) => {
        let interval = setInterval(() => {
            if (condition()) {
                clearInterval(interval);
                resolve();
            }
        }, 0);
    });
}

/**
 *
 * @param {Element} element
 * @returns {Promise<string>}
 */
window.test_driver_internal.get_computed_label = async function (element, context=null) {
    if (!element)
        return "";
    context = context ?? window;

    return context.internals.getComputedLabel(element);
}

/**
 *
 * @param {Element} element
 * @returns {Promise<string>}
 */
window.test_driver_internal.get_computed_role = async function (element, context=null) {
    if (!element)
        return "";
    context = context ?? window;

    return context.internals.getComputedRole(element);
}

/**
 *
 * @param {string} origin
 * @param {string} embedding_origin
 * @param {boolean} blocked
 * @returns {Promise<void>}
 */
window.test_driver_internal.set_storage_access = async function (origin, embedding_origin, blocked, context=null) {
    // FIXME: This function should support setting storage access for specific origins.
    if (origin != "*" || embedding_origin != "*")
        throw new Error("Unsupported value for origin or embedding_origin");

    context = context ?? window;
    await context.testRunner.setStorageAccess(blocked);
}
