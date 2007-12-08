function init()
{
    var count = document.getElementsByTagName('video').length;
    document.addEventListener("load", function () {
        if (!--count) {
            document.body.offsetLeft;
            if (window.layoutTestController)
                layoutTestController.notifyDone();
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
