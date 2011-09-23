function init()
{
    var totalCount = document.getElementsByTagName('video').length;
    var count = totalCount;
    document.addEventListener("canplaythrough", function () {
        if (!--count) {
            var video = document.getElementsByTagName('video')[0];
            video.play();
            video.addEventListener("playing", function() {
                video.pause();
                video.currentTime = 0;
                video.addEventListener("seeked", function() {
                    if (window.layoutTestController)
                        layoutTestController.notifyDone();
                });
            });
            document.body.offsetLeft;
        }
    }, true);
}

if (window.layoutTestController) {
    layoutTestController.waitUntilDone();
    setTimeout(function() { 
        document.body.appendChild(document.createTextNode('FAIL')); 
        if (window.layoutTestController)
            layoutTestController.notifyDone();
    } , 8000);
}

function initAndPause()
{
    var totalCount = document.getElementsByTagName('video').length;
    var count = totalCount;
    document.addEventListener("canplaythrough", function () {
        if (!--count) {
            var video = document.getElementsByTagName('video')[0];
            video.play();
            video.addEventListener("playing", function() {
                video.pause();
                video.addEventListener("pause", function() {
                    if (window.layoutTestController)
                        layoutTestController.notifyDone();
                });
            });
        }
    }, true);
}

