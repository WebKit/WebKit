install:
    xcopy "$(SRCROOT)\Source\cmake\*.cmake" "$(DSTROOT)\AppleInternal\tools\cmake" /e/v/i/h/y
    xcopy "$(SRCROOT)\Source\cmake\tools\scripts\*" "$(DSTROOT)\AppleInternal\tools\scripts" /e/v/i/h/y
    xcopy "$(SRCROOT)\Source\cmake\tools\vsprops\*" "$(DSTROOT)\AppleInternal\tools\vsprops" /e/v/i/h/y
