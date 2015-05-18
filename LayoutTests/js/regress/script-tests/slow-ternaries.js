var asmMod = function test (glob, env, b) {
  'use asm';
  const i8 = new glob.Int8Array(b);
  function f() {
    var i = 0; var r = 0;
    for (i = 0; (i | 0) < 3000000; i = (i + 1) | 0) {
      if ((i8[(i & 0xff) >> 0] | 0) == 1 ? ((i8[((i & 0xff) + 1) >> 0] | 0) == 2 ? ((i8[((i & 0xff) + 2) >> 0] | 0) == 3 ? (i8[((i & 0xff) + 3) >> 0] | 0) == 4 : 0) : 0) : 0)
        r = 1;
    }
    return r | 0;
  }
  return f;
};
var buffer1 = new ArrayBuffer(64*1024);
var asm1 = asmMod(this, {}, buffer1);
for (var i = 0; i < 5; i++) asm1();
