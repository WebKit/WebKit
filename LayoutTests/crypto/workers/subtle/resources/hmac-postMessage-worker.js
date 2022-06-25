onmessage = function(evt)
{
    var key = evt.data;
    if (!key)
        postMessage({ result:false, message:'key is ' + key });
    if (key.type != 'secret')
        postMessage({ result:false, message:'key.type should be "secret"' });
    else if (!key.extractable)
        postMessage({ result:false, message:'key.extractable should be true' });
    else if (key.algorithm.name != "HMAC")
        postMessage({ result:false, message:'key.algorithm.name should be "HMAC"' });
    else if (key.algorithm.length != 128)
        postMessage({ result:false, message:'key.algorithm.length should be 128' });
    else if (key.algorithm.hash.name != "SHA-1")
        postMessage({ result:false, message:'key.algorithm.hash.name should be "SHA-1"' });
    else if (key.usages.toString() != "sign,verify")
        postMessage({ result:false, message:'key.usages should be ["sign", "verify"]' });
    else
        postMessage({ result:true, key:key });
}
