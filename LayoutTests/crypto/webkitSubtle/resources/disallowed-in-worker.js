importScripts('../../../resources/js-test-pre.js');

description("Tests that crypto.webkitSubtle will throw in workers.");

shouldThrowErrorName("crypto.webkitSubtle", "NotSupportedError");

finishJSTest();
