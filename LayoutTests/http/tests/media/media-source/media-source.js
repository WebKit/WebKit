var MediaSourceTest = {};

MediaSourceTest.SegmentHelper = function(segmentInfo)
{
    this.MediaSegmentsToLoad = 0;
    this.SourceID = 'sourceId';

    this.videoTag = null;
    this.segmentInfo = segmentInfo;
    this.initSegment = null;
    this.mediaSegments = [];

    // Limit how many media segments we load based on time.
    var maxDuration = 3;
    for (var i in this.segmentInfo.media) {
        if (this.segmentInfo.media[i].timecode > maxDuration)
            break;
        this.MediaSegmentsToLoad++;
    }

    // Require 3 or more segments for testing. This guarantees that there are at least 3 random
    // access points in the content.
    if (this.MediaSegmentsToLoad < 3)
        consoleWrite("Need more media segments for testing. " + this.MediaSegmentsToLoad + " is not enough");
};

MediaSourceTest.SegmentHelper.prototype.init = function(videoTag, doneCallback)
{
    if (!videoTag) {
        doneCallback(false);
        return;
    }

    this.videoTag = videoTag;
    var t = this;
    var getDataCallback = function(data) { t.getInitSegmentDone_(doneCallback, data); };
    this.getData_(this.segmentInfo.url,
                  this.segmentInfo.init.offset, this.segmentInfo.init.size,
                  getDataCallback);
};

MediaSourceTest.SegmentHelper.prototype.getInitSegmentDone_ = function(doneCallback, data)
{
    if (!data) {
        doneCallback(false);
        return;
    }

    this.initSegment = data;

    this.downloadMediaSegment_(0, doneCallback);
};

MediaSourceTest.SegmentHelper.prototype.downloadMediaSegment_ = function(index, doneCallback)
{
    var t = this;
    var getDataCallback = function(data) { t.downloadMediaSegmentDone_(index, doneCallback, data); };
    this.getData_(this.segmentInfo.url,
                  this.segmentInfo.media[index].offset,
                  this.segmentInfo.media[index].size,
                  getDataCallback);
};

MediaSourceTest.SegmentHelper.prototype.downloadMediaSegmentDone_ = function(index, doneCallback, data)
{
    if (!data) {
        doneCallback(false);
        return;
    }

    this.mediaSegments.push(data);

    if (this.mediaSegments.length == this.MediaSegmentsToLoad) {
        doneCallback(true);
        return;
    }

    this.downloadMediaSegment_(index + 1, doneCallback);
};


MediaSourceTest.SegmentHelper.prototype.getData_ = function (url, start, length, callback)
{
    var request = new XMLHttpRequest();
    request.open("GET", url, true);

    if (start && start < 0) {
        failTest("Invalid start parameter " + start);
        return;
    }

    if (length && length <= 0) {
        failTest("Invalid length parameter " + length);
        return;
    }

    var expectedStatus = 200;
    if (start >= 0) {
        var startOffset = start;
        var rangeString = 'bytes=' + startOffset + '-';
        if (length && length > 0) {
            var endOffset = startOffset + length - 1;
            rangeString += endOffset;
        }

        expectedStatus = 206;
        request.setRequestHeader('Range', rangeString);
    }

    request.responseType = 'arraybuffer';
    request.onload = function()
    {
        if (request.status != expectedStatus) {
            failTest("Unexpected status code " + request.status + " for " + url);
            callback(null);
            return;
        }

        callback(new Uint8Array(request.response));
    };
    request.send();
};

MediaSourceTest.SegmentHelper.prototype.addSourceId = function()
{
    this.videoTag.webkitSourceAddId(this.SourceID, this.segmentInfo.type);
};

MediaSourceTest.SegmentHelper.prototype.appendInitSegment = function()
{
    this.videoTag.webkitSourceAppend(this.SourceID, this.initSegment);
};

MediaSourceTest.SegmentHelper.prototype.appendMediaSegment = function(index)
{
    this.videoTag.webkitSourceAppend(this.SourceID, this.mediaSegments[index]);
};

MediaSourceTest.SegmentHelper.prototype.appendUntilEndOfStream = function(startIndex)
{
    if (!this.videoTag.webkitSourceAppend) {
        failTest("webkitSourceAppend() is not available");
        return;
    }

    for (var index = startIndex; index < this.mediaSegments.length; index++)
        this.appendMediaSegment(index);

    this.videoTag.webkitSourceEndOfStream(this.videoTag.EOS_NO_ERROR);
};

MediaSourceTest.SegmentHelper.prototype.getTimeForMediaSegment = function(index)
{
    return this.segmentInfo.media[index].timecode;
};

MediaSourceTest.SegmentHelper.prototype.getMediaSegmentIndexForTimestamp = function(timestamp)
{
    if (timestamp <= this.getTimeForMediaSegment(0))
        return 0;

    for (var index = 1; index < this.mediaSegments.length; index++) {
        if (timestamp < this.getTimeForMediaSegment(index)) {
            return index - 1;
        }
    }

    return this.mediaSegments.length - 1;
};


MediaSourceTest.setSrcToMediaSourceTestURL = function(videoTag)
{
    if (!videoTag.webkitMediaSourceURL) {
        failTest("webkitMediaSourceURL is not available");
        return;
    }

    videoTag.src = videoTag.webkitMediaSourceURL;
};

