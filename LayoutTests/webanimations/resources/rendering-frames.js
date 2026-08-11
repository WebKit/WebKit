
const renderingFrames = async numberOfFrames => {
    return new Promise(resolve => {
        let elapsedFrames = 0;
        (function rAFCallback() {
            if (elapsedFrames++ >= numberOfFrames)
                resolve();
            else
                requestAnimationFrame(rAFCallback);
        })();
    });
}
