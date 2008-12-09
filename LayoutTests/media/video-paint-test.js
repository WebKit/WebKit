function init()
{
    var totalCount = document.getElementsByTagName('video').length;
    var count = totalCount;
    document.addEventListener("canplaythrough", function () {
        if (!--count) {
            document.body.offsetLeft;
            if (window.layoutTestController)
                setTimeout(function() { layoutTestController.notifyDone(); }, totalCount * 100);
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
