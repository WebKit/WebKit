//@ runDefault("--jitPolicyScale=0", "--gcMaxHeapSize=2000")

// This test happened to catch a case where we failed to zero the space in the butterfly before m_lastOffset when reallocating.

var array = Array(1000);
for (var i = 0; i < 100000; ++i) {
    array[i - array.length] = '';
    array[i ^ array.length] = '';
}
