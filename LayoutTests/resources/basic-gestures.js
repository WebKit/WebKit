function didShowKeyboard()
{
    return new Promise(resolve => {
        testRunner.runUIScript(`
            (function() {
                uiController.didShowKeyboardCallback = function() {
                    uiController.uiScriptComplete();
                }
            })();`, resolve);
    });
}

function doubleTapToZoomAtPoint(x, y)
{
    return new Promise(resolve => {
        testRunner.runUIScript(`
            (function() {
                let completionCount = 0;
                const checkDone = () => {
                    if (++completionCount == 3)
                        uiController.uiScriptComplete();
                };
                uiController.didEndZoomingCallback = checkDone;
                uiController.singleTapAtPoint(${x}, ${y}, checkDone);
                uiController.singleTapAtPoint(${x}, ${y}, checkDone);
            })();`, resolve);
    });
}

function doubleTapAtPoint(x, y)
{
    return new Promise(resolve => {
        testRunner.runUIScript(`
            (function() {
                uiController.doubleTapAtPoint(${x}, ${y}, function() {
                    uiController.uiScriptComplete();
                });
            })();`, resolve);
    });
}

function longPressAtPoint(x, y)
{
    return new Promise(resolve => {
        testRunner.runUIScript(`
            (function() {
                uiController.longPressAtPoint(${x}, ${y}, function() {
                    uiController.uiScriptComplete();
                });
            })();`, resolve);
    });
}

function liftUpAtPoint(x, y)
{
    return new Promise(resolve => {
        testRunner.runUIScript(`
            (function() {
                uiController.liftUpAtPoint(${x}, ${y}, 1, function() {
                    uiController.uiScriptComplete();
                });
            })();`, resolve);
    });
}

function longPressAndHoldAtPoint(x, y)
{
    return new Promise(resolve => {
        testRunner.runUIScript(`
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
            })();`, resolve);
    });
}

function tapAtPoint(x, y)
{
    return new Promise(resolve => {
        testRunner.runUIScript(`
            (function() {
                uiController.singleTapAtPoint(${x}, ${y}, function() {
                    uiController.uiScriptComplete();
                });
            })();`, resolve);
    });
}

function touchAndDragFromPointToPoint(startX, startY, endX, endY)
{
    return new Promise(resolve => {
        testRunner.runUIScript(`
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
            })();`, resolve);
    });
}

function holdAtPoint(x, y)
{
    return new Promise(resolve => {
        testRunner.runUIScript(`
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
            })();`, resolve);
    });
}

function continueTouchAndDragFromPointToPoint(startX, startY, endX, endY)
{
    return new Promise(resolve => {
        testRunner.runUIScript(`
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
             })();`, resolve);
    });
}
