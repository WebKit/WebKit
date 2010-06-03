description = "This tests whether the DOM can create TransformActionEvents.";

debug(description);

var event = null;

try
{
    event = document.createEvent("TransformActionEvent");
    shouldBeNonNull("event");
}
catch (e)
{
    testFailed("An exception was thrown: " + e.message);
}

successfullyParsed = true;
