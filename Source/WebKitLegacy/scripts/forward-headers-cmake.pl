use strict;

# An optional leading --webkit rewrites `<WebKitLegacy/X.h>` imports to
# `<WebKit/X.h>`, for the copies staged inside WebKit.framework, which
# re-exports the legacy ObjC API.
my $webkit = 0;
if (@ARGV && $ARGV[0] eq "--webkit") {
    shift @ARGV;
    $webkit = 1;
}

open(SRC, $ARGV[0]) or die "$!";
open(DST, '>', $ARGV[1]) or die "$!";
while (<SRC>) {
    # This is a subset of the postprocessing transformations done by Xcode in
    # `Source/WebKitLegacy/Scripts/migrate-header-rule` and
    # `Source/WebKit/Scripts/postprocess-header-rule`. Add more as they become
    # necessary for CMake.
    s/<WebCore\//<WebKitLegacy\//g;
    s/(^ *)WEBCORE_EXPORT /$1/;
    s/<WebKitLegacy/<WebKit/g if $webkit;
    print DST $_;
}
