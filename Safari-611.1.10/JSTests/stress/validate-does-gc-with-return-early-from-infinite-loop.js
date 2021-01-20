//@ skip if $architecture != "arm64" and $architecture != "x86-64"
//@ runDefault("--returnEarlyFromInfiniteLoopsForFuzzing=true", "--earlyReturnFromInfiniteLoopsLimit=1000000", "--useConcurrentJIT=false", "--validateDoesGC=true")

if ($vm.useJIT()) {
    while(1){} while(1){}
}
