description("Tests that navigator.webkitGetUserMedia is present.");

function hasGetUserMediaProperty()
{
    for (var property in navigator) {
        if (property == "mediaDevices") {
            var devices = navigator[property];
            for (property in devices) {
                if (property == "getUserMedia")
                    return true;
            }
        }
    }
    return false;
}

shouldBeTrue("typeof navigator.mediaDevices == 'object'");
shouldBeTrue("typeof navigator.mediaDevices.getUserMedia == 'function'");
shouldBeTrue("hasGetUserMediaProperty()");
shouldBeTrue("'mediaDevices' in navigator");
shouldBeTrue("'getUserMedia' in navigator.mediaDevices");
shouldBeFalse("navigator.hasOwnProperty('mediaDevices')");

window.jsTestIsAsync = false;
