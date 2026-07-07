use strict;

open(SRC, $ARGV[0]) or die "$!";
open(DST, '>', $ARGV[1]) or die "$!";
while (<SRC>) { 
    # This is a subset of the postprocessing transformations done by Xcode in
    # `Source/WebKit/Scripts/postprocess-header-rule`. Add more as they become
    # necessary for CMake.
    s/<WebKitLegacy/<WebKit/g;
    print DST $_;
}
