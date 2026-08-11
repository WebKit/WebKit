// Undefined for length argument of typed array constructor should be treated
// the same as if the argument was omitted, meaning the whole buffer is used.
if (new BigInt64Array(new ArrayBuffer(8), 0, undefined).length != 1)
    throw "undefined length should result in the whole buffer being used";
