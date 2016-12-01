importScripts('../../../../resources/js-test-pre.js');
importScripts('../../../resources/common.js');

description("Test digest with SHA-224 in workers");

jsTestIsAsync = true;

var message = asciiToUint8Array("Hello, World!");
var expectedDigest = "72a23dfa411ba6fde01dbfabf3b00a709c93ebf273dc29e2d8b261ff";

crypto.subtle.digest("sha-224", message).then(function(result){
    digest = result;
    shouldBe("bytesToHexString(digest)", "expectedDigest");
    finishJSTest();
});
