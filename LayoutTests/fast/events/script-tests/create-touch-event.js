description = "This tests whether the DOM can create TouchEvents.";

debug(description);

var event = null;

try
{
    event = document.createEvent("TouchEvent");
    shouldBeNonNull("event");
}
catch (e)
{
    testFailed("An exception was thrown: " + e.message);
}

successfullyParsed = true;
