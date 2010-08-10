description("Tests that the window.DeviceMotionEvent and window.ondevicemotion properties are present.");

function hasDeviceMotionEventProperty()
{
    for (var property in window) {
        if (property == "DeviceMotionEvent")
            return true;
    }
    return false;
}

shouldBeTrue("typeof window.DeviceMotionEvent == 'object'");
shouldBeFalse("typeof window.DeviceMotionEvent == 'function'");
shouldBeTrue("hasDeviceMotionEventProperty()");
shouldBeTrue("'DeviceMotionEvent' in window");
shouldBeTrue("window.hasOwnProperty('DeviceMotionEvent')");

function hasOnDeviceMotionProperty()
{
    for (var property in window) {
        if (property == "ondevicemotion")
            return true;
    }
    return false;
}

shouldBeTrue("typeof window.ondevicemotion == 'object'");
shouldBeTrue("hasOnDeviceMotionProperty()");
shouldBeTrue("'ondevicemotion' in window");
shouldBeTrue("window.hasOwnProperty('ondevicemotion')");

window.successfullyParsed = true;
