d_key equ 0
cachedata: DQ 0
dtable:
cmp al, [esi + (dtable-cachedata) + ebx*4 + d_key]
