importScripts("../../resources/js-test.js");

if (typeof(self.isSecureContext) === 'undefined')
    testPassed("self.isSecureContext is undefined.");
else
    testFailed("self.isSecureContext is defined.");
finishJSTest();
