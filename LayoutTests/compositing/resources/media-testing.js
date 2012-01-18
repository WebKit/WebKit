
if (window.layoutTestController)
    layoutTestController.waitUntilDone();

function ensureFrameVisible(video, videoRenderedCallbac)
{
    video.addEventListener('seeked', videoRenderedCallbac, false);
    video.currentTime = 0.5;
}

function setupVideo(videoElement, videoPath, canPlayThroughCallback, videoRenderedCallbac)
{
    var mediaFile = findMediaFile("video", videoPath);
    videoElement.addEventListener("canplaythrough", function () {
        if (canPlayThroughCallback)
            canPlayThroughCallback();
        ensureFrameVisible(this, videoRenderedCallbac);
    }, false);
    videoElement.src = mediaFile;
}
