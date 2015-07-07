description("Tests that navigator.webkitGetUserMedia is present.");

function hasGetUserMediaProperty()
{
    for (var property in navigator) {
        if (property == "webkitGetUserMedia")
            return true;
    }
    return false;
}

shouldBeTrue("typeof navigator.webkitGetUserMedia == 'function'");
shouldBeTrue("hasGetUserMediaProperty()");
shouldBeTrue("'webkitGetUserMedia' in navigator");
shouldBeFalse("navigator.hasOwnProperty('webkitGetUserMedia')");

window.jsTestIsAsync = false;
