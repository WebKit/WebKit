<?php
header("Content-Type: text/javascript; charset=UTF-8");
?>
importScripts("/js-test-resources/js-test.js");

// This is not a comprehensive list.
var bannedMIMETypes = [
    "audio/mp4",
    "audio/mpeg",
    "audio/ogg",
    "audio/wav",
    "audio/x-aiff",
    "image/gif",
    "image/png",
    "text/csv",
    "video/mpeg",
    "video/ogg",
    "video/quicktime",
];

self.scriptsSuccessfullyLoaded = 0;

description("Test that an external JavaScript script is blocked if has a banned MIME type.");

for (let mimeType of bannedMIMETypes) {
    try {
        importScripts(`../../security/contentTypeOptions/resources/script-with-header.pl?mime=${mimeType}&no-content-type-options=1`);
    } catch (e) { }
}

try {
    // For some reason this causes "SyntaxError: Invalid character '\ufffd'" when non-script MIME types are allowed to be executed.
    importScripts("../../security/resources/abe-that-increments-scriptsSuccessfullyLoaded.jpg");
} catch (e) { }

shouldBeZero("self.scriptsSuccessfullyLoaded");
finishJSTest();
