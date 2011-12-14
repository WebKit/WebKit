description('Tests that the window.DeviceOrientationEvent and window.ondeviceorientation properties are present.');

function hasDeviceOrientationEventProperty()
{
    for (var property in window) {
        if (property == 'DeviceOrientationEvent')
            return true;
    }
    return false;
}

shouldBeTrue("typeof window.DeviceOrientationEvent == 'object'");
shouldBeFalse("typeof window.DeviceOrientationEvent == 'function'");
shouldBeTrue("hasDeviceOrientationEventProperty()");
shouldBeTrue("'DeviceOrientationEvent' in window");
shouldBeTrue("window.hasOwnProperty('DeviceOrientationEvent')");

function hasOnDeviceOrientationProperty()
{
    for (var property in window) {
        if (property == 'ondeviceorientation')
            return true;
    }
    return false;
}

shouldBeTrue("typeof window.ondeviceorientation == 'object'");
shouldBeTrue("hasOnDeviceOrientationProperty()");
shouldBeTrue("'ondeviceorientation' in window");
shouldBeTrue("window.hasOwnProperty('ondeviceorientation')");

window.successfullyParsed = true;
