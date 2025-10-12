//@ skip if $memoryLimited
//@ runDefault("--useJIT=0", "--useConcurrentGC=0")
$vm.gc()
let start = $vm.totalGCTime()

// Allocate absolutely nothing except for a WeakBlock
for (let i = 0; i < 1000000; ++i)
{
    $vm.weakCreate()
}

new Array()

if ($vm.totalGCTime() - start == 0)
    throw "GC did not run!"
