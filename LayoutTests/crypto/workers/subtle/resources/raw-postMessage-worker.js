onmessage = function(evt)
{
    var key = evt.data;
    if (!key)
        postMessage({ result:false, message:'key is ' + key });
    if (key.type != 'secret')
        postMessage({ result:false, message:'key.type should be "secret"' });
    else if (key.extractable)
        postMessage({ result:false, message:'key.extractable should be false' });
    else if (key.algorithm.name != "PBKDF2")
        postMessage({ result:false, message:'key.algorithm.name should be "PBKDF2"' });
    else if (key.usages.toString() != "deriveBits,deriveKey")
        postMessage({ result:false, message:'key.usages should be ["deriveBits", "deriveKey"]' });
    else
        postMessage({ result:true, key:key });
}
