//@ skip if ($hostOS != "" || $architecture != "x86_64")

var a = [];
for (var i=0; i<0x04001000; i++) a.push(i+0.1);
a.length = 0x20000000;
a.slice(0x1fffffff,0x20000000);
