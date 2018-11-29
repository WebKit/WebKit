// To be skipped until we fix https://bugs.webkit.org/show_bug.cgi?id=192152
//@ skip if $architecture == "arm" and $hostOS == "linux"
function marsaglia(m_z, m_w, n) {
    var result;
    for (var i = 0; i < n; ++i) {
        m_z = (36969 * (m_z & 65535) + (m_z >> 16)) | 0;
        m_w = (18000 * (m_w & 65535) + (m_w >> 16)) | 0;
        result = ((m_z << 16) + m_w) | 0;
    }
    return result;
}

var result = 0;
for (var i = 0; i < 100; ++i)
    result += marsaglia(i, i + 1, 1000000);

if (result != 8216386243)
    throw "Error: bad result: " + result;

