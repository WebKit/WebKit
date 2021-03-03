if (window.internals)
    window.internals.settings.setScreenCaptureEnabled(true);

async function callGetDisplayMedia(options)
{
    let promise;
    window.internals.withUserGesture(() => {
        promise = navigator.mediaDevices.getDisplayMedia(options);
    });
    const stream = await promise; 
    const track = stream.getVideoTracks()[0];

    // getSettings is not computing the correct parameters right away. We should probably fix this.
    while (true) {
        const settings = track.getSettings();
        if (settings.width && settings.height)
            break;
        await new Promise(resolve => setTimeout(resolve, 50));
    }
    return stream;
}

async function waitForHeight(track, value) {
    let counter = 100;
    while (--counter > 0) {
        const settings = track.getSettings();
        if (settings.height === value)
            break;
        await new Promise(resolve => setTimeout(resolve, 50));
    }
    assert_greater_than(counter, 0, "waitForHeight");
}

async function waitForWidth(track, value) {
    let counter = 100;
    while (--counter > 0) {
        const settings = track.getSettings();
        if (settings.width === value)
            break;
        await new Promise(resolve => setTimeout(resolve, 50));
    }
    assert_greater_than(counter, 0, "waitForWidth");
}
