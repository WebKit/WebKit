onmessage = function(evt)
{
    var key = evt.data;
    if (key.type != 'secret')
        postMessage({ result:false, message:'key.type should be "secret"' });
    else if (!key.extractable)
        postMessage({ result:false, message:'key.extractable should be true' });
    else if (key.algorithm.name != "hmac")
        postMessage({ result:false, message:'key.algorithm.name should be "hmac"' });
    else if (key.usages.toString() != "encrypt,decrypt,sign,verify")
        postMessage({ result:false, message:'key.usages should be ["encrypt", "decrypt", "sign", "verify"]' });
    else
        postMessage({ result:true, key:key });
}
