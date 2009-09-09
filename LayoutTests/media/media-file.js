var audioCodecs = [
    ["audio/aac", "m4a"],
    ["audio/mpeg", "mp3"],
    ["audio/wav", "wav"],
    ["audio/ogg", "ogg"]
];

var videoCodecs = [
    ["video/mp4", "mp4"],
    ["video/mpeg", "mpg"],
    ["video/quicktime", "mov"],
    ["video/ogg", "ogg"]
];

function findMediaFile(tagName, name) {
    var codecs;
    if (tagName == "audio")
        codecs = audioCodecs;
    else
        codecs = videoCodecs;

    var element;
    try {
        element = document.getElementsByTagName(tagName)[0];
    } catch (ex) {
        element = document.createElement(tagName);
    }

    for (var i = 0; i < codecs.length; ++i) {
        if (element.canPlayType(codecs[i][0]))
            return name + "." + codecs[i][1];
    }

    return "";
}

function setSrcByTagName(tagName, src) {
    var elements = document.getElementsByTagName(tagName);
    if (elements) {
        for (var i = 0; i < elements.length; ++i)
            elements[i].src = src;
    }
}

function setSrcById(id, src) {
    var element = document.getElementById(id);
    if (element)
        id.src = src;
}
