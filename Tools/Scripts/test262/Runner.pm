#!/usr/bin/env perl

# Copyright (C) 2018 Bocoup LLC. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1. Redistributions of source code must retain the above
#    copyright notice, this list of conditions and the following
#    disclaimer.
# 2. Redistributions in binary form must reproduce the above
#    copyright notice, this list of conditions and the following
#    disclaimer in the documentation and/or other materials
#    provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER "AS IS" AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
# OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
# TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
# THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.

use strict;
use warnings;
use 5.8.8;
package Test262::Runner;

use File::Find;
use File::Temp qw(tempfile tempdir);
use File::Spec::Functions qw(abs2rel);
use File::Basename qw(dirname);
use File::Path qw(mkpath);
use Cwd qw(abs_path);
use FindBin;
use Env qw(DYLD_FRAMEWORK_PATH);
use Config;
use Time::HiRes qw(time);
use IO::Handle;
use IO::Select;

my $Bin;
BEGIN {
    $ENV{DBIC_OVERWRITE_HELPER_METHODS_OK} = 1;

    $Bin = $ENV{T262_EXEC_BIN} || $FindBin::Bin;

    unshift @INC, "$Bin";
    unshift @INC, "$Bin/lib";
    unshift @INC, "$Bin/local/lib/perl5";
    unshift @INC, "$Bin/local/lib/perl5/$Config{archname}";
    unshift @INC, "$Bin/..";

    $ENV{LOAD_ROUTES} = 1;
}

use YAML qw(Load LoadFile Dump DumpFile Bless);
use Parallel::ForkManager;
use Getopt::Long qw(GetOptions);

my $webkitdirIsAvailable;
if (eval {require webkitdirs; 1;}) {
    webkitdirs->import(qw(executableProductDir setConfiguration));
    $webkitdirIsAvailable = 1;
}
my $podIsAvailable;
if (eval {require Pod::Usage; 1;}) {
    Pod::Usage->import();
    $podIsAvailable = 1;
}

# Commandline settings
my $maxProcesses;
my @cliTestDirs;
my $verbose;
my $JSC;
my $harnessDir;
my %filterFeatures;
my $ignoreConfig;
my $config;
my %configSkipHash;
my $expect;
my $saveExpectations;
my $failingOnly;
my $latestImport;
my $runningAllTests;
my $timeout;
my $skippedOnly;

my $test262Dir;
my $webkitTest262Dir = abs_path("$Bin/../../../JSTests/test262");
my $expectationsFile = abs_path("$webkitTest262Dir/expectations.yaml");
my $configFile = abs_path("$webkitTest262Dir/config.yaml");

my $resultsDir = $ENV{PWD} . "/test262-results";
my $resultsFile;
my $summaryTxtFile;
my $summaryFile;

my @results;
my @files;

my $tempdir = tempdir();
my ($deffh, $deffile) = getTempFile();

my $startTime = time();

main();

