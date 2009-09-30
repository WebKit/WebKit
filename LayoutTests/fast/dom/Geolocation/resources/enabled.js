description("Tests that the navigator.geolocation object is present.");

function hasGeolocationProperty()
{
    for (var property in navigator) {
        if (property == "geolocation")
            return true;
    }
    return false;
}

shouldBeTrue("typeof navigator.geolocation == 'object'");
shouldBeTrue("hasGeolocationProperty()");
shouldBeTrue("'geolocation' in navigator");
shouldBeTrue("navigator.hasOwnProperty('geolocation')");

var isAsynchronous = false;
var successfullyParsed = true;
