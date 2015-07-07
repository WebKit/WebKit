// Test the performance of integer multiplication by implementing a 16-bit
// string hash.

function stringHash(string)
{
    // source: http://en.wikipedia.org/wiki/Java_hashCode()#Sample_implementations_of_the_java.lang.String_algorithm
    var h = 0;
    var len = string.length;
    for (var index = 0; index < len; index++) {
        h = (((31 * h) | 0) + string.charCodeAt(index)) | 0;
    }
    return h;
}

for (var i = 0; i < 10000; ++i) {
    var result = stringHash("The spirit is willing but the flesh is weak.");
    if (result != -723065856)
        throw "Bad result: " + result;
}

