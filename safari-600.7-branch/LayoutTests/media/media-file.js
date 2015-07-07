var audioCodecs = [
    ["audio/wav", "wav"],
    ["audio/aac", "m4a"],
    ["audio/mpeg", "mp3"],
    ["audio/ogg", "oga"]
];

var videoCodecs = [
    ["video/mp4", "mp4"],
    ["video/mpeg", "mpg"],
    ["video/quicktime", "mov"],
    ["video/ogg", "ogv"]
];

function findMediaFile(tagName, name) {
    var codecs;
    if (tagName == "audio")
        codecs = audioCodecs;
    else
        codecs = videoCodecs;

    var element = document.getElementsByTagName(tagName)[0];
    if (!element)
        element = document.createElement(tagName);

    for (var i = 0; i < codecs.length; ++i) {
        if (element.canPlayType(codecs[i][0]))
            return name + "." + codecs[i][1];
    }

    return "";
}

function mimeTypeForExtension(extension) {
    for (var i = 0; i < videoCodecs.length; ++i) {
        if (extension == videoCodecs[i][1])
            return videoCodecs[i][0];
    }
    for (var i = 0; i < audioCodecs.length; ++i) {
        if (extension == audioCodecs[i][1])
            return audioCodecs[i][0];
    }

    return "";
}

function mimeTypeForFile(filename) {
 var lastPeriodIndex = filename.lastIndexOf(".");
  if (lastPeriodIndex > 0)
    return mimeTypeForExtension(filename.substring(lastPeriodIndex + 1));

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
        element.src = src;
}

function stripExtension(filename) {
  var lastPeriodIndex = filename.lastIndexOf(".");
  if (lastPeriodIndex > 0)
    return filename.substring(0, lastPeriodIndex);
  return filename;
}
