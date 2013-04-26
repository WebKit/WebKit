description('Tests that the window.DeviceOrientationEvent and window.ondeviceorientation properties are present.');

shouldBeTrue("typeof window.DeviceOrientationEvent == 'object'");
shouldBeFalse("typeof window.DeviceOrientationEvent == 'function'");
shouldBeFalse("window.propertyIsEnumerable('DeviceOrientationEvent')");
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
