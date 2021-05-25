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

function handlePromise(promise) {
    function handle() { }
    return promise.then(handle, handle);
}

//  The base64'd data below was generated from the corresponding content/test.{mp4,ogv} files with:
//    rm -f test-frame.ogv test-frame.mp4 && \
//      ~/tmp/ffmpeg/ffmpeg -i test.mp4 -an -vframes 1 test-frame.png && \
//      pngtopnm test-frame.png > test-frame.pnm && pnmscale 0.25 test-frame.pnm|pnmtopng > test-frame.png && \
//      ~/tmp/ffmpeg/ffmpeg -i test-frame.png -an -vframes 1 -s 80x60 test-frame.mp4 && \
//      ffmpeg2theora test-frame.mp4 -o test-frame.ogv && rm test-frame.png test-frame.pnm
//    Followed by [base64 -w 120 test-frame.ogv] and [base64 -w 120 test-frame.mp4].
// 

const dataURLs = {
    "mp4" : `data:video/mp4;base64,
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
AQAAAABMYXZmNTQuMC4xMDA=`,

    "ogg" : `data:video/ogg;base64,
T2dnUwACAAAAAAAAAABDt3UdAAAAAG1FPOwBQGZpc2hlYWQAAwAAAAAAAAAAAAAA6AMAAAAAAAAAAAAAAAAAAOgDAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
AABPZ2dTAAIAAAAAAAAAAKK34ysAAAAAwlsClQEqgHRoZW9yYQMCAQAFAAQAAFAAADwABAAAABkAAAABAAABAAABAgAAAIDAT2dnUwAAAAAAAAAAAABDt3Ud
AQAAAFp3fKUBUGZpc2JvbmUALAAAAKK34ysDAAAAGQAAAAAAAAABAAAAAAAAAAAAAAAAAAAAAAAAAAYAAABDb250ZW50LVR5cGU6IHZpZGVvL3RoZW9yYQ0K
T2dnUwAAAAAAAAAAAACit+MrAQAAAHoeC5AOWP///////////////5CBdGhlb3JhKwAAAFhpcGguT3JnIGxpYnRoZW9yYSAxLjEgMjAwOTA4MjIgKFRodXNu
ZWxkYSkBAAAAGgAAAEVOQ09ERVI9ZmZtcGVnMnRoZW9yYS0wLjI0gnRoZW9yYb7NKPe5zWsYtalJShBznOYxjFKUpCEIMYxiEIQhCEAAAAAAAAAAAAARba5T
Z5LI/FYS/Hg5W2zmKvVoq1QoEykkWhD+eTmbjWZTCXiyVSmTiSSCGQh8PB2OBqNBgLxWKhQJBGIhCHw8HAyGAsFAiDgVFtrlNnksj8VhL8eDlbbOYq9WirVC
gTKSRaEP55OZuNZlMJeLJVKZOJJIIZCHw8HY4Go0GAvFYqFAkEYiEIfDwcDIYCwUCIOBQLDw8PDw8PDw8PDw8PDw8PDw8PDw8PDw8PDw8PDw8PDw8PDw8PDw
8PDw8PDw8PDw8PDw8PDw8PDw8PDw8PDw8PDw8MDA8SFBQVDQ0OERIVFRQODg8SFBUVFQ4QERMUFRUVEBEUFRUVFRUSExQVFRUVFRQVFRUVFRUVFRUVFRUVFR
UQDAsQFBkbHA0NDhIVHBwbDg0QFBkcHBwOEBMWGx0dHBETGRwcHh4dFBgbHB0eHh0bHB0dHh4eHh0dHR0eHh4dEAsKEBgoMz0MDA4TGjo8Nw4NEBgoOUU4Dh
EWHTNXUD4SFiU6RG1nTRgjN0BRaHFcMUBOV2d5eGVIXF9icGRnYxMTExMTExMTExMTExMTExMTExMTExMTExMTExMTExMTExMTExMTExMTExMTExMTExMTEx
MTExMTExMTExMTExMSEhUZGhoaGhIUFhoaGhoaFRYZGhoaGhoZGhoaGhoaGhoaGhoaGhoaGhoaGhoaGhoaGhoaGhoaGhoaGhoaGhoaERIWHyQkJCQSFBgiJC
QkJBYYISQkJCQkHyIkJCQkJCQkJCQkJCQkJCQkJCQkJCQkJCQkJCQkJCQkJCQkJCQkJBESGC9jY2NjEhUaQmNjY2MYGjhjY2NjYy9CY2NjY2NjY2NjY2NjY2
NjY2NjY2NjY2NjY2NjY2NjY2NjY2NjY2MVFRUVFRUVFRUVFRUVFRUVFRUVFRUVFRUVFRUVFRUVFRUVFRUVFRUVFRUVFRUVFRUVFRUVFRUVFRUVFRUVFRUVEh
ISFRcYGRsSEhUXGBkbHBIVFxgZGxwdFRcYGRscHR0XGBkbHB0dHRgZGxwdHR0eGRscHR0dHh4bHB0dHR4eHhERERQXGhwgEREUFxocICIRFBcaHCAiJRQXGh
wgIiUlFxocICIlJSUaHCAiJSUlKRwgIiUlJSkqICIlJSUpKioQEBAUGBwgKBAQFBgcICgwEBQYHCAoMEAUGBwgKDBAQBgcICgwQEBAHCAoMEBAQGAgKDBAQE
BggCgwQEBAYICAB8Xlx0fV7c7D8vrrAaZid8hRvB1RN7csxFuo43wH7lEkS9wbGS+tVSNMyuxdiECcjB7R1Ml85htasNjKpSvPt3D8k7iGmZXYuxBC+RR4ar
UGxkvH5y7mJXR7R5Jwn3VUhBiuap91VIrsaCM5TSg9o867khwMrWY2+cP4rwvBLzt/wnHaYe0edSRMYC6tZmU1BrvhktIUf2gXoU8bHMuyNA7lB7R51ym213
sFcFKowIviT/i0Wscg+4RDubX+4haRsMxZWgN05K5FD3bzqS9VSVCPM4TpWs2C43ihFdgaSByeKHu3Xf/2TG8tgpB7PAtOs7jixWYw+Ayo5GjUTSybX/1KW5
2RxYfB8nBNLJtHgt4DPq6BZWBFpjyZX/1KW5Ca0evOwG1EX/A9j5fQm5hOz6W2CtcCaWTXTFAeZO71VIgCTX69y9TiaXag3Os2ES1DcLKw0/xR5HfnCqkpQF
0Z1kxKNfhZWLycml2keduHMQh3HubB/pbUUoCK5wxetZRZWPJF/bdyE21H2YjMOhP/pkthqKUCOEWVm68+1J5n7ahES5sOhaZPdOC5j4kc91FVIsrF8ofe+A
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
aWtuFpwSSSSHPcIguQA=`,
};

function dataURL(type) 
{
    const url = dataURLs[type];
    if (!url)
        console.log(`No base64 url for type ${type}`)

    return url;
}

function findDataURL(tagName) {
    let element = document.getElementsByTagName(tagName)[0];
    if (!element)
        element = document.createElement(tagName);

    for (let type in dataURLs) {
        if (element.canPlayType(`${tagName}/${type}`))
            return dataURLs[type];
    }

    console.log(`No base64 url for type ${type}`)
    return '';
}
