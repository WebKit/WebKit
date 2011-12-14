description("Tests that no timers will trigger for navigator.geolocation object after onunload.");

if (window.layoutTestController) layoutTestController.setGeolocationPermission(true);

document.body.onload = function() {
    location = "data:text/html,You should have seen one unload alert appear.<script>window.setTimeout('if (window.layoutTestController) layoutTestController.notifyDone();', 100);</script>";
}

document.body.onunload = function() {
    navigator.geolocation.getCurrentPosition(
        function(p) {alert('FAIL: Unexpected Geolocation success callback.');},
        function(e) {alert('FAIL: Unexpected Geolocation error callback.' + e.code + e.message);},
        {timeout: 0, maximumAge:0}
    );
    alert("unload-called");
}

window.jsTestIsAsync = true;
window.successfullyParsed = true;
