const rab = new ArrayBuffer(50, {
  "maxByteLength": 100
});
const ta = new Int8Array(rab);
const evil = {};

evil.valueOf = function () {
  rab.resize(0);
  return 5;
};

ta.lastIndexOf(1, evil);