MediaSourceTest.mediaErrorString = function(videoTag)
{
    var errorString = "UNKNOWN";
    switch(videoTag.error.code) {
        case MediaError.MEDIA_ERR_ABORTED:
            errorString = "MEDIA_ERR_ABORTED";
            break;
        case MediaError.MEDIA_ERR_NETWORK:
            errorString = "MEDIA_ERR_NETWORK";
            break;
        case MediaError.MEDIA_ERR_DECODE:
            errorString = "MEDIA_ERR_DECODE";
            break;
        case MediaError.MEDIA_ERR_SRC_NOT_SUPPORTED:
            errorString = "MEDIA_ERR_SRC_NOT_SUPPORTED";
            break;
    }

    return errorString;
};

MediaSourceTest.defaultOnErrorChecks = function(event)
{
    var videoTag = event.target;

    consoleWrite("EVENT(error) : " + MediaSourceTest.mediaErrorString(videoTag));

    if (videoTag.webkitSourceState != HTMLMediaElement.SOURCE_CLOSED) {
        consoleWrite("Unexpected source state. Expected SOURCE_CLOSED" +
                     " got " + getSourceStateName(videoTag.webkitSourceState));
    }
};

MediaSourceTest.runNext_ = function(videoTag, testCaseIndex, tests, testWrapperFunction)
{
    if (testCaseIndex >= tests.length) {
        endTest();
        return;
    }

    var onOpenFunction = tests[testCaseIndex];
    var functionName = onOpenFunction.name;
    var runNextFunction = function()
    {
        MediaSourceTest.runNext_(videoTag, testCaseIndex + 1, tests, testWrapperFunction);
    };

    var errorEventHandler = function(event)
    {
        event.target.removeEventListener('error', errorEventHandler);
        MediaSourceTest.defaultOnErrorChecks(event);
        runNextFunction();
    };
    videoTag.addEventListener('error', errorEventHandler);

    consoleWrite("");
    consoleWrite("running " + functionName);

    if (testWrapperFunction) {
        onOpenFunction = testWrapperFunction(onOpenFunction);
    }

    onOpenFunction(runNextFunction, videoTag);
};

MediaSourceTest.startTesting = function(videoTag, tests)
{
    MediaSourceTest.runNext_(videoTag, 0, tests);
};

MediaSourceTest.startSourceOpenTesting = function(videoTag, tests)
{
    // Wrapper function that executes the test function when the
    // source open event has fired.
    var testWrapperFunction = function(testFunction)
    {
        return function(runNextFunction, videoTag)
        {
            MediaSourceTest.runOnSourceOpen(videoTag, function()
            {
                testFunction(runNextFunction, videoTag);
            });
        };
    };
    MediaSourceTest.runNext_(videoTag, 0, tests, testWrapperFunction);
};

MediaSourceTest.runOnSourceOpen = function(videoTag, onOpenFunction)
{
    var eventHandlerFunction = function (event)
    {
        event.target.removeEventListener('webkitsourceopen', eventHandlerFunction);
        onOpenFunction();
    };
    videoTag.addEventListener('webkitsourceopen', eventHandlerFunction);
    MediaSourceTest.setSrcToMediaSourceTestURL(videoTag);
};

MediaSourceTest.logSourceState = function(videoTag)
{
    consoleWrite("webkitSourceState : " + MediaSourceTest.getSourceStateName(videoTag.webkitSourceState));
};

MediaSourceTest.expectSourceState = function(videoTag, expected)
{
    if (videoTag.webkitSourceState != expected) {
        failTest("Unexpected source state. Expected " +
                 MediaSourceTest.getSourceStateName(expected) +
                 " got " + MediaSourceTest.getSourceStateName(videoTag.webkitSourceState));
    }
};

MediaSourceTest.getSourceStateName = function(state)
{
    var stateName = "UNKNOWN";
    switch (state) {
        case HTMLMediaElement.SOURCE_CLOSED:
            stateName = "SOURCE_CLOSED";
            break;
        case HTMLMediaElement.SOURCE_OPEN:
            stateName = "SOURCE_OPEN";
            break;
        case HTMLMediaElement.SOURCE_ENDED:
            stateName = "SOURCE_ENDED";
            break;
    }
    return stateName;
};

MediaSourceTest.expectReadyState = function(videoTag, expected)
{
    if (videoTag.readyState != expected) {
        failTest("Unexpected ready state. Expected " +
                 MediaSourceTest.getReadyStateName(expected) +
                 " got " + MediaSourceTest.getReadyStateName(videoTag.readyState));
    }
};

MediaSourceTest.getReadyStateName = function(state)
{
    var stateName = "UNKNOWN";
    switch (state) {
        case HTMLMediaElement.HAVE_NOTHING:
            stateName = "HAVE_NOTHING";
            break;
        case HTMLMediaElement.HAVE_METADATA:
            stateName = "HAVE_METADATA";
            break;
        case HTMLMediaElement.HAVE_CURRENT_DATA:
            stateName = "HAVE_CURRENT_DATA";
            break;
        case HTMLMediaElement.HAVE_FUTURE_DATA:
            stateName = "HAVE_FUTURE_DATA";
            break;
        case HTMLMediaElement.HAVE_ENOUGH_DATA:
            stateName = "HAVE_ENOUGH_DATA";
            break;
    }
    return stateName;
};
