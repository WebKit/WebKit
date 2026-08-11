if (window.internals)
    window.internals.settings.setScreenCaptureEnabled(true);

async function callGetDisplayMedia(options)
{
    let promise;
    window.internals.withUserGesture(() => {
        promise = navigator.mediaDevices.getDisplayMedia(options);
    });
    return await promise;
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
