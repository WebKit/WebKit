importScripts('../../../resources/js-test-pre.js');

description("Tests that crypto.webkitSubtle will throw in workers.");

shouldThrow("crypto.webkitSubtle", "'NotSupportedError (DOM Exception 9): The operation is not supported.'");

finishJSTest();
