onmessage = function(evt)
{
    var key = evt.data;
    if (!key)
        postMessage({ result:false, message:'key is ' + key });
    if (key.type != 'secret')
        postMessage({ result:false, message:'key.type should be "secret"' });
    else if (!key.extractable)
        postMessage({ result:false, message:'key.extractable should be true' });
    else if (key.algorithm.name != "AES-CBC")
        postMessage({ result:false, message:'key.algorithm.name should be "AES-CBC"' });
    else if (key.algorithm.length != 128)
        postMessage({ result:false, message:'key.algorithm.length should be 128' });
    else if (key.usages.toString() != "decrypt,encrypt")
        postMessage({ result:false, message:'key.usages should be ["decrypt", "encrypt"]' });
    else
        postMessage({ result:true, key:key });
}
