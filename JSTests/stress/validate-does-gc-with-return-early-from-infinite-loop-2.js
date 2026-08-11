//@ runDefault("--returnEarlyFromInfiniteLoopsForFuzzing=true", "--earlyReturnFromInfiniteLoopsLimit=100000", "--useConcurrentJIT=false", "--useFTLJIT=false", "--validateDoesGC=true")

if ($vm.useJIT()) {
    while(1){}
}
