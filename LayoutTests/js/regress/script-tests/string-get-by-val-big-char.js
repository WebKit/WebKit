function foo(string) {
    var result1, result2;
    var m_z = 1;
    var m_w = 2;
    for (var i = 0; i < 100000; ++i) {
        result1 = string[0]; // This will be slow, but we're testing if we stay in the DFG.
        for (var j = 0; j < 10; ++j) {
            m_z = (36969 * (m_z & 65535) + (m_z >> 16)) | 0;
            m_w = (18000 * (m_w & 65535) + (m_w >> 16)) | 0;
            result2 = ((m_z << 16) + m_w) | 0;
        }
    }
    return [result1, result2];
}

var lBar = String.fromCharCode(322);

var result = foo(lBar);
if (result[0] != lBar)
    throw "Bad result1: " + result[0];
if (result[1] != 561434430)
    throw "Bad result2: " + result[1];
