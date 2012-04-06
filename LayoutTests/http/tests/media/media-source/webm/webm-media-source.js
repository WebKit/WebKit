var clusterInfo = [
    { 'ts': 0 },
    { 'ts': 0.38499999046325684 },
    { 'ts': 0.7789999842643738 },
    { 'ts': 1.1740000247955322 },
    { 'ts': 1.5920000076293945 },
    { 'ts': 1.9869999885559082  },
    { 'ts': 2.38100004196167 },
    { 'ts': 2.7760000228881836 },
];

var headerData = null;
var clusterData = [];

function getData(url, callback)
{
    var request = new XMLHttpRequest();
    request.open("GET", url, true);
    request.responseType = 'arraybuffer';
    request.onload = function() {
        if (request.status != 200) {
            failTest("Unexpected status code " + request.status + " for " + url);
            callback(null);
            return;
        }

        callback(new Uint8Array(request.response));
    };
    request.send();
}

function createClusterGetFunction(clusterIndex, callback) {
    return function(data) {
        if (!data) {
            callback(false);
            return;
        }

        clusterData.push(data);

        if (clusterData.length == getClusterCount()) {
            callback(true);
            return;
        }

        var nextClusterIndex = clusterIndex + 1;
        getData("/media/resources/media-source/webm/test.webm.cluster-" + nextClusterIndex,
                createClusterGetFunction(nextClusterIndex, callback));
    };
}

function loadWebMData(callback) {
    getData("/media/resources/media-source/webm/test.webm.headers", function(data) {
        if (!data) {
            callback(false);
            return;
        }

        headerData = data;

        var clusterIndex = 0;
        getData("/media/resources/media-source/webm/test.webm.cluster-" + clusterIndex,
                createClusterGetFunction(clusterIndex, callback));
    });
}

function getHeaders()
{
    return headerData;
}

function getClusterCount()
{
    return clusterInfo.length;
}

function getCluster(clusterIndex)
{
    return clusterData[clusterIndex];
}

function getClusterTimeForIndex(clusterIndex)
{
    return clusterInfo[clusterIndex]['ts'];
}

function getClusterIndexForTimestamp(timestamp)
{
    if (timestamp <= clusterInfo[0]['ts'])
        return 0;

    for (var clusterIndex = 1; clusterIndex < clusterInfo.length; clusterIndex++) {
        if (timestamp < clusterInfo[clusterIndex]['ts']) {
            return clusterIndex - 1;
        }
    }

    return clusterInfo.length - 1;
}

function setSrcToMediaSourceURL(videoTag)
{
    if (!videoTag.webkitMediaSourceURL) {
        failTest("webkitMediaSourceURL is not available");
        return;
    }

    videoTag.src = videoTag.webkitMediaSourceURL;
}

function appendHeaders(videoTag)
{
    if (!videoTag.webkitSourceAppend) {
        failTest("webkitSourceAppend() is not available");
        return;
    }

    videoTag.webkitSourceAppend(getHeaders());
}

function appendCluster(videoTag, clusterIndex)
{
    if (!videoTag.webkitSourceAppend) {
        failTest("webkitSourceAppend() is not available");
        return;
    }

    if (clusterIndex < 0 || clusterIndex >= getClusterCount()) {
        failTest("Invalid cluster index " + clusterIndex);
        return;
    }

    try {
        var cluster = getCluster(clusterIndex);
        videoTag.webkitSourceAppend(cluster);
    } catch (err) {
        consoleWrite(err);
    }
}

function appendUntilEndOfStream(videoTag, startIndex)
{
    if (!videoTag.webkitSourceAppend) {
        failTest("webkitSourceAppend() is not available");
        return;
    }

    for (var clusterIndex = startIndex; clusterIndex < getClusterCount(); clusterIndex++) {
        videoTag.webkitSourceAppend(getCluster(clusterIndex));
    }
    videoTag.webkitSourceEndOfStream(videoTag.EOS_NO_ERROR);
}

function logSourceState(videoTag)
{
    consoleWrite("webkitSourceState : " + getSourceStateName(videoTag.webkitSourceState));
}

function getSourceStateName(state)
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
}

function getReadyStateName(state)
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
}

function expectSourceState(videoTag, expected)
{
    if (videoTag.webkitSourceState != expected) {
        failTest("Unexpected source state. Expected " +
                 getSourceStateName(expected) +
                 " got " + getSourceStateName(videoTag.webkitSourceState));
    }
}

function expectReadyState(videoTag, expected)
{
    if (videoTag.readyState != expected) {
        failTest("Unexpected ready state. Expected " +
                 getReadyStateName(expected) +
                 " got " + getReadyStateName(videoTag.readyState));
    }
}