description("Exercises a condition where the DOMWindow may leak if Geolocation requests are ongoing when the page is navigated away or closed. The page should reoload.");

if (window.layoutTestController) layoutTestController.setGeolocationPermission(true);
if (window.layoutTestController) layoutTestController.setMockGeolocationPosition(51.478, -0.166, 100.0);

navigator.geolocation.watchPosition(function() {}, null);

document.body.onload = function() {
  location = "data:text/html,Exercises a condition where the DOMWindow may leak if Geolocation requests are ongoing when the page is navigated away or closed. Complete.<script>if (window.layoutTestController) layoutTestController.notifyDone();</" + "script>";
}

window.jsTestIsAsync = true;
window.successfullyParsed = true;
