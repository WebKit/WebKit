async function fetchBuffer(url, options) {
    try {
        let result = await fetch(url);
        let buffer = await result.arrayBuffer();
        if (!options || !options.silent)
            consoleWrite(`FETCH: ${url} OK`);
        return buffer;
    } catch(e) {
        if (!options || !options.silent)
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
    if (!options.silent)
        consoleWrite('PROMISE: requestMediaKeySystemAccess resolved');

    let keys = await access.createMediaKeys();
    if (!options.silent)
        consoleWrite('PROMISE: createMediaKeys resolved');
    let certificate = await fetchBuffer('resources/cert.der', options);
    await keys.setServerCertificate(certificate);
    if (!options.silent)
        consoleWrite('PROMISE: keys.setServerCertificate resolved');

    if (options.setMediaKeys) {
        await video.setMediaKeys(keys);
        if (!options.silent)
            consoleWrite('PROMISE: setMediaKeys() resolved');
    }

    return keys;
}

async function fetchAndAppend(sourceBuffer, url, options) {
    const buffer = await fetchBuffer(url, options);
    sourceBuffer.appendBuffer(buffer);
    await waitFor(sourceBuffer, 'updateend', options && options.silent);
}

async function fetchAndSilentAppend(sourceBuffer, url, options) {
    const buffer = await fetchBuffer(url, options);
    sourceBuffer.appendBuffer(buffer);
    await waitFor(sourceBuffer, 'updateend', true);
}

async function runAndWaitForLicenseRequest(session, callback, options) {
    var licenseRequestPromise = waitFor(session, 'message');
    await callback();
    let message = await licenseRequestPromise;

    let response = await getResponse(message, options);
    await session.update(response);
    if (!options || options.silent)
        consoleWrite('PROMISE: session.update() resolved');
}

async function fetchAndWaitForLicenseRequest(session, sourceBuffer, url) {
    await runAndWaitForLicenseRequest(session, async () => {
        await fetchAndAppend(sourceBuffer, 'content/elementary-stream-video-header-keyid-4.m4v');
    });
}

async function fetchAppendAndWaitForEncrypted(video, mediaKeys, sourceBuffer, url, options) {
    let updateEndPromise = fetchAndSilentAppend(sourceBuffer, url, options);
    let encryptedEvent = await waitFor(video, 'encrypted', options && options.silent);

    let session = mediaKeys.createSession();
    session.generateRequest(encryptedEvent.initDataType, encryptedEvent.initData);
    let message = await waitFor(session, 'message', options && options.silent);
    let response = await getResponse(message, options);
    await session.update(response);
    if (!options || !options.silent)
        consoleWrite('PROMISE: session.update() resolved');
    await updateEndPromise;
    if (!options || !options.silent)
        consoleWrite(`EVENT(updateend)`);
    return session;
}

async function createBufferAndAppend(mediaSource, type, url, options) {
    let sourceBuffer = mediaSource.addSourceBuffer(type);
    if (!options || !options.silent)
        consoleWrite('Created sourceBuffer');
    await fetchAndAppend(sourceBuffer, url, options);
    return sourceBuffer;
}

async function createBufferAppendAndWaitForEncrypted(video, mediaSource, mediaKeys, type, url, options) {
    let sourceBuffer = mediaSource.addSourceBuffer(type);
    if (!options || !options.silent)
        consoleWrite('Created sourceBuffer');

    let session = await fetchAppendAndWaitForEncrypted(video, mediaKeys, sourceBuffer, url, options);

    return {sourceBuffer, session};
}

