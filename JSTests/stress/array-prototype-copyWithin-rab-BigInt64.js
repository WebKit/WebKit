{
    const rab = new ArrayBuffer(4 * BigInt64Array.BYTES_PER_ELEMENT, { maxByteLength: 8 * BigInt64Array.BYTES_PER_ELEMENT });
    const fixedLength = new BigInt64Array(rab, 0, 4);
    Array.prototype.copyWithin.call(fixedLength, 0, 2);
}
  
