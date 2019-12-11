
if (window.testRunner)
    testRunner.waitUntilDone();

function ensureFrameVisible(video, videoRenderedCallback)
{
    var playingHandler = function() {
        video.pause();
        video.currentTime = 0;
    };

    var seekedHandler = function() {
        if (videoRenderedCallback)
            videoRenderedCallback();
    };

    video.addEventListener('playing', playingHandler, false);
    video.addEventListener('seeked', seekedHandler, false);
    video.play();
}

function setupVideo(videoElement, videoPath, canPlayThroughCallback, videoRenderedCallback)
{
    var mediaFile = findMediaFile("video", videoPath);
    videoElement.addEventListener("canplaythrough", function () {
        if (canPlayThroughCallback)
            canPlayThroughCallback();
        ensureFrameVisible(this, videoRenderedCallback);
    }, false);
    videoElement.src = mediaFile;
}
