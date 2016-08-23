// Test the performance of Int8Array by implementing Adler32.

function adler32(array)
{
    var MOD_ADLER = 65521;
    var a = 1;
    var b = 0;
    var index;
 
    /* Process each byte of the data in order */
    for (index = 0; index < array.length; ++index)
    {
        a = (a + array[index]) % MOD_ADLER;
        b = (b + a) % MOD_ADLER;
    }
 
    return (b << 16) | a;
}

var array = new Int8Array(1000);
for (var i = 0; i < array.length ; ++i)
    array[i] = i;

var result = 0;
for (var i = 0; i < 300; ++i)
    result += adler32(array);

if (result != -63300)
    throw "Bad result: " + result;
