CompileFlags:
    # clangd uses clang to parse code files, and although clang strives to be
    # flag-compatible with GCC, it often lags on adding new flags.
    # Therefore, let's remove GCC flags that are not important for code
    # completion that are not supported in clangd-16.
    Remove: [
        -fcoroutines,
        -Wno-stringop-overread,
        -Wno-stringop-overflow,
        -Wno-maybe-uninitialized,
    ]
    Add: [
        "-ferror-limit=0",  # https://github.com/WebKit/WebKit/pull/30784#issuecomment-2257495415
    ]
---
If:
    PathMatch: [.*/UnifiedSource-.*]
Index:
    Background: Skip
---
If:
    PathMatch: [.*\.h]
    PathExclude: [.*/ThirdParty/.*]
CompileFlags:
    Add: [
        $header_file_platform_specific_flags
        --include=config.h,
        -std=c++2a,
    ]
---
If:
    PathMatch: [.*\.mm]
CompileFlags:
    Add: [-xobjective-c++, --include=config.h, -std=c++2a]
---
If:
    PathMatch: [.*\.m]
CompileFlags:
    Add: [-xobjective-c, --include=config.h]

