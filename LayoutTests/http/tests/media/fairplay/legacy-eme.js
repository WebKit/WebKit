
function selectKeySystem(version, mimeType)
{
    var keySystem;
    if (WebKitMediaKeys.isTypeSupported("com.apple.fps." + version + "_0", mimeType))
        keySystem = "com.apple.fps." + version + "_0";
    return keySystem;
}

async function startLegacyEME(options) {
    let video = options.video;

    var keySystem = selectKeySystem(options.protocolVersion, options.mimeType);
    if (!keySystem) {
        consoleWrite('FAIL: key system or GPU not supported');
        throw 'keySystem error';
    }

    let keys = new WebKitMediaKeys(keySystem);
    if (!keys) {
        consoleWrite('FAIL: Could not create MediaKeys');
        throw 'MediaKeys error';
    }

    video.webkitSetMediaKeys(keys);

    return keys;
}

function extractContentId(initData) {
    contentId = uInt16ArrayToString(initData);
    // contentId is passed up as a URI, from which the host must be extracted:
    var link = document.createElement('a');
    link.href = contentId;
    return link.hostname;
}

function concatInitDataIdAndCertificate(initData, id, cert) {
    if (typeof id == "string")
        id = stringToUint16Array(id);
    // layout is [initData][4 byte: idLength][idLength byte: id][4 byte:certLength][certLength byte: cert]
    var offset = 0;
    var buffer = new ArrayBuffer(initData.byteLength + 4 + id.byteLength + 4 + cert.byteLength);
    var dataView = new DataView(buffer);

    var initDataArray = new Uint8Array(buffer, offset, initData.byteLength);
    initDataArray.set(initData);
    offset += initData.byteLength;

    dataView.setUint32(offset, id.byteLength, true);
    offset += 4;

    var idArray = new Uint16Array(buffer, offset, id.length);
    idArray.set(id);
    offset += idArray.byteLength;

    dataView.setUint32(offset, cert.byteLength, true);
    offset += 4;

    var certArray = new Uint8Array(buffer, offset, cert.byteLength);
    certArray.set(cert);

    return new Uint8Array(buffer, 0, buffer.byteLength);
}