sub processCLI {
    my $help = 0;
    my $release;
    my $ignoreExpectations;
    my @features;
    my $stats;
    my $specifiedResultsFile;
    my $specifiedExpectationsFile;

    # If adding a new commandline argument, you must update the POD
    # documentation at the end of the file.
    GetOptions(
        'j|jsc=s' => \$JSC,
        't|t262=s' => \$test262Dir,
        'o|test-only=s@' => \@cliTestDirs,
        'p|child-processes=i' => \$maxProcesses,
        'h|help' => \$help,
        'release' => \$release,
        'debug' => sub { $release = 0; },
        'v|verbose' => \$verbose,
        'f|features=s@' => \@features,
        'c|config=s' => \$configFile,
        'i|ignore-config' => \$ignoreConfig,
        's|save' => \$saveExpectations,
        'e|expectations=s' => \$specifiedExpectationsFile,
        'x|ignore-expectations' => \$ignoreExpectations,
        'F|failing-files' => \$failingOnly,
        'l|latest-import' => \$latestImport,
        'stats' => \$stats,
        'r|results=s' => \$specifiedResultsFile,
        'timeout=i' => \$timeout,
        'S|skipped-files' => \$skippedOnly,
    );

    if ($help) {
        if ($podIsAvailable) {
            pod2usage(-exitstatus => 0, -verbose => 2, -input => __FILE__);
        } else {
            print "Pod::Usage is not available to print the help options\n";
            exit;
        }
    }

    if ($specifiedResultsFile) {
        if (!$stats && !$failingOnly) {
            print "Waring: supplied results file not used for this command.\n";
        }
        elsif (-e $specifiedResultsFile) {
            $resultsFile = $specifiedResultsFile;
        }
        else {
            die "Error: results file $specifiedResultsFile does not exist.";
        }
    }

    if ($stats || $failingOnly) {
        # If not supplied, try to find the results file in expected directory
        $resultsFile ||= abs_path("$resultsDir/results.yaml");

        if ($failingOnly && ! -e $resultsFile) {
            die "Error: cannot find results file to run failing tests," .
                "please specify with --results.";
        }

        if ($stats) {
            if (! -e $resultsFile) {
                die "Error: cannot find results file to summarize," .
                    " please specify with --results.";
            }
            summarizeResults();
            exit;
        }
    }

    if ($JSC) {
        $JSC = abs_path($JSC);
        # Make sure the path and file jsc exist
        if (! ($JSC && -e $JSC)) {
            die "Error: --jsc path does not exist.";
        }

        if (not defined $DYLD_FRAMEWORK_PATH) {
            $DYLD_FRAMEWORK_PATH = dirname($JSC);
        }
    } else {
        $JSC = getBuildPath($release);
    }

    if ($latestImport) {
        # Does not allow custom $test262Dir
        $test262Dir = '';
    }

    if (! $test262Dir) {
        $test262Dir = $webkitTest262Dir;
    } else {
        $test262Dir = abs_path($test262Dir);
    }

    $harnessDir = "$test262Dir/harness";
    if (! -e $harnessDir) {
        # if the harness directory does not exist in the custom test262 path,
        # then use the webkits harness directory.
        $harnessDir = "$webkitTest262Dir/harness";
    }


    if (! $ignoreConfig) {
        if ($configFile && ! -e $configFile) {
            die "Error: Config file $configFile does not exist!\n" .
                "Run without config file with -i or supply with --config.\n";
        }
        $config = LoadFile($configFile) or die $!;
        if ($config->{skip} && $config->{skip}->{files}) {
            %configSkipHash = map { $_ => 1 } @{$config->{skip}->{files}};
        }
    }

    if ($specifiedExpectationsFile) {
        $expectationsFile = abs_path($specifiedExpectationsFile);
        if (! -e $expectationsFile && ! $ignoreExpectations) {
            print("Warning: Supplied expectations file $expectationsFile does"
                  . " not exist. Running tests without expectation file.\n");
        }
    }

    # If the expectation file doesn't exist and is not specified, run all tests.
    # If we are running only skipped files, then ignore the expectation file.
    if (! $ignoreExpectations && -e $expectationsFile && !$skippedOnly) {
        $expect = LoadFile($expectationsFile) or die $!;
    }

    # If running only the skipped files from the config list
    if ($skippedOnly) {
        if (! -e $configFile) {
            die "Error: Config file $configFile does not exist!\n" .
                "Cannot run skipped tests from config. Supply file with --config.\n";
        }
    }

    if (@features) {
        %filterFeatures = map { $_ => 1 } @features;
    }

    $maxProcesses ||= getProcesses();

    print "\nSettings:\n"
        . "Test262 Dir: " . abs2rel($test262Dir) . "\n"
        . "JSC: " . abs2rel($JSC) . "\n"
        . "Child Processes: $maxProcesses\n";

    print "Test timeout: $timeout\n" if $timeout;
    print "DYLD_FRAMEWORK_PATH: $DYLD_FRAMEWORK_PATH\n" if $DYLD_FRAMEWORK_PATH;
    print "Features to include: " . join(', ', @features) . "\n" if @features;
    print "Paths: " . join(', ', @cliTestDirs) . "\n" if @cliTestDirs;
    print "Config file: " . abs2rel($configFile) . "\n" if $config;
    print "Expectations file: " . abs2rel($expectationsFile) . "\n" if $expect;
    print "Results file: ". abs2rel($resultsFile) . "\n" if $stats || $failingOnly;

    print "Running only the failing files in expectations.yaml\n" if $failingOnly;
    print "Running only the latest imported files\n" if $latestImport;
    print "Running only the skipped files in config.yaml\n" if $skippedOnly;
    print "Verbose mode\n" if $verbose;

    print "---\n\n";
}

