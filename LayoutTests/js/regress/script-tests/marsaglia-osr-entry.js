// This is a great test for checking if OSR entry works at all. It's great for debugging
// fundamental OSR entry bugs.

function marsaglia(m_z, m_w, n) {
    var result;
    for (var i = 0; i < n; ++i) {
        m_z = (36969 * (m_z & 65535) + (m_z >> 16)) | 0;
        m_w = (18000 * (m_w & 65535) + (m_w >> 16)) | 0;
        result = ((m_z << 16) + m_w) | 0;
    }
    return result;
}

var result = marsaglia(5, 7, 10000000);

if (result != -1047364056)
    throw "Error: bad result: " + result;

