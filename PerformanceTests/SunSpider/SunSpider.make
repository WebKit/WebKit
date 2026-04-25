install:
    xcopy /exclude:$(SRCROOT)\xcopy.excludes "$(SRCROOT)\tests" "$(DSTROOT)\AppleInternal\tests\SunSpider\tests" /e/v/i/h/y
    xcopy /exclude:$(SRCROOT)\xcopy.excludes "$(SRCROOT)\resources" "$(DSTROOT)\AppleInternal\tests\SunSpider\resources" /e/v/i/h/y
    xcopy /exclude:$(SRCROOT)\xcopy.excludes "$(SRCROOT)\sunspider" "$(DSTROOT)\AppleInternal\tests\SunSpider" /v/i/h/y
