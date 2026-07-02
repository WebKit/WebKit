// Undefined for length argument of typed array constructor should be treated
// the same as if the argument was omitted, meaning the whole buffer is used.
if (new Uint8Array(new ArrayBuffer(3), 0, undefined).length != 3)
    throw "undefined length should result in the whole buffer being used";
