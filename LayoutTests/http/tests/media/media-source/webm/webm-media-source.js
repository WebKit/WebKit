var clusterInfo = [
    { 'ts': 0 },
    { 'ts': 0.38499999046325684 },
    { 'ts': 0.7789999842643738 },
    { 'ts': 1.1740000247955322 },
    { 'ts': 1.5920000076293945 },
    { 'ts': 1.9869999885559082  },
    { 'ts': 2.38100004196167 },
    { 'ts': 2.7760000228881836 },
    { 'ts': 3.1710000038146973 },
    { 'ts': 3.565999984741211 },
    { 'ts': 3.9600000381469727 },
    { 'ts': 4.377999782562256 },
    { 'ts': 4.7729997634887695 },
    { 'ts': 5.168000221252441 },
    { 'ts': 5.563000202178955 },
    { 'ts': 5.956999778747559 },
];

function getData(url)
{
    var request = new XMLHttpRequest();
    request.open("GET", url, false);
    request.responseType = 'arraybuffer';
    request.send();

    if (request.status != 200) {
        failTest("Unexpected status code " + request.status + " for " + url);
        return false;
    }

    return new Uint8Array(request.response);
}

function getHeaders()
{
    return getData("/media/resources/media-source/webm/test.webm.headers");
}

function getClusterCount()
{
    return clusterInfo.length;
}

function getCluster(clusterIndex)
{
    return getData("/media/resources/media-source/webm/test.webm.cluster-" + clusterIndex);
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

    videoTag.webkitSourceAppend(getCluster(clusterIndex));
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

function expectSourceState(videoTag, expected)
{
    if (videoTag.webkitSourceState != expected) {
        failTest("Unexpected source state. Expected " +
                 getSourceStateName(expected) +
                 " got " + getSourceStateName(videoTag.webkitSourceState));
    }
}