
importScripts('../media-source-loader.js');

function waitForEvent(element, type) {
    return new Promise(resolve => {
        element.addEventListener(type, event => {
            resolve(event);
        }, { once: true });
    });
}

function loaderPromise(loader) {
    return new Promise((resolve, reject) => {
        loader.onload = resolve;
        loader.onerror = reject;
    });
}

function logToMain(msg) {
    postMessage({topic: 'info', arg: msg});
}

function logErrorToMain(msg) {
    postMessage({topic: 'error', arg: msg});
}

function logTrackInfo(sourceBuffer, type) {
    let tracks = sourceBuffer[type];
    if (tracks) {
        logErrorToMain(`sourceBuffer.${type} is not null`);
        return false;
    }
    
    logToMain(`sourceBuffer.${type} is undefined`);
    return true;
}

function getPath(fullpath) {
    return fullpath.substring(0, fullpath.lastIndexOf('/'));
}

var mediaSource;

onmessage = async (msg) => {
    switch (msg.data.topic) {
    case 'setup':
        try {
            mediaSource = new ManagedMediaSource();
            const handle = mediaSource.handle;
            // Transfer the MediaSourceHandle to the main thread for use in attaching to
            // the main thread media element that will play the content being buffered
            // here in the worker.
            postMessage({topic: 'handle', arg: handle}, [handle]);

            await waitForEvent(mediaSource, 'sourceopen');
            logToMain("'sourceopen' event received");
        } catch (e) {
            logErrorToMain('MSE not supported');
        }
        break;
    case 'manifest':
        const path = getPath(getPath(msg.data.arg));
        const loader = new MediaSourceLoader(msg.data.arg, path + '/');
        await loaderPromise(loader);
        logToMain("loaderPromise resolved");

        sourceBuffer = mediaSource.addSourceBuffer(loader.type());
        sourceBuffer.appendBuffer(loader.initSegment());
        await waitForEvent(sourceBuffer, 'update');
        logToMain("sourceBuffer 'update' event received for initsegment");

        sourceBuffer.appendBuffer(loader.mediaSegment(0));
        await waitForEvent(sourceBuffer, 'update');
        logToMain("sourceBuffer 'update' event received");

        if (!logTrackInfo(sourceBuffer, 'audioTracks'))
            return;
        
        if (!logTrackInfo(sourceBuffer, 'videoTracks'))
            return;
        
        if (!logTrackInfo(sourceBuffer, 'textTracks'))
            return;

        postMessage({topic: 'done'});
        break;
    default:
        logErrorToMain('unsupported message received');
    }
};
