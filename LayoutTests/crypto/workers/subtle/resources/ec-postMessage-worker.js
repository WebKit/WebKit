importScripts("../../../resources/common.js");

onmessage = function(evt)
{
    if (publicKey = evt.data.publicKey) {
        if (publicKey.type != 'public')
            postMessage({ result:false, message:'publicKey.type should be "public"' });
        else if (!publicKey.extractable)
            postMessage({ result:false, message:'publicKey.extractable should be true' });
        else if (publicKey.algorithm.name != "ECDH")
            postMessage({ result:false, message:'publicKey.algorithm.name should be "ECDH"' });
        else if (publicKey.algorithm.namedCurve != "P-256")
            postMessage({ result:false, message:'publicKey.algorithm.namedCurve should be P-256' });
        else if (publicKey.usages.toString() != "")
            postMessage({ result:false, message:'publicKey.usages should be [ ]' });
        else
            postMessage({ result:true, publicKey:publicKey });
    } else if (privateKey = evt.data.privateKey) {
        if (privateKey.type != 'private')
            postMessage({ result:false, message:'privateKey.type should be "private"' });
        else if (!privateKey.extractable)
            postMessage({ result:false, message:'privateKey.extractable should be true' });
        else if (privateKey.algorithm.name != "ECDH")
            postMessage({ result:false, message:'publicKey.algorithm.name should be "ECDH"' });
        else if (privateKey.algorithm.namedCurve != "P-256")
            postMessage({ result:false, message:'publicKey.algorithm.namedCurve should be P-256' });
        else if (privateKey.usages.toString() != "deriveBits")
            postMessage({ result:false, message:'privateKey.usages should be ["deriveBits"]' });
        else
            postMessage({ result:true, privateKey:privateKey });
    } else {
        postMessage({ result:false, message:'key is ' + key });
    }
}
