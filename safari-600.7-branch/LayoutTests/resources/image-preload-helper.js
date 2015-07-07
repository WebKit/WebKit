// This is used by several tests to help get images reliably preloaded.

// Given a node, loads all urls specified in style declarations
// attached to the node or it's decendants.
// imageCount specifies the number of images we expect to find (to try to add some
// protection against brittleness due to imperfect url parsing, since other missing a preload
// will typically result in a test that fails only occasionally).
// If failPattern is specified, then any url that matches the regex
// will be expected to fail to load.
function preloadImagesFromStyle(rootNode, imageCount, onComplete, failPattern) {
    var basePath = location.href.substring(0, location.href.lastIndexOf('/') + 1);
    var nodes = rootNode.querySelectorAll('[style]');
    var imagesToLoad = [];
    var seenUrls = {};
    for (var i = 0; i < nodes.length; i++) {
        var urls = nodes[i].style.cssText.split(/url\w*\(([^)]*)\)/);
        for (var j = 1; j < urls.length; j += 2) {
            // Attempt to convert URL to a relative path in order to have deterministic error messages.
            var url = urls[j];
            if (url.indexOf(basePath) == 0)
                url = url.substring(basePath.length);

            var error = false;
            if (failPattern && failPattern.test(url))
                error = true;
            if (url in seenUrls)
                continue;
            seenUrls[url] = true;
            imagesToLoad.push({url: url, error: error});
        }
    }

    if (imageCount != imagesToLoad.length) {
        var msg = 'Found the following ' + imagesToLoad.length + ' images, when expecting ' + imageCount + ': ';
        for (var i = 0; i < imagesToLoad.length; i++) {
            msg += '\n' + imagesToLoad[i].url;
        }
        testFailed(msg);
    }

    loadImages(imagesToLoad, onComplete);
}

// For each object in the given array, attempt to load the image specified by the
// url property.  If the error property is specified and true, then the load is
// expected to fail.  Once all loads have completed or failed, onComplete is invoked.
function loadImages(imagesToLoad, onComplete) {

    var imagesLeftToLoad = imagesToLoad.length;

    function onImageLoad(url, success, e) {
        // This debug output order is non-deterministic - only show when not running in DRT
        if (!window.testRunner)
            debug( 'Event "' + e.type + '": ' + url);

        if (!success)
            testFailed('Got unexpected \'' + e.type + '\' event for image: ' + url);
        
        imagesLeftToLoad--;
        if (imagesLeftToLoad == 0) {
            onComplete();
        }
        if (imagesLeftToLoad < 0)
            testFailed('Got more load/error callbacks than expected.');
    }

    for (var i = 0; i < imagesToLoad.length; i++) {
        var img = new Image();
        var expectError = imagesToLoad[i].error;
        img.addEventListener('load', onImageLoad.bind(undefined, imagesToLoad[i].url, !expectError));
        img.addEventListener('error', onImageLoad.bind(undefined, imagesToLoad[i].url, !!expectError));
        img.src = imagesToLoad[i].url;
    }
}
