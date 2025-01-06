globalThis.console ??= {};
console.log ??= print;

var cb;
globalThis.setInterval ||= function setInterval(cb, ms) {
  function iter() {
    setTimeout(iter, ms);
    cb && cb();
  }

  setTimeout(iter, ms);
};

setInterval(() => {
  cb && cb();
}, 16);

function requestAnimationFrame(callback) {
  cb = callback;
}

function copyFromSrcToTgt({
  count,
  size,
  srcBuffer,
  srcOffset,
  srcStride,
  tgtBuffer,
  tgtOffset,
  tgtStride,
}) {
  const source_buffer = new Uint32Array(srcBuffer, srcOffset);
  const target_buffer = new Uint32Array(tgtBuffer, tgtOffset);

  for (let v = 0; v < count; v++) {
    const src_base = (v * srcStride) / 4;
    const tgt_base = (v * tgtStride) / 4;
    for (let k = 0; k < size / 4; k++) {
      target_buffer[tgt_base + k] = source_buffer[src_base + k];
    }
  }
}
let buffersNotAligned = [];
for (let i = 0; i < 500; i++) {
  let typedBuffer = new Float32Array(i % 2 === 0 ? 900 : 810);
  typedBuffer.fill(i + 1);
  buffersNotAligned.push(typedBuffer);
}

let buffersAligned = [];
for (let i = 0; i < 500; i++) {
  let typedBuffer = new Float32Array(900);
  typedBuffer.fill(i + 1);
  buffersAligned.push(typedBuffer);
}
let success = 0;
let fail = 0;
function doCopyOperation(buffers) {
  for (let i = 0; i < buffers.length; i += 2) {
    let buffer1 = buffers[i];
    let buffer2 = buffers[i + 1];
    let dstBuffer = new Float32Array(
      2 * Math.max(buffer1.length, buffer2.length)
    );
    copyFromSrcToTgt({
      count: buffer1.length / 3,
      size: 3 * 4, // byte size of 3 float 32
      srcBuffer: buffer1.buffer,
      tgtBuffer: dstBuffer.buffer,
      srcOffset: 0,
      srcStride: 12,
      tgtOffset: 0,
      tgtStride: 24,
    });
    copyFromSrcToTgt({
      // This is a deliberate mistake so that we can go out of bound for buffer2, which should yield undefined values
      // using buffer2.length instead fixes the problem but the sample has been made to showcase the issue
      // Going out of bound works fine on Win, Linux and Android, was also working fine on MacOS before 14.5 and iOS before 17.5
      // It also works fine on current MacOS and iOS for a period
      count: buffer1.length / 3,
      size: 3 * 4, // byte size of 3 float 32
      srcBuffer: buffer2.buffer,
      tgtBuffer: dstBuffer.buffer,
      srcOffset: 0,
      srcStride: 12,
      tgtOffset: 12,
      tgtStride: 24,
    });
    if (dstBuffer[0] === 0) {
      fail++;
    } else {
      success++;
    }
  }
}
function doOperation() {
  // This fails after a warm up period
  doCopyOperation(buffersNotAligned);
  // This should always work, but also starts failing after a warm up period, if the previous line is commented, this never fails
  doCopyOperation(buffersAligned);
  if (success > 1e3 || fail > 1e3) {
    if (fail) {
        print("FAILED");
        quit(1);
    } else
        quit(0);
    return;
  }
  requestAnimationFrame(doOperation);
}
requestAnimationFrame(doOperation);
