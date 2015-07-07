// Test the performance of Int16Array by implementing a 16-bit string hash, and
// test the performance of used-as-int multiplications.

function stringHash(array)
{
    // source: http://en.wikipedia.org/wiki/Java_hashCode()#Sample_implementations_of_the_java.lang.String_algorithm
    var h = 0;
    var len = array.length;
    for (var index = 0; index < len; index++) {
        h = (((31 * h) | 0) + array[index]) | 0;
    }
    return h;
}

var array = new Int16Array(1000);
for (var i = 0; i < array.length; ++i)
    array[i] = i;

var result = 0;
for (var i = 0; i < 300; ++i)
    result += stringHash(array);

if (result != 168792418800)
    throw "Bad result" + result;
