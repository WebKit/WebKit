description("Tests that the window.ondeviceorientation property is present.");

function hasOnDeviceOrientationProperty()
{
    for (var property in window) {
        if (property == "ondeviceorientation")
            return true;
    }
    return false;
}

shouldBeTrue("typeof window.ondeviceorientation == 'object'");
shouldBeTrue("hasOnDeviceOrientationProperty()");
shouldBeTrue("'ondeviceorientation' in window");
shouldBeTrue("window.hasOwnProperty('ondeviceorientation')");

window.successfullyParsed = true;
