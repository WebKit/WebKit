description('Tests that document.createEvent() works with orientationChange')

function handleOrientationChange()
{
    document.getElementById('result').innerHTML = "PASS";
}

window.addEventListener('onorientationchange', handleOrientationChange, false);

try {
    var event = document.createEvent("OrientationEvent");
    event.initEvent("orientationchange", false, false);
    window.dispatchEvent(event);
} catch(e) {
    document.getElementById('result').innerHTML = "FAIL... orientationChange event doesn't appear to be enabled or implemented.";
}

window.successfullyParsed = true;
