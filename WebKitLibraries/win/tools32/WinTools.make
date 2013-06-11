install:
    xcopy "$(SRCROOT)\vsprops\*.props" "$(DSTROOT)\AppleInternal\tools32\vsprops" /e/v/i/h/y
    xcopy "$(SRCROOT)\scripts\*" "$(DSTROOT)\AppleInternal\tools32\scripts" /e/v/i/h/y
    xcopy "$(SRCROOT)\vsprops\*.props" "$(DSTROOT)\AppleInternal\tools64\vsprops" /e/v/i/h/y
    xcopy "$(SRCROOT)\scripts\*" "$(DSTROOT)\AppleInternal\tools64\scripts" /e/v/i/h/y