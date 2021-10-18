package webkitperl::build::output;

use strict;
use warnings;
use Carp;

BEGIN {
   use Exporter   ();
   our ($VERSION, @ISA, @EXPORT, @EXPORT_OK, %EXPORT_TAGS);
   $VERSION     = 1.00;
   @ISA         = qw(Exporter);
   @EXPORT      = qw(
                    shouldIgnoreLine
                    $platform
                );
   %EXPORT_TAGS = ();
   @EXPORT_OK   = ();
}

our $platform = 'mac';

my $inEntitlements = 0;
my $inDevicePreparationWarnings = 0;

sub shouldShowSubsequentLine {
    my $line = shift;

    return 1 if $line =~ /referenced from:$/;
    return 1 if $line =~ /(note:|error:)/;

    return 0;
}

sub shouldIgnoreLine {
    my ($previousLine, $line) = @_;

    if ($line =~ /^Entitlements:$/) {
        $inEntitlements = 1;
        return 1
    }

    if ($inEntitlements) {
        $inEntitlements = 0 if $line =~ /^}$/;
        return 1
    }

    # iPhone preparation errors always start and end with lines containing 'iPhoneConnect:'.
    if ($inDevicePreparationWarnings) {
        $inDevicePreparationWarnings = 0 if $line =~ /== END: Underlying device preparation warnings ==/;
        return 1
    }

    if ($line =~ /iPhoneConnect:/) {
        $inDevicePreparationWarnings = 1;
        return 1
    }

    return 1 if $line =~ /^\s*$/;
    return 1 if $line =~ /^Command line invocation:/;
    return 1 if $line =~ /^Build settings from command line:/;
    return 1 if $line =~ /^User defaults from command line:/;
    return 1 if $line =~ /^Prepare build/;
    return 1 if $line =~ /^Build system information/;
    return 1 if $line =~ /^note: Planning build/;
    return 1 if $line =~ /^note: Constructing build description/;
    return 1 if $line =~ /^note: Build description (constructed|loaded) in .*/;
    return 1 if $line =~ /^note: Using build description .*/;
    return 1 if $line =~ /^note: Using eager compilation/;
    return 1 if $line =~ /^note: Execution policy exception registration failed and was skipped: Error Domain=NSPOSIXErrorDomain Code=1 "Operation not permitted"/;
    return 1 if $line =~ /^note: detected encoding of input file as Unicode \(.*\)/;
    return 1 if $line =~ /make(\[\d+\])?: Nothing to be done for/;
    return 1 if $line =~ /^JavaScriptCore\/create_hash_table/;
    return 1 if $line =~ /JavaScriptCore.framework\/PrivateHeaders\/create_hash_table/;
    return 1 if $line =~ /^JavaScriptCore\/pcre\/dftables/;
    return 1 if $line =~ /^Creating hashtable for /;
    return 1 if $line =~ /^Wrote output to /;
    return 1 if $line =~ /^UNDOCUMENTED: /;
    return 1 if $line =~ /libtool.*has no symbols/;
    return 1 if $line =~ /^# Lower case all the values, as CSS values are case-insensitive$/;
    return 1 if $line =~ /^if sort /;
    return 1 if $line =~ /set-webkit-configuration/;
    return 1 if $line =~ /^building file list/;
    return 1 if $line =~ /^\.\/$/;
    return 1 if $line =~ /^\S+\.h$/;
    return 1 if $line =~ /^\S+\/$/;
    return 1 if $line =~ /^sent \d+ bytes/;
    return 1 if $line =~ /^total size is/;
    return 1 if $line =~ /One of the two will be used\. Which one is undefined\./;
    return 1 if $line =~ /The Legacy Build System will be removed in a future release/;
    return 1 if $line =~ /^\( (xcodebuild|if) /;
    return 1 if $line =~ /^warning: can't find additional SDK/;
    return 1 if $line =~ /^warning: no umbrella header found for target '.*', module map will not be generated$/;
    return 1 if $line =~ /^warning\: detected internal install, passing entitlements to simulator anyway\./;
    return 1 if $line =~ /may not function in the Simulator because Ad Hoc/;
    return 1 if $line =~ /\/usr\/bin\/clang .*? \> \S+.sb/;
    return 1 if $line =~ / xcodebuild\[[0-9]+:[0-9a-f]+\]\s+DVTAssertions: Warning in .*/;
    return 1 if $line =~ /^(Details|Object|Method|Function|Thread):/;
    return 1 if $line =~ /^Please file a bug at /;
    return 1 if $line =~ /created by an unsupported XCDependencyGraph build$/;
    return 1 if $line =~ /warning: The assignment of '.*' at ".*" uses \$\(inherited\). In the new build system this will inherit from an earlier definition of '.*' in this xcconfig file or its imports; the old build system would discard earlier definitions. This may result in changes to resolved build setting values./;
    return 1 if $line =~ /.* com.apple.actool.compilation-results .*/;
    return 1 if $line =~ /.*\/Assets.car/;
    return 1 if $line =~ /.*\/assetcatalog_generated_info.plist/;
    return 1 if $line =~ /^mount: .+ failed with/;
    return 1 if $line =~ /^Using .+ production environment.$/;
    return 1 if $line =~ /replacing existing signature$/;
    return 1 if $line =~ /^Unlocking '.*\.keychain-db'$/;
    return 1 if $line =~ /^\d+ localizable strings$/;
    return 1 if $line =~ /^\d+ plural rules$/;
    return 1 if $line =~ /^The list of exported symbols did not change.$/;
    return 1 if $line =~ /^ditto: Cannot get the real path for source/;
    return 1 if $line =~ /^Duplicate Entry Was Skipped:/;
    return 1 if $line =~ /^Adding .*?entitlements/;
    return 1 if $line =~ /^Making app bundle launchable/;
    return 1 if $line =~ /^Finished adding entitlements\.$/;
    return 1 if $line =~ /^.* will not be code signed because its settings don't specify a development team.$/;

    if ($platform eq "win") {
        return 1 if $line =~ /^\s*(touch|perl|cat|rm -f|del|python|\/usr\/bin\/g\+\+|gperf|echo|sed|if \[ \-f|WebCore\/generate-export-file) /;
        return 1 if $line =~ /^\s*(if not exist \"|if errorlevel 1)/;
        return 1 if $line =~ /(^\s*|MSB3073:\s+)(set |REM |cmd \/c)/;
        return 1 if $line =~ /^\s*[cC]:\\[pP]rogram [fF]iles.*\\.*\\(CL|midl)\.exe /;
        return 1 if $line =~ /^\s*Processing .*\.(acf|h|idl)\s*$/;
        return 1 if $line =~ /^\s*printf /;
        return 1 if $line =~ /^\s*\/usr\/bin\/bash\s*/;
        return 1 if $line =~ /^\s*offlineasm: Nothing changed/;
        return 1 if $line =~ / \d+ File\(s\) copied/;
        return 1 if $line =~ /^\s*File not found - \*\.h/;
        return 1 if $line =~ /mkdir\s+\"/;
        return 1 if $line =~ /xcopy \/y \/d \"/;
        return 1 if $line =~ /\.obj\"\s*$/;
        return 1 if $line =~ /:\s+(cmd \/c|set)\s+/;
        return 1 if $line =~ /MSB3073:\s+$/;
        return 1 if $line =~ /MSB3073:\s+if not exist/;
        return 1 if $line =~ /which.exe bash/;
    } else {
        return 1 if $line =~ /^(touch|perl|cat|rm -f|python|\/usr\/bin\/g\+\+|\/bin\/ln|gperf|echo|sed|if \[ \-f|WebCore\/generate-export-file|write-file|chmod) /;
        return 1 if $line =~ /^    / && !shouldShowSubsequentLine($previousLine);
        return 1 if $line =~ /^printf /;
        return 1 if $line =~ /^offlineasm: Nothing changed/;
    }
    return 1 if $line =~ /^Showing first/;

    return 0;
}

1;
