importScripts('../../../../resources/js-test-pre.js');
importScripts('../../../resources/common.js');

description("Test digest with SHA-256 in workers");

jsTestIsAsync = true;

var message = asciiToUint8Array("Hello, World!");
var expectedDigest = "dffd6021bb2bd5b0af676290809ec3a53191dd81c7f70a4b28688a362182986f";

crypto.subtle.digest("sha-256", message).then(function(result){
    digest = result;
    shouldBe("bytesToHexString(digest)", "expectedDigest");
    finishJSTest();
});
