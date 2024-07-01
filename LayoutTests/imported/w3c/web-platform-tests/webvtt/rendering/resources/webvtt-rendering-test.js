if (window.testRunner)
    testRunner.waitUntilDone();

window.addEventListener('load', async event => {
    let waitFor = (object, type) => {
        return new Promise(resolve => {
            object.addEventListener(type, resolve);
        }, { once: true });
    };

    let video = document.querySelector('video');
    if (!video.textTracks.length)
        await waitFor(video.textTracks, 'addtrack');
    if (video.textTracks[0].mode !== 'showing')
        video.textTracks[0].mode = 'showing';
    if (!video.textTracks[0].activeCues)
        await waitFor(video.textTracks[0], 'cuechange');

    if (window.testRunner)
        testRunner.notifyDone();
});