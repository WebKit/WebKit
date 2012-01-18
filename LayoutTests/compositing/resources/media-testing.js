
if (window.layoutTestController)
    layoutTestController.waitUntilDone();

function ensureFrameVisible(video, videoRenderedCallback)
{
    video.addEventListener('timeupdate', function() {
        video.pause();
        video.currentTime = 0;
        if (videoRenderedCallback)
            videoRenderedCallback();
    }, false);
    
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
