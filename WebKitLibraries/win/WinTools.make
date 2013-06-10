install:
    xcopy "$(SRCROOT)\tools32\vsprops\*.props" "$(DSTROOT)\AppleInternal\tools32\vsprops" /e/v/i/h/y
    xcopy "$(SRCROOT)\tools32\scripts\*" "$(DSTROOT)\AppleInternal\tools32\scripts" /e/v/i/h/y
    xcopy "$(SRCROOT)\tools64\vsprops\*.props" "$(DSTROOT)\AppleInternal\tools64\vsprops" /e/v/i/h/y
    xcopy "$(SRCROOT)\tools64\scripts\*" "$(DSTROOT)\AppleInternal\tools64\scripts" /e/v/i/h/y