//@ runDefault("--thresholdForJITAfterWarmUp=10", "--thresholdForFTLOptimizeAfterWarmUp=20", "--useConcurrentJIT=0", "--useConcurrentGC=0")

function var3() { 
    const var15 = { a : 10 } ;
    const var7 = { } ;
    var7.d = var15 ;
    const var10 = [ 1 ] ;
    var10 . toString = [ ] ;
    if ( ! var10 ) { return 0 ; }
    for ( var i =0; i < 50; i++ ) {
        var var2 = "aa";
        var2.x = function ( ) {
            var3.c = new Uint32Array ( 1 ) ;
            var15.b = new Uint32Array ( 1 ) ;
        }
        var7 [ 0 ] = { } ;
    } 
} 

for(var i = 0; i < 2000; i++) {
    var3();
}
