
function target_test(...args)
{
    if (args.length !== 2 && args.length !== 3) {
        console.error(`target_test expected 2 or 3 arguments but got ${args.length}.`);
        return;
    }

    const impliedOptions = args.length == 2;

    let options = impliedOptions ? { } : args[0];
    let continutation = args[impliedOptions ? 0 : 1];
    let description = args[impliedOptions ? 1 : 2];

    options.x = options.x || 0;
    options.y = options.y || 0;
    options.width = options.width || "100%";
    options.height = options.height || "100%";

    async_test(test => {
        continutation(makeTarget(test, options), test);
    }, description);
}

function makeTarget(test, options)
{
    const target = document.body.appendChild(document.createElement("div"));
    target.setAttribute("style", `
        position: absolute;
        left: ${options.x};
        top: ${options.y};
        width: ${options.width};
        height: ${options.height};
    `);
    test.add_cleanup(() => target.remove());
    return target;
}

class EventTracker
{
    constructor(target, eventNames)
    {
        this.target = target;
        this.events = [];
        this.pointerIdToTouchIdMap = {};

        for (let eventName of eventNames)
            target.addEventListener(eventName, this);
    }

    clear()
    {
        this.events = [];
    }

    handleEvent(event)
    {
        if (event instanceof PointerEvent)
            this._handlePointerEvent(event);
        else if (event instanceof MouseEvent)
            this._handleMouseEvent(event);
    }

    _handlePointerEvent(event)
    {
        if (!this.pointerIdToTouchIdMap[event.pointerId])
            this.pointerIdToTouchIdMap[event.pointerId] = Object.keys(this.pointerIdToTouchIdMap).length + 1;

        this.events.push({
            id: this.pointerIdToTouchIdMap[event.pointerId],
            type: event.type,
            x: event.clientX,
            y: event.clientY,
            pressure: event.pressure,
            isPrimary: event.isPrimary,
            isTrusted: event.isTrusted,
            button: event.button,
            buttons: event.buttons
        });
    }

    _handleMouseEvent(event)
    {
        this.events.push({
            type: event.type,
            x: event.clientX,
            y: event.clientY,
        });
    }

    assertMatchesEvents(expectedEvents)
    {
        assert_true(!!this.events.length, "Event tracker saw some events.");
        assert_equals(expectedEvents.length, this.events.length, "Expected events and actual events have the same length.");
        for (let i = 0; i < expectedEvents.length; ++i) {
            const expectedEvent = expectedEvents[i];
            const actualEvent = this.events[i];
            for (let property of Object.getOwnPropertyNames(expectedEvent))
                assert_equals(expectedEvent[property], actualEvent[property], `Property ${property} matches for event at index ${i}.`);
        }
    }
}

const ui = new (class UIController {

    constructor()
    {
        this.fingers = {};
    }

    finger()
    {
        const id = Object.keys(this.fingers).length + 1;
        return this.fingers[id] = new Finger(id);
    }

    swipe(from, to)
    {
        const durationInSeconds = 0.1;
        return new Promise(resolve => this._run(`uiController.dragFromPointToPoint(${from.x}, ${from.y}, ${to.x}, ${to.y}, ${durationInSeconds})`).then(() =>
            setTimeout(resolve, durationInSeconds * 1000)
        ));
    }

    tap(options)
    {
        return this._run(`uiController.singleTapAtPoint(${options.x}, ${options.y})`);
    }

    doubleTapToZoom(options)
    {
        const durationInSeconds = 0.35;
        return new Promise(resolve => this._run(`uiController.doubleTapAtPoint(${options.x}, ${options.y}, 0)`).then(() =>
            setTimeout(resolve, durationInSeconds * 1000)
        ));
        return this._run();
    }

    pinchOut(options)
    {
        options.x = options.x || 0;
        options.y = options.y || 0;

        const startPoint = { x: options.x + options.width, y: options.y + options.height };
        const endPoint = { x: options.x + options.width * options.scale, y: options.y + options.height * options.scale };

        function step(factor)
        {
            return {
                x: endPoint.x + (startPoint.x - endPoint.x) * (1 - factor),
                y: endPoint.y + (startPoint.y - endPoint.y) * (1 - factor)
            };
        }

        const one = this.finger();
        const two = this.finger();
        return this.sequence([
            one.begin({ x: options.x, y: options.y }),
            two.begin(step(0)),
            two.move(step(0.2)),
            two.move(step(0.4)),
            two.move(step(0.6)),
            two.move(step(0.8)),
            two.move(step(1)),
            one.end(),
            two.end()
        ]);
    }

    sequence(touches)
    {
        const activeFingers = {};

        return this._runEvents(touches.map((touches, index) => {
            if (!Array.isArray(touches))
                touches = [touches];

            const processedIDs = {};

            // Update the list of active touches.
            touches.forEach(touch => {
                processedIDs[touch.id] = true;
                if (touch.phase === "ended")
                    delete activeFingers[touch.id];
                else
                    activeFingers[touch.id] = { x: touch.x, y: touch.y };
            });

            // Now go through the active touches and check that they're all listed in the new touches.
            for (let id in activeFingers) {
                if (!processedIDs[id])
                    touches.push(this.fingers[id].stationary(activeFingers[id]));
            }

            return {
                inputType : "hand",
                timeOffset : index * 0.05,
                coordinateSpace : "content",
                touches : touches
            }
        }));
    }

    tapStylus(options)
    {
        options.azimuthAngle = options.azimuthAngle || 0;
        options.altitudeAngle = options.altitudeAngle || 0;
        options.pressure = options.pressure || 0;
        return this._run(`uiController.stylusTapAtPoint(${options.x}, ${options.y}, ${options.azimuthAngle}, ${options.altitudeAngle}, ${options.pressure})`);
    }

    _runEvents(events)
    {
        return this._run(`uiController.sendEventStream('${JSON.stringify({ events })}')`);
    }

    _run(command)
    {
        return new Promise(resolve => testRunner.runUIScript(`(function() {
            (function() { ${command} })();
            uiController.uiScriptComplete();
        })();`, resolve));
    }

})();

class Finger
{

    constructor(id)
    {
        this.id = id;
    }

    begin(options)
    {
        return this._action("began", options.x || 0, options.y || 0);
    }

    move(options)
    {
        return this._action("moved", options.x || 0, options.y || 0);
    }

    end(options)
    {
        return this._action("ended", this._lastX, this._lastY);
    }

    stationary(options)
    {
        return this._action("stationary", options.x || this._lastX, options.y || this._lastY, options.pressure || 0);
    }

    _action(phase, x, y, pressure = 0)
    {
        this._lastX = x;
        this._lastY = y;
        return { inputType: "finger", id: this.id, phase, x, y, pressure };
    }

}
