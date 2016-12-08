importScripts("../../../resources/common.js");

onmessage = function(evt)
{
    if (publicKey = evt.data.publicKey) {
        if (publicKey.type != 'public')
            postMessage({ result:false, message:'publicKey.type should be "public"' });
        else if (!publicKey.extractable)
            postMessage({ result:false, message:'publicKey.extractable should be true' });
        else if (publicKey.algorithm.name != "RSASSA-PKCS1-v1_5")
            postMessage({ result:false, message:'publicKey.algorithm.name should be "RSASSA-PKCS1-v1_5"' });
        else if (publicKey.algorithm.modulusLength != 2048)
            postMessage({ result:false, message:'publicKey.algorithm.modulusLength should be 2048' });
        else if (bytesToHexString(publicKey.algorithm.publicExponent) != "010001")
            postMessage({ result:false, message:'publicKey.algorithm.publicExponent should be "010001"' });
        else if (publicKey.algorithm.hash.name != "SHA-256")
            postMessage({ result:false, message:'publicKey.algorithm.hash.name should be "SHA-256"' });
        else if (publicKey.usages.toString() != "verify")
            postMessage({ result:false, message:'publicKey.usages should be ["verify"]' });
        else
            postMessage({ result:true, publicKey:publicKey });
    } else if (privateKey = evt.data.privateKey) {
        if (privateKey.type != 'private')
            postMessage({ result:false, message:'privateKey.type should be "private"' });
        else if (privateKey.extractable)
            postMessage({ result:false, message:'privateKey.extractable should be false' });
        else if (privateKey.algorithm.name != "RSASSA-PKCS1-v1_5")
            postMessage({ result:false, message:'privateKey.algorithm.name should be "RSASSA-PKCS1-v1_5"' });
        else if (privateKey.algorithm.modulusLength != 2048)
            postMessage({ result:false, message:'privateKey.algorithm.modulusLength should be 2048' });
        else if (bytesToHexString(privateKey.algorithm.publicExponent) != "010001")
            postMessage({ result:false, message:'privateKey.algorithm.publicExponent should be "010001"' });
        else if (privateKey.algorithm.hash.name != "SHA-256")
            postMessage({ result:false, message:'publicKey.algorithm.hash.name should be "SHA-256"' });
        else if (privateKey.usages.toString() != "sign")
            postMessage({ result:false, message:'privateKey.usages should be ["sign"]' });
        else
            postMessage({ result:true, privateKey:privateKey });
    } else {
        postMessage({ result:false, message:'key is ' + key });
    }
}
