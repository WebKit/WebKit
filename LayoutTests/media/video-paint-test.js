function init()
{
    var totalCount = document.getElementsByTagName('video').length;
    var count = totalCount;
    document.addEventListener("canplaythrough", function () {
        if (!--count) {
            var video = document.getElementsByTagName('video')[0];
            if (window.layoutTestController) {
                video.play();
                video.addEventListener("playing", function() { layoutTestController.notifyDone(); });
            }
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
