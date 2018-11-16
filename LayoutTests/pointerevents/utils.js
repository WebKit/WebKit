
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

const ui = new (class UIController {

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

        return this._runEvents({
            interpolate : "linear",
            timestep: 0.1,
            coordinateSpace : "content",
            startEvent: startEvent,
            endEvent: endEvent
        });
    }

    _runEvents(events)
    {
        return this._run(`uiController.sendEventStream('${JSON.stringify({ events: [events] })}')`);
    }

    _run(command)
    {
        return new Promise(resolve => testRunner.runUIScript(`(function() {
            (function() { ${command} })();
            uiController.uiScriptComplete();
        })();`, resolve));
    }

})();
