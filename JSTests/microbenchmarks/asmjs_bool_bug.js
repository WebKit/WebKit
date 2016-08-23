// The strlen function is derived from here:
// http://kripken.github.io/mloc_emscripten_talk/#/20

var MEM8  = new Uint8Array(1024);

// Calculate length of C string:
function strlen(ptr) {
  ptr = ptr|0;
  var curr = 0;
  curr = ptr;
  while (MEM8[curr]|0 != 0) {
    curr = (curr + 1)|0;
  }
  return (curr - ptr)|0;
}

//----- Test driver ----

for (i = 0; i < 1024; i++) {
 MEM8[i] = i%198;
}

MEM8[7] = 0;

var sum = 0
for (i = 0; i < 1000000; i++) {
  sum += strlen(5);
}

if (sum != 2000000)
    throw "Bad result: " + result;
