importScripts('../../../../resources/js-test-pre.js');
importScripts('../../../resources/common.js');

description("Test digest with SHA-384 in workers");

jsTestIsAsync = true;

var message = asciiToUint8Array("Hello, World!");
var expectedDigest = "5485cc9b3365b4305dfb4e8337e0a598a574f8242bf17289e0dd6c20a3cd44a089de16ab4ab308f63e44b1170eb5f515";

crypto.subtle.digest("sha-384", message).then(function(result){
    digest = result;
    shouldBe("bytesToHexString(digest)", "expectedDigest");
    finishJSTest();
});
