function resetCookies()
{
    if (window.layoutTestController)
        layoutTestController.setAlwaysAcceptCookies(true);

    // Due to cross-origin restrictions, we can only (simply) reset cookies for our current origin.
    var url = "http://" + window.location.hostname +":8000/cookies/resources/cookie-utility.php?queryfunction=deleteCookies";
    var req = new XMLHttpRequest();
    try {
        req.open('GET', url, false);
        req.send();
    } catch (e) {
        alert("Attempt to clear " + url + " cookies might have failed.  Test results might be off from here on out. (" + e + ")");
    }
    
    if (window.layoutTestController)
        layoutTestController.setAlwaysAcceptCookies(false);
}
