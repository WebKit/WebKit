
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
        const target = document.body.appendChild(document.createElement("div"));
        target.setAttribute("style", `
            position: absolute;
            left: ${options.x};
            top: ${options.y};
            width: ${options.width};
            height: ${options.height};
        `);
        test.add_cleanup(() => target.remove());

        continutation(target, test);
    }, description);
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

    handleEvent(event)
    {
        if (!this.pointerIdToTouchIdMap[event.pointerId])
            this.pointerIdToTouchIdMap[event.pointerId] = Object.keys(this.pointerIdToTouchIdMap).length + 1;

        const id = this.pointerIdToTouchIdMap[event.pointerId];
        this.events.push({
            id,
            type: event.type,
            x: event.clientX,
            y: event.clientY,
            isPrimary: event.isPrimary
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

    beginTouches(options)
    {
        return this._run(`uiController.touchDownAtPoint(${options.x}, ${options.y}, ${options.numberOfTouches || 1})`);
    }

    swipe(from, to)
    {
        const durationInSeconds = 0.5;
        return this._run(`uiController.dragFromPointToPoint(${from.x}, ${from.y}, ${to.x}, ${to.y}, ${durationInSeconds})`);
    }

    pinchOut(options)
    {
        options.x = options.x || 0;
        options.y = options.y || 0;

        const startEvent = {
            inputType : "hand",
            timeOffset : 0,
            touches : [
                { inputType : "finger",
                  phase : "moved",
                  id : 1,
                  x : options.x,
                  y : options.y,
                  pressure : 0
                },
                { inputType : "finger",
                  phase : "moved",
                  id : 2,
                  x : (options.x + options.width) / options.scale,
                  y : (options.y + options.height) / options.scale,
                  pressure : 0
                }
            ]
        };

        const endEvent = {
            inputType : "hand",
            timeOffset : 0.5,
            touches : [
                { inputType : "finger",
                  phase : "moved",
                  id : 1,
                  x : options.x,
                  y : options.y,
                  pressure : 0
                },
                { inputType : "finger",
                  phase : "moved",
                  id : 2,
                  x : options.x + options.width,
                  y : options.y + options.height,
                  pressure : 0
                }
            ]
        };

        return this._runEvents([{
            interpolate : "linear",
            timestep: 0.1,
            coordinateSpace : "content",
            startEvent: startEvent,
            endEvent: endEvent
        }]);
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

    beginStylus(options)
    {
        options.azimuthAngle = options.azimuthAngle || 0;
        options.altitudeAngle = options.altitudeAngle || 0;
        options.pressure = options.pressure || 0;
        return this._run(`uiController.stylusDownAtPoint(${options.x}, ${options.y}, ${options.azimuthAngle}, ${options.altitudeAngle}, ${options.pressure})`);
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
        return this._action("stationary", options.x || 0, options.y || 0);
    }

    _action(phase, x, y)
    {
        this._lastX = x;
        this._lastY = y;
        return { inputType: "finger", id: this.id, phase, x, y };
    }

}
