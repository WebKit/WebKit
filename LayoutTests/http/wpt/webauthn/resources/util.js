const testCredentialIdBase64url = "SMSXHngF7hEOsElA73C3RY-8bR4";
const testES256PrivateKeyBase64 =
    "BDj/zxSkzKgaBuS3cdWDF558of8AaIpgFpsjF/Qm1749VBJPgqUIwfhWHJ91nb7U" +
    "PH76c0+WFOzZKslPyyFse4goGIW2R7k9VHLPEZl5nfnBgEVFh5zev+/xpHQIvuq6" +
    "RQ==";
const testES256PublicKeyBase64url =
    "BDj_zxSkzKgaBuS3cdWDF558of8AaIpgFpsjF_Qm1749VBJPgqUIwfhWHJ91nb7U" +
    "PH76c0-WFOzZKslPyyFse4g";
const testRpId = "localhost";
const testUserhandleBase64 = "AAECAwQFBgcICQ==";

const RESOURCES_DIR = "/WebKit/webauthn/resources/";

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

function decodeAuthData(authDataUint8Array)
{
    let authDataObject = { };
    let pos = 0;
    let size = 0;

    // RP ID Hash
    size = 32;
    if (pos + size > authDataUint8Array.byteLength)
        return { };
    authDataObject.rpIdHash = authDataUint8Array.slice(pos, pos + size);
    pos = pos + size;

    // FLAGS
    size = 1;
    if (pos + size > authDataUint8Array.byteLength)
        return { };
    authDataObject.flags = authDataUint8Array.slice(pos, pos + size)[0];
    pos = pos + size;

    // Counter
    size = 4;
    if (pos + size > authDataUint8Array.byteLength)
        return { };
    authDataObject.counter = new Uint32Array(authDataUint8Array.slice(pos, pos + size))[0];
    pos = pos + size;

    if (pos == authDataUint8Array.byteLength)
        return authDataObject;

    // AAGUID
    size = 16;
    if (pos + size > authDataUint8Array.byteLength)
        return { };
    authDataObject.aaguid = authDataUint8Array.slice(pos, pos + size);
    pos = pos + size;

    // L
    size = 2;
    if (pos + size > authDataUint8Array.byteLength)
        return { };
    // Little Endian
    authDataObject.l = new Uint16Array(authDataUint8Array.slice(pos, pos + size))[0];
    pos = pos + size;

    // Credential ID
    size = authDataObject.l;
    if (pos + size > authDataUint8Array.byteLength)
        return { };
    authDataObject.credentialID = authDataUint8Array.slice(pos, pos + size);
    pos = pos + size;

    // FIXME(): Add CBOR decoder to parse the public key.

    // Assume no extensions.
    return authDataObject;
}

function concatenateBuffers(buffer1, buffer2)
{
    let tmp = new Uint8Array(buffer1.byteLength + buffer2.byteLength);
    tmp.set(new Uint8Array(buffer1), 0);
    tmp.set(new Uint8Array(buffer2), buffer1.byteLength);
    return tmp.buffer;
}

// Very dirty asn1 decoder. Just works.
function extractRawSignature(asn1signature)
{
    const signature = new Uint8Array(asn1signature);
    let tmp = new Uint8Array(64);
    const rStart =  signature[3] - 32;
    tmp.set(new Uint8Array(signature.slice(4 + rStart, 36 + rStart)), 0);
    const sStart =  signature[37 + rStart] - 32;
    tmp.set(new Uint8Array(signature.slice(38 + rStart + sStart)), 32);
    return tmp.buffer;
}

function waitForLoad()
{
    return new Promise((resolve) => {
        window.addEventListener('message', (message) => {
            resolve(message);
        });
    });
}

function withCrossOriginIframe(resourceFile)
{
    return new Promise((resolve) => {
        waitForLoad().then((message) => {
            resolve(message);
        });
        const frame = document.createElement("iframe");
        frame.src = get_host_info().HTTPS_REMOTE_ORIGIN + RESOURCES_DIR + resourceFile;
        document.body.appendChild(frame);
    });
}

function promiseRejects(test, expected, promise, description)
{
    return promise.then(test.unreached_func("Should have rejected: " + description)).catch(function(e) {
        assert_throws(expected, function() { throw e }, description);
        assert_equals(e.message, description);
    });
}