sub main {
    processCLI();

    my @defaultHarnessFiles = (
        "$harnessDir/sta.js",
        "$harnessDir/assert.js",
        "$harnessDir/doneprintHandle.js",
        "$Bin/agent.js"
    );

    print $deffh getHarness(\@defaultHarnessFiles);

    if (!@cliTestDirs) {
        # If not commandline test path supplied, use the root directory of all tests.
        push(@cliTestDirs, 'test') if not @cliTestDirs;
        $runningAllTests = 1;
    }

    if ($latestImport) {
        @files = loadImportFile();
    } elsif ($failingOnly) {
        # If we only want to re-run failure, only run tests in results file
        findAllFailing();
    } else {
        # Otherwise, get all files from directory
        foreach my $testsDir (@cliTestDirs) {
            find(
                { wanted => \&wanted, bydepth => 1 },
                qq($test262Dir/$testsDir)
                );
            sub wanted {
                /(?<!_FIXTURE)\.[jJ][sS]$/s && push(@files, $File::Find::name);
            }
        }
    }

    my $pm = Parallel::ForkManager->new($maxProcesses);
    my $select = IO::Select->new();

    my @children;
    my @parents;
    my $activeChildren = 0;

    my @resultsfhs;

    # Open each process
    PROCESSES:
    foreach (0..$maxProcesses-1) {
        my $i = $_;

        # Make temporary files to record results
        my ($fh, $filename) = getTempFile();
        $resultsfhs[$i] = $fh;

        socketpair($children[$i], $parents[$i], 1, 1, 0);
        my $child = $children[$i];
        my $parent = $parents[$i];
        $child->autoflush(1);
        $parent->autoflush(1);

        # seeds each child with a file;

        my $pid = $pm->start;
        if ($pid) { # parent
            $select->add($child);
            # each child starts with a file;
            my $file = shift @files;
            if ($file) {
                chomp $file;
                print $child "$file\n";
                $activeChildren++;
            }

            next PROCESSES;
        }

        # children will start here
        srand(time ^ $$); # Creates a new seed for each fork
        CHILD:
        while (<$parent>) {
            my $file = $_;
            chomp $file;
            if ($file eq 'END') {
                last;
            }

            processFile($file, $resultsfhs[$i]);
            print $parent "signal\n";
        }

        $child->close();
        $pm->finish;
    }

    my @ready;
    FILES:
    while ($activeChildren and @ready = $select->can_read($timeout)) {
        foreach (@ready) {
            my $readyChild = $_;
            my $childMsg = <$readyChild>;
            chomp $childMsg;
            $activeChildren--;
            my $file = shift @files;
            if ($file) {
                chomp $file;
                print $readyChild "$file\n";
                $activeChildren++;
            } elsif (!$activeChildren) {
                last FILES;
            }
        }
    }

    foreach (@children) {
        print $_ "END\n";
    }

    foreach (@parents) {
        $_->close();
    }

    my $count = 0;
    for my $parent (@parents) {
        my $child = $children[$count];
        print $child "END\n";
        $parent->close();
        $count++;
    }

    $pm->wait_all_children;

    # Read results from file into @results and close
    foreach (0..$maxProcesses-1) {
        my $i = $_;
        seek($resultsfhs[$i], 0, 0);
        push @results, LoadFile($resultsfhs[$i]);
        close $resultsfhs[$i];
    }

    close $deffh;

    @results = sort { "$a->{path} . $a->{mode}" cmp "$b->{path} . $b->{mode}" } @results;

    my %failed;
    my $failcount = 0;
    my $newfailcount = 0;
    my $newpasscount = 0;
    my $skipfilecount = 0;
    my $newfailurereport = '';
    my $newpassreport = '';

    # Create expectation file and calculate results
    foreach my $test (@results) {

        my $expectedFailure = 0;
        if ($expect && $expect->{$test->{path}}) {
            $expectedFailure = $expect->{$test->{path}}->{$test->{mode}}
        }

        if ($test->{result} eq 'FAIL') {
            $failcount++;

            # Record this round of failures
            SetFailureForTest(\%failed, $test);

            # If an unexpected failure
            if (!$expectedFailure || ($expectedFailure ne $test->{error})) {
                $newfailcount++;

                if ($verbose) {
                    my $path = $test->{path};
                    my $mode = $test->{mode};
                    # Print full output from JSC
                    my $err = $test->{output};
                    $newfailurereport .= "FAIL $path ($mode)\n"
                        . "Full Output:\n"
                        . "$err\n\n";
                }
            }

        }
        elsif ($test->{result} eq 'PASS') {
            # If this is an newly passing test
            if ($expectedFailure || $skippedOnly) {
                $newpasscount++;

                if ($verbose || $skippedOnly) {
                    my $path = $test->{path};
                    my $mode = $test->{mode};
                    $newpassreport .= "PASS $path ($mode)\n";
                }
            }
        }
        elsif ($test->{result} eq 'SKIP') {
            $skipfilecount++;
        }
    }

    # In verbose mode, the result of every test is printed, so summarize useful results
    if ($verbose && $expect && ($newfailurereport || $newpassreport)) {
        print "\n";
        if ($newfailurereport) {
            print "---------------NEW FAILING TESTS SUMMARY---------------\n\n";
            print "$newfailurereport";
        }
        if ($newpassreport) {
            print "---------------NEW PASSING TESTS SUMMARY---------------\n\n";
            print "$newpassreport\n";
        }
        print "---------------------------------------------------------\n\n";
    }

    # If we are running only skipped tests, report all the new passing tests
    if ($skippedOnly && $newpassreport) {
        print "---------------NEW PASSING TESTS SUMMARY---------------\n";
        print "\n$newpassreport\n";
        print "---------------------------------------------------------\n";
    }

    my $totalRun = scalar @results - $skipfilecount;
    print "\n$totalRun tests run\n";
    print "$skipfilecount test files skipped\n";

    if (!$expect) {
        print "$failcount tests failed\n";
        print "$newpasscount tests newly pass\n" if $skippedOnly;
    } else {
        print "$failcount tests failed in total\n";
        print "$newfailcount tests newly fail\n";
        print "$newpasscount tests newly pass\n";
    }

    if ($saveExpectations) {
        if (!$runningAllTests) {
            UpdateResults($expect, \@results, \%failed);
        }
        DumpFile($expectationsFile, \%failed);
        print "Saved expectation file in: $expectationsFile\n";
    }
    if (! -e $resultsDir) {
        mkpath($resultsDir);
    }

    $resultsFile = abs_path("$resultsDir/results.yaml");

    DumpFile($resultsFile, \@results);
    print "Saved all the results in $resultsFile\n";

    my $styleCss = abs_path("$Bin/report.css");
    qx/cp $styleCss $resultsDir/;
    summarizeResults();
    printHTMLResults(\%failed, $totalRun, $failcount, $newfailcount, $skipfilecount);

    print "See the summaries and results in the $resultsDir.\n\n";

    printf("Done in %.2f seconds!\n", time() - $startTime);

    my $totalfailures = $expect ? $newfailcount : $failcount;
    exit ($totalfailures ? 1 : 0);
}

