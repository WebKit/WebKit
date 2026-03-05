//@ skip if $memoryLimited
const crash_point= 0x41414141;
const loop_start = crash_point&0xffffff00;
const loop_end   = crash_point+1;
for (let i=0; i<5000; i++){
    const get_byte_length = arr => arr.byteLength;
    const ab = new ArrayBuffer(99, {maxByteLength: 99});
    const dv = new DataView(ab, 1, 44);
    for (let counter = loop_start; counter < loop_end; counter++) {
        if (counter == loop_end-2) {
          ab.resize(1);
          get_byte_length(1);
        }else{
          try {
            get_byte_length(dv);
          } catch (e) {}
        }
    }
}
