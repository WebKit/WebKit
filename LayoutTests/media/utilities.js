function once(target, name, callback) {
    let promise = new Promise((resolve, reject) => {
        target.addEventListener(name, (event) => {
            resolve(event);
        }, { once: true });
    });
    if (callback)
        promise.then(callback);
    return promise;
}

function fetchWithXHR(uri, onLoadFunction) {
    let p = new Promise((resolve, reject) => {
        let xhr = new XMLHttpRequest();
        xhr.open("GET", uri, true);
        xhr.responseType = "arraybuffer";
        xhr.addEventListener("load", () => {
            resolve(xhr.response);
        });
        xhr.send();
    });

    if (onLoadFunction)
        p.then(onLoadFunction);

    return p;
};

function loadSegment(sb, typedArrayOrArrayBuffer) {
    let typedArray = (typedArrayOrArrayBuffer instanceof ArrayBuffer)
        ? new Uint8Array(typedArrayOrArrayBuffer)
        : typedArrayOrArrayBuffer;
    return new Promise((resolve, reject) => {
        once(sb, 'update').then(() => { resolve(); });
        sb.appendBuffer(typedArray);
    });
}

function fetchAndLoad(sb, prefix, chunks, suffix) {
    // Fetch the buffers in parallel.
    let buffers = {};
    let fetches = [];
    for (var chunk of chunks)
        fetches.push(fetchWithXHR(prefix + chunk + suffix).then(((c, x) => buffers[c] = x).bind(null, chunk)));

    // Load them in series, as required per spec.
    return Promise.all(fetches).then(() => {
        let rv = Promise.resolve();
        for (let chunk of chunks)
            rv = rv.then(loadSegment.bind(null, sb, buffers[chunk]));
        return rv;
    });
}

const delay = ms => new Promise(res => setTimeout(res, ms));

function waitForVideoFrame(video, cb) {
    const p = new Promise((resolve) => {
        video.requestVideoFrameCallback((now, metadata) => resolve(now, metadata));
    });
    if (cb)
        p.then(cb);
    return p;
}

function waitForVideoFrameUntil(video, time, cb) {
    const p = new Promise(resolve => {
        const callback = ((now, metadata) => {
            if (metadata.mediaTime >= time) {
                resolve(now, metadata);
                return;
            }
            video.requestVideoFrameCallback(callback);
        });
        video.requestVideoFrameCallback(callback);
    });
    if (cb)
        p.then(cb);
    return p;
}
