importScripts('../../../../resources/js-test-pre.js');
importScripts('../../../resources/common.js');

description("Test digest with SHA-1 in workers");

jsTestIsAsync = true;

var message = asciiToUint8Array("Hello, World!");
var expectedDigest = "0a0a9f2a6772942557ab5355d76af442f8f65e01";

crypto.subtle.digest("sha-1", message).then(function(result){
    digest = result;
    shouldBe("bytesToHexString(digest)", "expectedDigest");
    finishJSTest();
});
