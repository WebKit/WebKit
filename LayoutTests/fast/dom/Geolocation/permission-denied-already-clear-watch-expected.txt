Tests that when Geolocation permission has been denied prior to a call to watchPosition, and the watch is cleared in the error callback, there is no crash. This a regression test for https://bugs.webkit.org/show_bug.cgi?id=32111.

On success, you will see a series of "PASS" messages, followed by "TEST COMPLETE".


PASS error.code is error.PERMISSION_DENIED
PASS error.message is "User denied Geolocation"

PASS error.code is error.PERMISSION_DENIED
PASS error.message is "User denied Geolocation"
PASS successfullyParsed is true

TEST COMPLETE

