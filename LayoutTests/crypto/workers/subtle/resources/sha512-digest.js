importScripts('../../../../resources/js-test-pre.js');
importScripts('../../../resources/common.js');

description("Test digest with SHA-512 in workers");

jsTestIsAsync = true;

var message = asciiToUint8Array("Hello, World!");
var expectedDigest = "374d794a95cdcfd8b35993185fef9ba368f160d8daf432d08ba9f1ed1e5abe6cc69291e0fa2fe0006a52570ef18c19def4e617c33ce52ef0a6e5fbe318cb0387";

crypto.subtle.digest("sha-512", message).then(function(result){
    digest = result;
    shouldBe("bytesToHexString(digest)", "expectedDigest");
    finishJSTest();
});
