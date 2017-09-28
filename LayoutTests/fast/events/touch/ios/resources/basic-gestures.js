function pressAtPoint(x, y)
{
    return `
    (function() {
        uiController.longPressAtPoint(${x}, ${y}, function() {
            uiController.uiScriptComplete();
        });
    })();`
}

function dragFromPointToPoint(startX, startY, endX, endY)
{
    return `
    (function() {
    var eventStream = {
    events : [
        {
            interpolate : "linear",
            timestep: 0.1,
            coordinateSpace : "content",
            startEvent : {
                inputType : "hand",
                timeOffset : 0,
                touches : [
                    {
                        inputType : "finger",
                        phase : "began",
                        id : 1,
                        x : ${startX},
                        y : ${startY},
                        pressure : 0
                    }
                ]
            },
            endEvent : {
                inputType : "hand",
                timeOffset : 0.5,
                touches : [
                    {
                        inputType : "finger",
                        phase : "moved",
                        id : 1,
                        x : ${endX},
                        y : ${endY},
                        pressure : 0
                    }
                ]
            }
    }]};
    
    uiController.sendEventStream(JSON.stringify(eventStream), function() {});
        uiController.uiScriptComplete();
    })();`
}
