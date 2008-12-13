description("This tests that the Event classes have constructors.");

var event = document.createEvent("Event");
shouldBeTrue("event instanceof window.Event");
shouldBeTrue("event.constructor === window.Event");

var uiEvent = document.createEvent("UIEvent");
shouldBeTrue("uiEvent instanceof window.UIEvent");
shouldBeTrue("uiEvent instanceof window.Event");
shouldBeTrue("uiEvent.constructor === window.UIEvent");

var keyboardEvent = document.createEvent("KeyboardEvent");
shouldBeTrue("keyboardEvent instanceof window.KeyboardEvent");
shouldBeTrue("keyboardEvent instanceof window.UIEvent");
shouldBeTrue("keyboardEvent instanceof window.Event");
shouldBeTrue("keyboardEvent.constructor === window.KeyboardEvent");

var mouseEvent = document.createEvent("MouseEvent");
shouldBeTrue("mouseEvent instanceof window.MouseEvent");
shouldBeTrue("mouseEvent instanceof window.UIEvent");
shouldBeTrue("mouseEvent instanceof window.Event");
shouldBeTrue("mouseEvent.constructor === window.MouseEvent");

var wheelEvent = document.createEvent("WheelEvent");
shouldBeTrue("wheelEvent instanceof window.WheelEvent");
shouldBeTrue("wheelEvent instanceof window.UIEvent");
shouldBeTrue("wheelEvent instanceof window.Event");
shouldBeTrue("wheelEvent.constructor === window.WheelEvent");

var mutationEvent = document.createEvent("MutationEvent");
shouldBeTrue("mutationEvent instanceof window.MutationEvent");
shouldBeTrue("mutationEvent instanceof window.Event");
shouldBeTrue("mutationEvent.constructor === window.MutationEvent");

var overflowEvent = document.createEvent("OverflowEvent");
shouldBeTrue("overflowEvent instanceof window.OverflowEvent");
shouldBeTrue("overflowEvent instanceof window.Event");
shouldBeTrue("overflowEvent.constructor === window.OverflowEvent");

var progressEvent = document.createEvent("ProgressEvent");
shouldBeTrue("progressEvent instanceof window.ProgressEvent");
shouldBeTrue("progressEvent instanceof window.Event");
shouldBeTrue("progressEvent.constructor === window.ProgressEvent");

var textEvent = document.createEvent("TextEvent");
shouldBeTrue("textEvent instanceof window.TextEvent");
shouldBeTrue("textEvent instanceof window.Event");
shouldBeTrue("textEvent.constructor === window.TextEvent");

var messageEvent = document.createEvent("MessageEvent");
shouldBeTrue("messageEvent instanceof window.MessageEvent");
shouldBeTrue("messageEvent instanceof window.Event");
shouldBeTrue("messageEvent.constructor === window.MessageEvent");

var animationEvent = document.createEvent("WebKitAnimationEvent");
shouldBeTrue("animationEvent instanceof window.WebKitAnimationEvent");
shouldBeTrue("animationEvent instanceof window.Event");
shouldBeTrue("animationEvent.constructor === window.WebKitAnimationEvent");

var transitionEvent = document.createEvent("WebKitTransitionEvent");
shouldBeTrue("transitionEvent instanceof window.WebKitTransitionEvent");
shouldBeTrue("transitionEvent instanceof window.Event");
shouldBeTrue("transitionEvent.constructor === window.WebKitTransitionEvent");

var successfullyParsed = true;
