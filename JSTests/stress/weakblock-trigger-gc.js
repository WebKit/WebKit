//@ runDefault("--useJIT=0", "--useConcurrentGC=0")
$vm.gc()
let startTime = $vm.totalGCTime()
let startBlocks = $vm.weakBlockCount()

// Allocate absolutely nothing except for WeakImpls. weakCreate() destroys its Weak before it
// returns, so every slot is free again immediately and a handful of WeakBlocks must serve the
// whole loop.
const count = 3000000
for (let i = 0; i < count; ++i)
    $vm.weakCreate()

let grewBy = $vm.weakBlockCount() - startBlocks
if (grewBy > 4)
    throw new Error(`${count} create-and-drop Weaks needed ${grewBy} new WeakBlocks; slots are not being recycled`)

new Array()

if ($vm.totalGCTime() - startTime == 0)
    throw "GC did not run!"
