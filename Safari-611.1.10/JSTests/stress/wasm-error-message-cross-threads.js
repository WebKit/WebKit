if ($vm.isWasmSupported()) {
    var counter = 0;
    obj1 = {[Symbol.asyncIterator]:[-4.0,0x1e3],[Symbol.search]:[-5.0,4294967297,-5.0],"construct":Uint8ClampedArray.__proto__,[Symbol.replace]:WeakSet,[Symbol.species]:()=>null,9007199254740992:[0,64,"length",,"deleteProperty",2147483648,,"symbol",4294967297,"get",128,Symbol.toPrimitive],"Q":6};
    buffer1 = new ArrayBuffer(2048);
    proxy1 = new Proxy(obj1,{get:f,deleteProperty:f,get:encodeURI});
    wasmCode1=new Uint8Array([0x00,0x61,0x73,0x6d,0x01,0x00,0x00,0x00,2,1,3,3,81,97,88,78,66,80,69,103,7,109,111,100,117,108,101,57,1,84,66,98,1,0x70,1,0b11111011,0b10111001,0b1101100,2,7,109,111,100,117,108,101,52,7,109,111,100,117,108,101,56,6,102,105,101,108,100,54,2,0,0b10010111,0b10110010,0b11011101,0b11101110,0b1000100,5,122,1,110,0,1,0x70,1,5,3,9,0b10111001,0b11110101,0b11001010,0b0010101,2,0,0x43,767132387,0x0b,5,2,0,2,0b1110000,0b1000001,0b11101101,0b1000111,0x23,0b10111011,0b11001100,0b10110011,0b0011001,0x0b,5,1,0,0b10100100,0b1101000,3,1,0b11011111,0b11000011,0b0011010,1,0x60,4,0x7c,0x7f,0x7e,0x7e,1,0x7d,3,5,4,1,0b11000101,0b10100011,0b11011110,0b0100010,2,3,8,1,3]);
    function f  (   ) {
    ;
    if (counter++ > 10000)
        return;
    let  o = buffer1 ;
    for (  let i  in (WebAssembly.compile(wasmCode1)).catch(f) ){ ;
    for ( i  of "Symbol.split" ){ ;
    } ;

    Date.prototype.toString.call(proxy1 );; ;
    } ;
    } ;
    ( f ) ( ) ;
}

