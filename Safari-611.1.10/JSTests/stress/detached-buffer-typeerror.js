function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected ${expected} but got ${actual}`);
}

function shouldThrowTypeError(func) {
    let error;
    try {
        func();
    } catch (e) {
        error = e;
    }

    if (!(error instanceof TypeError))
        throw new Error('Expected TypeError!');
}

function viewLength(view) {
  return view.byteLength;
}
noInline(viewLength);

function viewOffset(view) {
  return view.byteOffset;
}
noInline(viewOffset);

function loadU8(view, offset) {
  return view.getUint8(offset);
}
noInline(loadU8);

function storeU8(view, offset, value) {
  return view.setUint8(offset, value);
}
noInline(storeU8);

const buffer = new ArrayBuffer(1);
const view = new DataView(buffer);

for (let i = 0; i < 1e5; i++) {
  storeU8(view, 0, 0xff);
  shouldBe(loadU8(view, 0), 0xff);
  shouldBe(viewLength(view), 1);
  shouldBe(viewOffset(view), 0);
}

transferArrayBuffer(buffer);

shouldThrowTypeError(() => storeU8(view, 0, 0xff));
shouldThrowTypeError(() => loadU8(view, 0));
shouldThrowTypeError(() => viewLength(view));
shouldThrowTypeError(() => viewOffset(view));
