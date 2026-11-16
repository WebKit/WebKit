importScripts("/js-test-resources/js-test.js");

description('Tests that importing scripts with an "X-Content-Type-Options: nosniff" header are blocked if they have a non-script MIME type.');

var unscriptyMIMETypes = [
    "application/json",
    "image/png",
    "text/html",
    "text/vbs",
    "text/vbscript",
    "text/xx-javascript",
];

self.scriptsSuccessfullyLoaded = 0;

for (let mimeType of unscriptyMIMETypes) {
    try {
        importScripts("script-with-header.pl?mime=" + mimeType);
    } catch (e) { }
}

shouldBeZero("self.scriptsSuccessfullyLoaded");
finishJSTest();
