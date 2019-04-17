
const isDebug = false;

function logDebug(message)
{
    if (isDebug)
        console.log(message);
}

function dispatchMouseActions(actions)
{
    if (!window.eventSender)
        return Promise.reject(new Error("window.eventSender is undefined."));

    return new Promise(function(resolve, reject) {
        setTimeout(() => {
            for (let action of actions) {
                switch (action.type) {
                case "pointerMove":
                    let origin = { x: 0, y: 0 };
                    if (action.origin instanceof Element) {
                        const bounds = action.origin.getBoundingClientRect();
                        logDebug(`${action.origin.id} [${bounds.left}, ${bounds.top}, ${bounds.width}, ${bounds.height}]`);
                        origin.x = bounds.left + 1;
                        origin.y = bounds.top + 1;
                    }
                    logDebug(`eventSender.mouseMoveTo(${action.x + origin.x}, ${action.y + origin.y})`);
                    eventSender.mouseMoveTo(action.x + origin.x, action.y + origin.y);
                    break;
                case "pointerDown":
                    // FIXME: what to do with "button"?
                    logDebug(`eventSender.mouseDown()`);
                    eventSender.mouseDown();
                    break;
                case "pointerUp":
                    // FIXME: what to do with "button"?
                    logDebug(`eventSender.mouseUp()`);
                    eventSender.mouseUp();
                    break;
                default:
                    return Promise.reject(new Error(`Unknown action type "${action.type}".`));
                }
            }
            resolve();
        });
    });
}

if (window.test_driver_internal === undefined)
    window.test_driver_internal = { };

window.test_driver_internal.action_sequence = function(actions)
{
    const action = actions[0];

    if (action.type !== "pointer")
        return Promise.reject(new Error(`Unknown action type "${action.type}".`));

    if (action.parameters.pointerType !== "mouse")
        return Promise.reject(new Error(`Unknown pointer type pointer type "${action.parameters.pointerType}".`));

    // FIXME: Handle iOS.
    // FIXME: what do with "id" property?
    return dispatchMouseActions(action.actions);
};
