function asciiToUint8Array(str)
{
    var chars = [];
    for (var i = 0; i < str.length; ++i)
        chars.push(str.charCodeAt(i));
    return new Uint8Array(chars);
}

// Builds a hex string representation for an array-like input.
// "bytes" can be an Array of bytes, an ArrayBuffer, or any TypedArray.
// The output looks like this:
//    ab034c99
function bytesToHexString(bytes)
{
    if (!bytes)
        return null;

    bytes = new Uint8Array(bytes);
    var hexBytes = [];

    for (var i = 0; i < bytes.length; ++i) {
        var byteString = bytes[i].toString(16);
        if (byteString.length < 2)
            byteString = "0" + byteString;
        hexBytes.push(byteString);
    }

    return hexBytes.join("");
}

function bytesToASCIIString(bytes)
{
    return String.fromCharCode.apply(null, new Uint8Array(bytes));
}

var Base64URL = {
    stringify: function (a) {
        var base64string = btoa(String.fromCharCode.apply(0, a));
        return base64string.replace(/=/g, "").replace(/\+/g, "-").replace(/\//g, "_");
    },
    parse: function (s) {
        s = s.replace(/-/g, "+").replace(/_/g, "/").replace(/\s/g, '');
        return new Uint8Array(Array.prototype.map.call(atob(s), function (c) { return c.charCodeAt(0) }));
    }
};

function hexStringToUint8Array(hexString)
{
    if (hexString.length % 2 != 0)
        throw "Invalid hexString";
    var arrayBuffer = new Uint8Array(hexString.length / 2);

    for (var i = 0; i < hexString.length; i += 2) {
        var byteValue = parseInt(hexString.substr(i, 2), 16);
        if (byteValue == NaN)
            throw "Invalid hexString";
        arrayBuffer[i/2] = byteValue;
    }

    return arrayBuffer;
}
