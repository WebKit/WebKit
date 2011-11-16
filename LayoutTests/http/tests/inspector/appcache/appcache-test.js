var framesCount = 0;

function createAndNavigateIFrame(url)
{
    var iframe = document.createElement("iframe");
    iframe.src = url;
    iframe.name = "frame" + ++framesCount;
    iframe.id = iframe.name;
    document.body.appendChild(iframe);
}

function removeIFrame(name)
{
    var iframe = document.querySelector("#" + name);
    iframe.parentElement.removeChild(iframe);
}

function navigateIFrame(name, url)
{
    var iframe = document.querySelector("#" + name);
    iframe.src = url;
}

function swapFrameCache(name)
{
    var iframe = document.querySelector("#" + name);
    iframe.contentWindow.applicationCache.swapCache();
}

var initialize_ApplicationCacheTest = function() {

InspectorTest.createAndNavigateIFrame = function(url, callback)
{
    InspectorTest.addSniffer(WebInspector.ResourceTreeModel.prototype, "_frameNavigated", frameNavigated);
    InspectorTest.evaluateInPage("createAndNavigateIFrame(unescape('" + escape(url) + "'))");

    function frameNavigated(frame)
    {
        callback(frame.id);
    }
}

InspectorTest.navigateIFrame = function(frameId, url, callback)
{
    var frame = WebInspector.resourceTreeModel.frameForId(frameId)
    InspectorTest.evaluateInPage("navigateIFrame(unescape('" + escape(frame.name) +"'), unescape('" + escape(url) + "'))");
    InspectorTest.addSniffer(WebInspector.ResourceTreeModel.prototype, "_frameNavigated", frameNavigated);

    function frameNavigated(frame)
    {
        callback(frame.id);
    }
}

InspectorTest.removeIFrame = function(frameId, callback)
{
    var frame = WebInspector.resourceTreeModel.frameForId(frameId)
    InspectorTest.evaluateInPage("removeIFrame(unescape('" + escape(frame.name) +"'))");
    InspectorTest.addSniffer(WebInspector.ResourceTreeModel.prototype, "_frameDetached", frameDetached);

    function frameDetached(frame)
    {
        callback(frame.id);
    }
}

InspectorTest.swapFrameCache = function(frameId)
{
    var frame = WebInspector.resourceTreeModel.frameForId(frameId);
    InspectorTest.evaluateInPage("swapFrameCache(unescape('" + escape(frame.name) +"'))");
}


InspectorTest.dumpApplicationCache = function()
{
    InspectorTest.dumpApplicationCacheTree();
    InspectorTest.dumpApplicationCacheModel();
    InspectorTest.addResult("");
}

InspectorTest.dumpApplicationCacheTree = function()
{
    InspectorTest.addResult("Dumping application cache tree:");
    var applicationCacheTreeElement = WebInspector.panels.resources.applicationCacheListTreeElement;
    if (!applicationCacheTreeElement.children.length) {
        InspectorTest.addResult("    (empty)");
        return;
    }
    for (var i = 0; i < applicationCacheTreeElement.children.length; ++i) {
        var manifestTreeElement = applicationCacheTreeElement.children[i];
        InspectorTest.addResult("    Manifest URL: " + manifestTreeElement.manifestURL);
        if (!manifestTreeElement.children.length) {
            InspectorTest.addResult("    (no frames)");
            continue;
        }
        for (var j = 0; j < manifestTreeElement.children.length; ++j) {
            var frameTreeElement = manifestTreeElement.children[j];
            InspectorTest.addResult("        Frame: " + frameTreeElement.displayName);
        }
    }
}

InspectorTest.frameIdToString = function(frameId)
{
    if (!InspectorTest.framesByFrameId)
        InspectorTest.framesByFrameId = {};
    var frame = WebInspector.resourceTreeModel.frameForId(frameId);
    if (!frame)
        frame = InspectorTest.framesByFrameId[frameId];
    InspectorTest.framesByFrameId[frameId] = frame;

    return frame.name;
}

InspectorTest.applicationCacheStatusToString = function(status)
{
    var statusInformation = {};
    statusInformation[applicationCache.UNCACHED]    = "UNCACHED";
    statusInformation[applicationCache.IDLE]        = "IDLE";
    statusInformation[applicationCache.CHECKING]    = "CHECKING";
    statusInformation[applicationCache.DOWNLOADING] = "DOWNLOADING";
    statusInformation[applicationCache.UPDATEREADY] = "UPDATEREADY";
    statusInformation[applicationCache.OBSOLETE]    = "OBSOLETE";
    return statusInformation[status] || statusInformation[applicationCache.UNCACHED];
}

InspectorTest.dumpApplicationCacheModel = function()
{
    InspectorTest.addResult("Dumping application cache model:");
    var model = WebInspector.panels.resources._applicationCacheModel;

    var frameIds = [];
    for (var frameId in model._manifestURLsByFrame)
        frameIds.push(frameId);

    function compareFunc(a, b) {
        return InspectorTest.frameIdToString(a).localeCompare(InspectorTest.frameIdToString(b));
    }
    frameIds.sort(compareFunc);

    if (!frameIds.length) {
        InspectorTest.addResult("    (empty)");
        return;
    }
    for (var i = 0; i < frameIds.length; ++i) {
        var frameId = frameIds[i];
        var manifestURL = model.frameManifestURL(frameId);
        var status = model.frameManifestStatus(frameId);
        InspectorTest.addResult("    Frame: " + InspectorTest.frameIdToString(frameId));
        InspectorTest.addResult("        manifest url: " + manifestURL);
        InspectorTest.addResult("        status:       " + InspectorTest.applicationCacheStatusToString(status));
    }
}

InspectorTest.waitForFrameManifestURLAndStatus = function(frameId, manifestURL, status, callback)
{
    var frameManifestStatus = WebInspector.panels.resources._applicationCacheModel.frameManifestStatus(frameId);
    var frameManifestURL = WebInspector.panels.resources._applicationCacheModel.frameManifestURL(frameId);
    if (frameManifestStatus === status && frameManifestURL.indexOf(manifestURL) !== -1) {
        callback();
        return;
    }

    var handler = InspectorTest.waitForFrameManifestURLAndStatus.bind(this, frameId, manifestURL, status, callback);
    InspectorTest.addSniffer(WebInspector.ApplicationCacheModel.prototype, "_frameManifestUpdated", handler);
}

InspectorTest.startApplicationCacheStatusesRecording = function()
{
    if (InspectorTest.applicationCacheStatusesRecords) {
        InspectorTest.applicationCacheStatusesRecords = {};
        return;
    }

    InspectorTest.applicationCacheStatusesRecords = {};
    function addRecord(frameId, manifestURL, status) {
        var record = {};
        record.manifestURL = manifestURL;
        record.status = status;
        if (!InspectorTest.applicationCacheStatusesRecords[frameId])
            InspectorTest.applicationCacheStatusesRecords[frameId] = [];
        InspectorTest.applicationCacheStatusesRecords[frameId].push(record);

        if (InspectorTest.awaitedFrameStatusEventsCount && InspectorTest.awaitedFrameStatusEventsCount[frameId]) {
            InspectorTest.awaitedFrameStatusEventsCount[frameId].count--;
            if (!InspectorTest.awaitedFrameStatusEventsCount[frameId].count)
                InspectorTest.awaitedFrameStatusEventsCount[frameId].callback();
        }
    }

    InspectorTest.addSniffer(WebInspector.ApplicationCacheModel.prototype, "_frameManifestUpdated", addRecord, true);
}

InspectorTest.ensureFrameStatusEventsReceived = function(frameId, count, callback)
{
    var records = InspectorTest.applicationCacheStatusesRecords[frameId] || [];
    var eventsLeft = count - records.length;

    if (!eventsLeft) {
        callback();
        return;
    }

    if (!InspectorTest.awaitedFrameStatusEventsCount)
        InspectorTest.awaitedFrameStatusEventsCount = {};
    InspectorTest.awaitedFrameStatusEventsCount[frameId] = { count: eventsLeft, callback: callback };
}

};

