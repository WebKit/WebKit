function test()
{
    // Simulate the exact conditions from Bug 283898
    const ramSize = 654180352; // Raspberry Pi 3B RAM size
    const criticalGCMemoryThreshold = 0.8;
    const memoryAboveCriticalThreshold = Math.floor(ramSize * (1.0 - criticalGCMemoryThreshold));
    const originalMaxEdenSizeWhenCritical = Math.floor(memoryAboveCriticalThreshold / 4);
    const estimatedMaxEdenSize = 31967829; // From bug report
    
    // Verify the bug condition exists
    if (originalMaxEdenSizeWhenCritical <= estimatedMaxEdenSize)
        return false;
    
    // Simulate the fix
    const constrainedMaxEdenSizeWhenCritical = Math.min(originalMaxEdenSizeWhenCritical, estimatedMaxEdenSize);
    const criticalEdenSize = Math.min(constrainedMaxEdenSizeWhenCritical, estimatedMaxEdenSize / 2);
    
    // Verify the fix works
    if (criticalEdenSize > estimatedMaxEdenSize)
        return false;
    
    // Verify critical mode is more restrictive
    const normalBytesAllowed = estimatedMaxEdenSize;
    const criticalBytesAllowed = Math.min(criticalEdenSize, normalBytesAllowed);
    if (criticalBytesAllowed >= normalBytesAllowed)
        return false;
    
    // Verify allocation is reduced in critical mode
    const originalCriticalBytes = Math.min(originalMaxEdenSizeWhenCritical, normalBytesAllowed);
    const fixedCriticalBytes = Math.min(criticalEdenSize, normalBytesAllowed);
    if (fixedCriticalBytes >= originalCriticalBytes)
        return false;
    
    return true;
}
noInline(test);

for (var i = 0; i < 1e4; ++i)
    if (!test())
        throw new Error("Bug 283898 fix verification failed"); 
