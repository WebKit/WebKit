function decodeUTF8(array) {
    var string = "";
    for (var i = 0; i < array.length; ++i)
        string += String.fromCharCode(array[i]);
    return decodeURIComponent(escape(string));
}

function encodeUTF8(string) {
    string = unescape(encodeURIComponent(string));

    var array = new Uint8Array(string.length);
    for (var i = 0; i < array.length; ++i)
        array[i] = string.charCodeAt(i);
    return array;
}

function arraysEqual(a, b) {
    if (a.length != b.length)
        return false;
    for (var i = 0; i < a.length; ++i) {
        if (a[i] != b[i])
            return false;
    }
    return true;
}

function arrayToString(array) {
    return "[" + Array.prototype.join.call(array, ", ") + "]";
}

function setHeader(s) {
}

function print(s) {
    document.getElementById("console").innerHTML += "<br/>" + s;
}

function tryArray(array) {
    try {
        var string = decodeUTF8(array);
        try {
            var array2 = encodeUTF8(string);
            if (!arraysEqual(array, array2)) {
                print("Round trip failed: " + arrayToString(array) + " turned into " + arrayToString(array2));
                return;
            }
        } catch (e) {
            print("Threw exception in encode for: " + arrayToString(array));
            return;
        }
    } catch (e) {
        return;
    }
}

var array = new Uint8Array(5);

function doSteps(numSteps) {
    while (numSteps--) {
        tryArray(array);

        var done = false;
        array[0]++;
        for (var i = 0; i < array.length; ++i) {
            if (array[i])
                break;
            if (i + 1 == array.length) {
                done = true;
                break;
            }
            array[i + 1]++;
        }

        if (done)
            return false;
    }

    return true;
}

doSteps(5000);
