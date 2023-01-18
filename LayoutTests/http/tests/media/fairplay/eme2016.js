async function fetchBuffer(url) {
    try {
        let result = await fetch(url);
        let buffer = await result.arrayBuffer();
        consoleWrite(`FETCH: ${url} OK`);
        return buffer;
    } catch(e) {
        consoleWrite(`FETCH: ${url} ERROR`);
        failTest();
    }
}

async function getResponse(event, options) {
    var licenseResponse = await fetch('resources/index.py', {
        method: 'POST',
        headers: new Headers({'Content-type': 'application/x-www-form-urlencoded'}),
        body: JSON.stringify({
            "fairplay-streaming-request" : {
                "version" : 1,
                "streaming-keys" : [{
                    "id" : 1,
                    "uri" : 'skd://twelve',
                    "spc" : base64EncodeUint8Array(new Uint8Array(event.message)),
                }]
            }
        }),
    });

    if (!options || !options.silent)
        consoleWrite('PROMISE: licenseResponse resolved');

    try {
        let responseObject = await licenseResponse.json();
        var keyResponse = responseObject["fairplay-streaming-response"]["streaming-keys"][0];
    } catch(e) {
        throw `Server returned malformed response: ${e}`;
    }
    return base64DecodeUint8Array(keyResponse.ckc);
}

async function startEME(options) {
    let video = options.video;
    let capabilities = options.capabilities;

    let access = await navigator.requestMediaKeySystemAccess("com.apple.fps", capabilities);
    consoleWrite('PROMISE: requestMediaKeySystemAccess resolved');

    let keys = await access.createMediaKeys();
    consoleWrite('PROMISE: createMediaKeys resolved');
    let certificate = await fetchBuffer('resources/cert.der');
    await keys.setServerCertificate(certificate);
    consoleWrite('PROMISE: keys.setServerCertificate resolved');
    await video.setMediaKeys(keys);
    consoleWrite('PROMISE: setMediaKeys() resolved');

    return keys;
}

async function fetchAndAppend(sourceBuffer, url, silent) {
    let buffer = await fetchBuffer(url);
    sourceBuffer.appendBuffer(buffer);
    await waitFor(sourceBuffer, 'updateend', silent);
}

async function runAndWaitForLicenseRequest(session, callback) {
    var licenseRequestPromise = waitFor(session, 'message');
    await callback();
    let message = await licenseRequestPromise;

    let response = await getResponse(message);
    await session.update(response);
    consoleWrite('PROMISE: session.update() resolved');
}

async function fetchAndWaitForLicenseRequest(session, sourceBuffer, url) {
    await runAndWaitForLicenseRequest(session, async () => {
        await fetchAndAppend(sourceBuffer, 'content/elementary-stream-video-header-keyid-4.m4v');
    });
}

async function fetchAppendAndWaitForEncrypted(video, sourceBuffer, url) {
    let updateEndPromise = fetchAndAppend(sourceBuffer, url, true);
    let encryptedEvent = await waitFor(video, 'encrypted');

    let session = video.mediaKeys.createSession();
    session.generateRequest(encryptedEvent.initDataType, encryptedEvent.initData);
    let message = await waitFor(session, 'message');
    let response = await getResponse(message);
    await session.update(response);
    consoleWrite('PROMISE: session.update() resolved');
    await updateEndPromise;
    consoleWrite(`EVENT(updateend)`);
    return session;
}

async function createBufferAndAppend(mediaSource, type, url) {
    let sourceBuffer = mediaSource.addSourceBuffer(type);
    consoleWrite('Created sourceBuffer');
    await fetchAndAppend(sourceBuffer, url);
    return sourceBuffer;
}

async function createBufferAppendAndWaitForEncrypted(video, mediaSource, type, url) {
    let sourceBuffer = mediaSource.addSourceBuffer(type);
    consoleWrite('Created sourceBuffer');

    let session = await fetchAppendAndWaitForEncrypted(video, sourceBuffer, url);

    return {sourceBuffer, session};
}

