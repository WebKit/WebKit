function didShowKeyboard()
{
	return `
	(function() {
        uiController.didShowKeyboardCallback = function() {
            uiController.uiScriptComplete();
		}
    })();`
}


function longPressAtPoint(x, y)
{
    return `
    (function() {
        uiController.longPressAtPoint(${x}, ${y}, function() {
            uiController.uiScriptComplete();
        });
    })();`
}

function liftUpAtPoint(x, y)
{
    return `
    (function() {
        uiController.liftUpAtPoint(${x}, ${y}, 1, function() {
            uiController.uiScriptComplete();
        });
    })();`
}

function longPressAndHoldAtPoint(x, y)
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
                        x : ${x},
                        y : ${y},
                        pressure : 0
                    }
                ]
            },
            endEvent : {
                inputType : "hand",
                timeOffset : 2.0,
                touches : [
                    {
                        inputType : "finger",
                        phase : "moved",
                        id : 1,
                        x : ${x},
                        y : ${y},
                        pressure : 0
                    }
                ]
            }
    }]};
    
    uiController.sendEventStream(JSON.stringify(eventStream), function() {});
        uiController.uiScriptComplete();
    })();`
}

function tapAtPoint(x, y)
{
    return `
    (function() {
        uiController.singleTapAtPoint(${x}, ${y}, function() {
            uiController.uiScriptComplete();
        });
    })();`
}

function touchAndDragFromPointToPoint(startX, startY, endX, endY)
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

function holdAtPoint(x, y)
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
                        phase : "moved",
                        id : 1,
                        x : ${x},
                        y : ${y},
                        pressure : 0
                    }
                ]
            },
            endEvent : {
                inputType : "hand",
                timeOffset : 5.0,
                touches : [
                    {
                        inputType : "finger",
                        phase : "moved",
                        id : 1,
                        x : ${x},
                        y : ${y},
                        pressure : 0
                    }
                ]
            }
    }]};
    
    uiController.sendEventStream(JSON.stringify(eventStream), function() {});
        uiController.uiScriptComplete();
    })();`
}

function continueTouchAndDragFromPointToPoint(startX, startY, endX, endY)
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
                           phase : "moved",
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