sub loadImportFile {
    my $importFile = abs_path("$Bin/../../../JSTests/test262/latest-changes-summary.txt");
    die "Error: Import file not found at $importFile.\n" if ! -e $importFile;

    open(my $fh, "<", $importFile) or die $!;

    my @files = grep { $_ =~ /^[AM]\s*test\// } <$fh>;

    return map { $_ =~ s/^\w\s(\w*)/$test262Dir\/$1/; chomp $_; $_ } @files;
}

sub getProcesses {
    my $cores;
    my $uname = qx(which uname >> /dev/null && uname);
    chomp $uname;

    if ($uname eq 'Darwin') {
        # sysctl should be available
        $cores = qx/sysctl -n hw.ncpu/;
    } elsif ($uname eq 'Linux') {
        $cores = qx(which getconf >> /dev/null && getconf _NPROCESSORS_ONLN);
        if (!$cores) {
            $cores = qx(which lscpu >> /dev/null && lscpu -p | egrep -v '^#' | wc -l);
        }
    }

    chomp $cores;

    if (!$cores) {
        $cores = 1;
    }

    if ($cores <= 8) {
        return $cores * 4;
    }
    else {
        return $cores * 2;
    }
}

sub parseError {
    my $error = shift;

    if ($error =~ /^Exception: ([\w\d]+: .*)/m) {
        return $1;
    } else {
        # Unusual error format. Save the first line instead.
        my @errors = split("\n", $error);
        return $errors[0];
    }
}

sub getBuildPath {
    my ($release) = @_;

    my $jsc;

    if ($webkitdirIsAvailable) {
        my $webkit_config = $release ? 'Release' : 'Debug';
        setConfiguration($webkit_config);
        my $jscDir = executableProductDir();

        $jsc = $jscDir . '/jsc';
        $jsc = $jscDir . '/JavaScriptCore.framework/Resources/jsc' if (! -e $jsc);
        $jsc = $jscDir . '/bin/jsc' if (! -e $jsc);

        # Sets the Env DYLD_FRAMEWORK_PATH, abs_path will remove any extra '/' character
        $DYLD_FRAMEWORK_PATH = abs_path(dirname($jsc)) if (-e $jsc);
    }

    if (! $jsc || ! -e $jsc) {
        # If we cannot find jsc using webkitdirs, look in path
        $jsc = qx(which jsc);
        chomp $jsc;

        if (! $jsc ) {
            die("Cannot find jsc, try with --release or specify with --jsc <path>.\n\n");
        }
    }

    return $jsc;
}

sub processFile {
    my ($filename, $resultsfh) = @_;
    my $contents = getContents($filename);
    my $data = parseData($contents, $filename);
    my $resultsdata;

    # Check test against filters in config file
    my $file = abs2rel( $filename, $test262Dir );
    my $skipTest = shouldSkip($file, $data);

    # If we only want to run skipped tests, invert filter
    $skipTest = !$skipTest if $skippedOnly;

    if ($skipTest) {
        if (! $skippedOnly) {
            $resultsdata = processResult($filename, $data, "skip");
            DumpFile($resultsfh, $resultsdata);
        }
        return;
    }
    else {
        my @scenarios = getScenarios(@{ $data->{flags} });

        my $includes = $data->{includes};
        my ($includesfh, $includesfile);

        ($includesfh, $includesfile) = compileTest($includes) if defined $includes;

        foreach my $scenario (@scenarios) {
            my ($result, $execTime) = runTest($includesfile, $filename, $scenario, $data);

            $resultsdata = processResult($filename, $data, $scenario, $result, $execTime);
            DumpFile($resultsfh, $resultsdata);
        }

        close $includesfh if defined $includesfh;
    }
}

sub shouldSkip {
    my ($filename, $data) = @_;

    if (exists $config->{skip}) {
        # Filter by file
        if( $configSkipHash{$filename} ) {
            return 1;
        }

        # Filter by paths
        my @skipPaths;
        @skipPaths = @{ $config->{skip}->{paths} } if defined $config->{skip}->{paths};
        return 1 if (grep {$filename =~ $_} @skipPaths);

        my @skipFeatures;
        @skipFeatures = map {
            # Remove inline comments from the yaml parsed config
            $_ =~ /(\S*)/;
        } @{ $config->{skip}->{features} } if defined $config->{skip}->{features};

        my $skip = 0;
        my $keep = 0;
        my @features = @{ $data->{features} } if $data->{features};
        # Filter by features, loop over file features to for less iterations
        foreach my $feature (@features) {
            $skip = (grep {$_ eq $feature} @skipFeatures) ? 1 : 0;

            # keep the test if the config skips the feature but it was also request
            # through the CLI --features
            return 1 if $skip && !$filterFeatures{$feature};

            $keep = 1 if $filterFeatures{$feature};
        }

        # filter tests that do not contain the --features features
        return 1 if (%filterFeatures and not $keep);
    }

    return 0;
}

sub getScenarios {
    my @flags = @_;
    my @scenarios;
    my $nonStrict = 'default';
    my $strictMode = 'strict mode';

    if (grep $_ eq 'raw', @flags) {
        push @scenarios, 'raw';
    } elsif (grep $_ eq 'noStrict', @flags) {
        push @scenarios, $nonStrict;
    } elsif (grep $_ eq 'onlyStrict', @flags) {
        push @scenarios, $strictMode;
    } elsif (grep $_ eq 'module', @flags) {
        push @scenarios, 'module';
    } else {
        # Add 2 default scenarios
        push @scenarios, $strictMode;
        push @scenarios, $nonStrict;
    };

    return @scenarios;
}

sub compileTest {
    my $includes = shift;
    my ($tfh, $tfname) = getTempFile();

    my @includes = map { "$harnessDir/$_" } @{ $includes };
    my $includesContent = getHarness(\@includes);
    print $tfh $includesContent;

    return ($tfh, $tfname);
}

sub runTest {
    my ($includesfile, $filename, $scenario, $data) = @_;
    $includesfile ||= '';

    my $args = '';

    if ($timeout) {
        $args .= " --watchdog=$timeout ";
    }

    if (exists $data->{negative}) {
        my $type = $data->{negative}->{type};
        $args .=  " --exception=$type ";
    }

    if (exists $data->{flags}) {
        my @flags = $data->{flags};
        if (grep $_ eq 'async', @flags) {
            $args .= ' --test262-async ';
        }
    }

    my $prefixFile = '';

    if ($scenario eq 'module') {
        $prefixFile='--module-file=';
    } elsif ($scenario eq 'strict mode') {
        $prefixFile='--strict-file=';
    }

    # Raw tests should not include the default harness
    my $defaultHarness = '';
    $defaultHarness = $deffile if $scenario ne 'raw';

    my $prefix = $DYLD_FRAMEWORK_PATH ? qq(DYLD_FRAMEWORK_PATH=$DYLD_FRAMEWORK_PATH) : "";
    my $execTimeStart = time();

    my $result = qx($prefix $JSC $args $defaultHarness $includesfile '$prefixFile$filename');
    my $execTime = time() - $execTimeStart;

    chomp $result;

    if ($?) {
        return ($result, $execTime);
    } else {
        return (0, $execTime);
    }
}

sub processResult {
    my ($path, $data, $scenario, $result, $execTime) = @_;

    # Report a relative path
    my $file = abs2rel( $path, $test262Dir );
    my %resultdata;
    $resultdata{path} = $file;
    $resultdata{mode} = $scenario;
    $resultdata{time} = $execTime;

    my $currentfailure = parseError($result) if $result;
    my $expectedfailure = $expect
        && $expect->{$file}
        && $expect->{$file}->{$scenario};

    if ($scenario ne 'skip' && $currentfailure) {

        # We have a new failure if we haven't loaded an expectation file
        # (all fails are new) OR we have loaded an expectation fail and
        # (there is no expected failure OR the failure has changed).
        my $isnewfailure = ! $expect
            || !$expectedfailure || $expectedfailure ne $currentfailure;

        # Print the failure if we haven't loaded an expectation file
        # or the failure is new.
        my $printFailure = (!$expect || $isnewfailure) && !$skippedOnly;

        my $newFail = '';
        $newFail = '! NEW ' if $isnewfailure;
        my $failMsg = '';
        $failMsg = "FAIL $file ($scenario)\n";

        my $featuresList = '';

        if ($verbose && $data->{features}) {
            $featuresList = 'Features: ' . join(', ', @{ $data->{features} }) . "\n";
        }

        print "$newFail$failMsg$featuresList$result\n\n" if ($printFailure || $verbose);

        $resultdata{result} = 'FAIL';
        $resultdata{error} = $currentfailure;
        $resultdata{output} = $result;
    } elsif ($scenario ne 'skip' && !$currentfailure) {
        if ($expectedfailure) {
            print "NEW PASS $file ($scenario)\n";
        } elsif ($verbose) {
            print "PASS $file ($scenario)\n";
        }

        $resultdata{result} = 'PASS';
    } else {
        if ($verbose) {
            print "SKIP $file\n";
        }
        $resultdata{result} = 'SKIP';
    }

    $resultdata{features} = $data->{features} if $data->{features};

    return \%resultdata;
}

sub getTempFile {
    my ($tfh, $tfname) = tempfile(DIR => $tempdir);

    return ($tfh, $tfname);
}

sub getContents {
    my $filename = shift;

    open(my $fh, '<', $filename) or die $!;
    my $contents = join('', <$fh>);
    close $fh;

    return $contents;
}

sub parseData {
    my ($contents, $filename) = @_;

    my $parsed;
    my $found = '';
    if ($contents =~ /\/\*(---[\r\n]+[\S\s]*)[\r\n]+\s*---\*\//m) {
        $found = $1;
    };

    eval {
        $parsed = Load($found);
    };
    if ($@) {
        print "\nError parsing YAML data on file $filename.\n";
        print "$@\n";
    };

    return $parsed;
}

sub getHarness {
    my ($filesref) = @_;

    my $content;
    for (@{$filesref}) {
        my $file = $_;

        open(my $harness_file, '<', $file)
            or die "$!, '$file'";

        $content .= join('', <$harness_file>);

        close $harness_file;
    };

    return $content || '';
}

sub SetFailureForTest {
    my ($failed, $test) = @_;

    if ($failed->{$test->{path}}) {
        $failed->{$test->{path}}->{$test->{mode}} = $test->{error};
    }
    else {
        $failed->{$test->{path}} = {
            $test->{mode} => $test->{error}
        };
    }
}

sub UpdateResults {
    print "Updating results... \n";

    my ($expect, $results, $failed) = @_;

    foreach my $test (@{$results}) {
        delete $expect->{$test->{path}};
    }

    foreach my $path (keys(%{$expect})) {
        foreach my $mode (keys(%{$expect->{$path}})) {
            my $test = {
                path => $path,
                mode => $mode,
                error => $expect->{$path}->{$mode},
                result => 'FAIL'
            };

            SetFailureForTest($failed, $test);
        }
    }
}

sub summarizeResults {
    print "Summarizing results...\n";

    if (not @results) {
        my @rawresults = LoadFile($resultsFile) or die $!;
        @results = @{$rawresults[0]};
    }

    # Create test262-results folder if it does not exits
    if (! -e $resultsDir) {
        mkpath($resultsDir);
    }
    $summaryTxtFile = abs_path("$resultsDir/summary.txt");
    $summaryFile = abs_path("$resultsDir/summary.yaml");
    my $summaryHTMLFile = abs_path("$resultsDir/summary.html");

    my %byfeature;
    my %bypath;

    foreach my $test (@results) {
        my $result = $test->{result};

        if ($test->{features}) {
            foreach my $feature (@{$test->{features}}) {

                if (not exists $byfeature{$feature}) {
                    $byfeature{$feature} = [0, 0, 0, 0];
                }

                if ($result eq 'PASS') {
                    $byfeature{$feature}->[0]++;
                }
                if ($result eq 'FAIL') {
                    $byfeature{$feature}->[1]++;
                }
                if ($result eq 'SKIP') {
                    $byfeature{$feature}->[2]++;
                }

                if ($test->{time}) {
                    $byfeature{$feature}->[3] += $test->{time};
                }
            }
        }
        my @paths = split('/', $test->{path});
        @paths = @paths[ 1 ... scalar @paths-2 ];
        foreach my $i (0..scalar @paths-1) {
            my $partialpath = join("/", @paths[0...$i]);

            if (not exists $bypath{$partialpath}) {
                $bypath{$partialpath} = [0, 0, 0, 0];
            }

            if ($result eq 'PASS') {
                $bypath{$partialpath}->[0]++;
            }
            if ($result eq 'FAIL') {
                $bypath{$partialpath}->[1]++;
            }
            if ($result eq 'SKIP') {
                $bypath{$partialpath}->[2]++;
            }

            if ($test->{time}) {
                $bypath{$partialpath}->[3] += $test->{time};
            }
        }

    }

    open(my $sfh, '>', $summaryTxtFile) or die $!;
    open(my $htmlfh, '>', $summaryHTMLFile) or die $!;

    print $htmlfh qq{<html><head>
        <title>Test262 Summaries</title>
        <link rel="stylesheet" href="report.css">
        </head>
        <body>
        <h1>Test262 Summaries</h1>
        <div class="visit">Visit <a href="index.html">the index</a> for a report of failures.</div>
        <h2>By Features</h2>
        <table class="summary-table">
            <thead>
                <th>Feature</th>
                <th>%</th>
                <th>Total</th>
                <th>Run</th>
                <th>Passed</th>
                <th>Failed</th>
                <th>Skipped</th>
                <th>Exec. time</th>
                <th>Avg. time</th>
            </thead>
            <tbody>};

    print $sfh sprintf("%-6s %-6s %-6s %-6s %-6s %-6s %-7s %-6s %s\n", 'TOTAL', 'RUN', 'PASS-%', 'PASS', 'FAIL', 'SKIP', 'TIME', 'AVG', 'FEATURE');

    foreach my $key (sort keys %byfeature) {
        my $totalFilesRun = $byfeature{$key}->[0] + $byfeature{$key}->[1];
        my $totalFiles = $totalFilesRun + $byfeature{$key}->[2];

        my $iper = ($byfeature{$key}->[0] / $totalFiles) * 100;
        my $per = sprintf("%.0f", $iper) . "%";

        my $time = sprintf("%.1f", $byfeature{$key}->[3]) . "s";
        my $avgTime;

        if ($totalFilesRun) {
            $avgTime = sprintf("%.2f", $byfeature{$key}->[3] / $totalFilesRun) . "s";
        } else {
            $avgTime = "0s";
        }

        print $sfh sprintf("%-6s %-6s %-6s %-6d %-6d %-6d %-7s %-6s %s\n",
                           $totalFiles,
                           $totalFilesRun,
                           $per,
                           $byfeature{$key}->[0],
                           $byfeature{$key}->[1],
                           $byfeature{$key}->[2],
                           $time,
                           $avgTime,
                           $key);

        print $htmlfh qq{
            <tr class="per-$iper">
                <td>$key</td>
                <td>$per</td>
                <td>$totalFiles</td>
                <td>$totalFilesRun</td>
                <td>$byfeature{$key}->[0]</td>
                <td>$byfeature{$key}->[1]</td>
                <td>$byfeature{$key}->[2]</td>
                <td>$time</td>
                <td>$avgTime</td>
            </tr>};
    }

    print $htmlfh qq{</tbody></table>
        <h2>By Path</h2>
        <table class="summary-table">
            <thead>
                <th>Folder</th>
                <th>%</th>
                <th>Total</th>
                <th>Run</th>
                <th>Passed</th>
                <th>Failed</th>
                <th>Skipped</th>
                <th>Exec. time</th>
                <th>Avg. time</th>
            </thead>
            <tbody>};

    print $sfh sprintf("\n\n%-6s %-6s %-6s %-6s %-6s %-6s %-7s %-6s %s\n", 'TOTAL', 'RUN', 'PASS-%', 'PASS', 'FAIL', 'SKIP', 'TIME', 'AVG', 'FOLDER'); 
    foreach my $key (sort keys %bypath) {
        my $totalFilesRun = $bypath{$key}->[0] + $bypath{$key}->[1];
        my $totalFiles = $totalFilesRun + $bypath{$key}->[2];

        my $iper = ($bypath{$key}->[0] / $totalFiles) * 100;
        my $per = sprintf("%.0f", $iper) . "%";

        my $time = sprintf("%.1f", $bypath{$key}->[3]) . "s";
        my $avgTime;

        if ($totalFilesRun) {
            $avgTime = sprintf("%.2f", $bypath{$key}->[3] / $totalFilesRun) . "s";
        } else {
            $avgTime = "0s";
        }

        print $sfh sprintf("%-6s %-6s %-6s %-6d %-6d %-6d %-7s %-6s %s\n",
                           $totalFiles,
                           $totalFilesRun,
                           $per,
                           $bypath{$key}->[0],
                           $bypath{$key}->[1],
                           $bypath{$key}->[2],
                           $time,
                           $avgTime,
                           $key);

        print $htmlfh qq{
            <tr class="per-$iper">
                <td>$key</td>
                <td>$per</td>
                <td>$totalFiles</td>
                <td>$totalFilesRun</td>
                <td>$bypath{$key}->[0]</td>
                <td>$bypath{$key}->[1]</td>
                <td>$bypath{$key}->[2]</td>
                <td>$time</td>
                <td>$avgTime</td>
            </tr>};
    }

    print $htmlfh qq{</tbody></table>
        <div class="visit">Visit <a href="index.html">the index</a> for a report of failures.</div>
        </body></html>};

    close($sfh);
    close($htmlfh);

    my %resultsyaml = (
        byFolder => \%bypath,
        byFeature => \%byfeature,
    );

    DumpFile($summaryFile, \%resultsyaml);
}

sub findAllFailing {
    my @allresults = LoadFile($resultsFile) or die $!;
     @allresults = @{$allresults[0]};

    my %filedictionary;
    foreach my $test (@allresults) {
        if ($test->{result} eq 'FAIL') {
            $filedictionary{$test->{path}} = 1;
        }
    }

    @files = map { qq($test262Dir/$_) } keys %filedictionary;
}

sub printHTMLResults {
    my %failed = %{shift()};
    my ($total, $failcount, $newfailcount, $skipcount) = @_;

    # Create test262-results folder if it does not exits
    if (! -e $resultsDir) {
        mkpath($resultsDir);
    }

    my $indexHTML = abs_path("$resultsDir/index.html");
    open(my $htmlfh, '>', $indexHTML) or die $!;

    print $htmlfh qq{<html><head>
        <title>Test262 Results</title>
        <link rel="stylesheet" href="report.css">
        </head>
        <body>
        <h1>Test262 Results</h1>
        <div class="visit">Visit <a href="summary.html">the summary</a> for statistics.</div>};

    print $htmlfh qq{<h2>Stats</h2><ul>};

    {
        my $failedFiles = scalar (keys %failed);
        my $totalPlus = $total + $skipcount;
        print $htmlfh qq{
            <li>$total test files run from $totalPlus files, $skipcount skipped test files</li>
            <li>$failcount failures from $failedFiles distinct files, $newfailcount new failures</li>
        };
    }

    print $htmlfh qq{</ul><h2>Failures</h2><ul>};

    foreach my $path (sort keys %failed) {
        my $scenarios = $failed{$path};
        print $htmlfh qq{<li class="list-item">
            <label for="$path" class="expander-control">$path</label>
            <input type="checkbox" id="$path" class="expander">
            <ul class="expand">};
        while (my ($scenario, $value) = each %{$scenarios}) {
            print $htmlfh qq{<li>$scenario: $value</li>};
        }
        print $htmlfh qq{</ul></li>}
    }

    print $htmlfh qq{</ul>
    <div class="visit">Visit <a href="summary.html">the summary</a> for statistics.</div>
    </body></html>};

    close $htmlfh;
}

__END__

=head1 DESCRIPTION

This program will run all Test262 tests. If you edit, make sure your changes are Perl 5.8.8 compatible.

=head1 SYNOPSIS

Run using native Perl:

=over 8

test262-runner -j $jsc-dir

=back

Run using carton (recommended for testing on Perl 5.8.8):

=over 8

carton exec 'test262-runner -j $jsc-dir'

=back

=head1 OPTIONS

=over 8

=item B<--help, -h>

Print a brief help message and exits.

=item B<--child-processes, -p>

Specify the number of child processes.

=item B<--t262, -t>

Specify root test262 directory.

=item B<--jsc, -j>

Specify JSC location. If not provided, script will attempt to look up JSC.

=item B<--release>

Use the Release build of JSC. Can only use if --jsc <path> is not provided. The Debug build of JSC is used by default.

=item B<--debug>

Use the Debug build of JSC. Can only use if --jsc <path> is not provided. Negates the --release option.

=item B<--verbose, -v>

Verbose output for test results. Includes error message for test.

=item B<--config, -c>

Specify a config file. If not provided, script will load local JSTests/test262/config.yaml

=item B<--ignore-config, -i>

Ignores config file if supplied or findable in directory. Will still filter based on commandline arguments.

=item B<--features, -f>

Filter test on list of features (only runs tests in feature list).

=item B<--test-only, -o>

Specify one or more specific test262 directory of test to run, relative to the root test262 directory. For example, --test-only 'test/built-ins/Number/prototype'

=item B<--save, -s>

Overwrites the test262-expectations.yaml file with the current list of test262 files and test results.

=item B<--expectations, -e>

Specify a expectations file for loading and saving.  If not provided, script will load and save to JSTests/test262/expectations.yaml.

=item B<--ignore-expectations, -x>

Ignores the test262-expectations.yaml file and outputs all failures, instead of only unexpected failures.

=item B<--failing-files, -F>

Runs all test files that failed in a given results file (specifc with --results). This option will run the rests in both strict and non-strict modes, even if the test only fails in one of the two modes.

=item B<--latest-import, -l>

Runs the test files listed in the last import (./JSTests/test262/latest-changes-summary.txt).

=item B<--skipped-files, -S>

Runs all test files that are skipped according to the config.yaml file.

=item B<--stats>

Calculate conformance statistics from results/results.yaml file or a supplied results file (--results). Saves results in results/summary.txt and results/summary.yaml.

=item B<--results, -r>

Specifies a results file for the --stats or --failing-files options.

=item B<--timeout>

Specifies a timeout execution in ms for each test. Defers the value to the jsc --watchdog argument. Disabled by default.

=back

=cut
