# DRT needs to access WebCore and JSC internals, this can't work in non-shared
# mode because it ends up linking them twice (in the DRT executable and the 
# merged libWebKit library).
set(SHARED_CORE 1)
