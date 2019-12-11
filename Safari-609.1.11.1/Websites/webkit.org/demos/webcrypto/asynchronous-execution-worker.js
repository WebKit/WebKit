var aesCbcParams = {
    name: "aes-cbc",
    iv: new Uint8Array([0x6a, 0x6e, 0x4f, 0x77, 0x39, 0x39, 0x6f, 0x4f, 0x5a, 0x46, 0x4c, 0x49, 0x45, 0x50, 0x4d, 0x72]),
}
var plainText = new Uint8Array(104857600); // 100MB
var times = 10;

onmessage = function(evt)
{
    var key = evt.data;

    var array = [ ];
    for (var i = 0; i < times; i++)
        array.push(crypto.subtle.encrypt(aesCbcParams, key, plainText));
    Promise.all(array).then(function() {
        postMessage(true);
    });
}
