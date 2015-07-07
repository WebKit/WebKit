// Copyright (c) 2004 by Arthur Langereis (arthur_ext at domain xfinitegames, tld com)


// 1 op = 2 assigns, 16 compare/branches, 8 ANDs, (0-8) ADDs, 8 SHLs
// O(n)
function bitsinbyte(b) {
var m = 1, c = 0;
while(m<0x100) {
if(b & m) c++;
m <<= 1;
}
return c;
}

function TimeFunc(func) {
var x, y, t;
for(var x=0; x<350; x++)
for(var y=0; y<256; y++) func(y);
}

TimeFunc(bitsinbyte);
