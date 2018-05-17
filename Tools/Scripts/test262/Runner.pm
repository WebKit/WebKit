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
my $max_process;
my @cliTestDirs;
my $verbose;
my $JSC;
my $test262Dir;
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

my $expectationsFile = abs_path("$Bin/../../../JSTests/test262/expectations.yaml");
my $configFile = abs_path("$Bin/../../../JSTests/test262/config.yaml");

my $resultsDir = $ENV{PWD} . "/test262-results";
mkpath($resultsDir);

my $resultsFile = abs_path("$resultsDir/results.yaml");
my $summaryTxtFile = abs_path("$resultsDir/summary.txt");
my $summaryFile = abs_path("$resultsDir/summary.yaml");

my @results;
my @files;

my $tempdir = tempdir();
my ($deffh, $deffile) = getTempFile();

my @default_harnesses;

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
        'p|child-processes=i' => \$max_process,
        'h|help' => \$help,
        'release' => \$release,
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

    if ($stats) {
        if (! -e $resultsFile) {
            die "Error: cannot find results file to summarize," .
                "please specify with --results.";
        }
        summarizeResults();
        exit;
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
        $test262Dir = abs_path("$Bin/../../../JSTests/test262");
    } else {
        $test262Dir = abs_path($test262Dir);
    }
    $harnessDir = "$test262Dir/harness";

    if (! $ignoreConfig) {
        if ($configFile && ! -e $configFile) {
            die "Error: Config file $configFile does not exist!\n" .
                "Run without config file with -i or supply with --config.\n"
        }
        $config = LoadFile($configFile) or die $!;
        if ($config->{skip} && $config->{skip}->{files}) {
            %configSkipHash = map { $_ => 1 } @{$config->{skip}->{files}};
        }
    }

    if ( $failingOnly && ! -e $resultsFile ) {
        die "Error: cannot find results file to run failing tests," .
            " please specify with --results.";
    }

    if ($specifiedExpectationsFile) {
        $expectationsFile = abs_path($specifiedExpectationsFile);
        if (! -e $expectationsFile && ! $ignoreExpectations) {
            print("Warning: Supplied expectations file $expectationsFile does"
                  . " not exist. Running tests without expectation file.\n");
        }
    }

    # If the expectation file doesn't exist and is not specified, run all tests.
    if (! $ignoreExpectations && -e $expectationsFile) {
        $expect = LoadFile($expectationsFile) or die $!;
    }

    if (@features) {
        %filterFeatures = map { $_ => 1 } @features;
    }

    $max_process ||= getProcesses();

    print "\n-------------------------Settings------------------------\n"
        . "Test262 Dir: $test262Dir\n"
        . "JSC: $JSC\n"
        . "Child Processes: $max_process\n";

    print "Test timeout: $timeout\n" if $timeout;
    print "DYLD_FRAMEWORK_PATH: $DYLD_FRAMEWORK_PATH\n" if $DYLD_FRAMEWORK_PATH;
    print "Features to include: " . join(', ', @features) . "\n" if @features;
    print "Paths: " . join(', ', @cliTestDirs) . "\n" if @cliTestDirs;
    print "Config file: $configFile\n" if $config;
    print "Expectations file: $expectationsFile\n" if $expect;
    print "Results file: $resultsFile\n" if $stats || $failingOnly;

    print "Running only the latest imported files\n" if $latestImport;

    print "Verbose mode\n" if $verbose;

    print "--------------------------------------------------------\n\n";
}

sub main {
    processCLI();

    @default_harnesses = (
        "$harnessDir/sta.js",
        "$harnessDir/assert.js",
        "$harnessDir/doneprintHandle.js",
        "$Bin/agent.js"
    );
    print $deffh getHarness(<@default_harnesses>);

    # If not commandline test path supplied, use the root directory of all tests.
    push(@cliTestDirs, 'test') if not @cliTestDirs;

    if ($latestImport) {
        @files = loadImportFile();
    } elsif ($failingOnly) {
        # If we only want to re-run failure, only run tests in results file
        findAllFailing();
    } else {
        $runningAllTests = 1;
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

    # If we are processing many files, fork process
    if (scalar @files > $max_process * 5) {

        # Make temporary files to record results
        my @resultsfhs;
        for (my $i = 0; $i <= $max_process-1; $i++) {
            my ($fh, $filename) = getTempFile();
            $resultsfhs[$i] = $fh;
        }

        my $pm = Parallel::ForkManager->new($max_process);
        my $filesperprocess = int(scalar @files / $max_process);

        FILES:
        for (my $i = 0; $i <= $max_process-1; $i++) {
            $pm->start and next FILES; # do the fork
            srand(time ^ $$); # Creates a new seed for each fork

            my $first = $filesperprocess * $i;
            my $last = $i == $max_process-1 ? scalar @files : $filesperprocess * ($i+1);

            for (my $j = $first; $j < $last; $j++) {
                processFile($files[$j], $resultsfhs[$i]);
            };

            $pm->finish; # do the exit in the child process
        };

        $pm->wait_all_children;

        # Read results from file into @results and close
        for (my $i = 0; $i <= $max_process-1; $i++) {
            seek($resultsfhs[$i], 0, 0);
            push @results, LoadFile($resultsfhs[$i]);
            close $resultsfhs[$i];
        }
    }
    # Otherwising, running sequentially is fine
    else {
        my ($resfh, $resfilename) = getTempFile();
        foreach my $file (@files) {
            processFile($file, $resfh);
        };
        seek($resfh, 0, 0);
        @results = LoadFile($resfh);
        close $resfh;
    }

    close $deffh;

    @results = sort { "$a->{path} . $a->{mode}" cmp "$b->{path} . $b->{mode}" } @results;

    my %failed;
    my $failcount = 0;
    my $newfailcount = 0;
    my $newpasscount = 0;
    my $skipfilecount = 0;

    # Create expectation file and calculate results
    foreach my $test (@results) {

        my $expectedFailure = 0;
        if ($expect && $expect->{$test->{path}}) {
            $expectedFailure = $expect->{$test->{path}}->{$test->{mode}}
        }

        if ($test->{result} eq 'FAIL') {
            $failcount++;

            # Record this round of failures
            if ( $failed{$test->{path}} ) {
                $failed{$test->{path}}->{$test->{mode}} =  $test->{error};
            }
            else {
                $failed{$test->{path}} = {
                    $test->{mode} => $test->{error}
                };
            }

            # If an unexpected failure
            $newfailcount++ if !$expectedFailure || ($expectedFailure ne $test->{error});

        }
        elsif ($test->{result} eq 'PASS') {
            # If this is an newly passing test
            $newpasscount++ if $expectedFailure;
        }
        elsif ($test->{result} eq 'SKIP') {
            $skipfilecount++;
        }
    }

    if ($saveExpectations) {
        DumpFile($expectationsFile, \%failed);
        print "\nSaved results in: $expectationsFile\n";
    } else {
        print "\nRun with --save to save a new expectations file\n";
    }

    if ($runningAllTests) {
        DumpFile($resultsFile, \@results);
        print "Saved all the results in $resultsFile\n";
        summarizeResults();
    }

    my $total = scalar @results - $skipfilecount;
    print "\n" . $total . " tests ran\n";

    if ( !$expect ) {
        print $failcount . " tests failed\n";
    } else {
        print $failcount . " tests failed in total\n";
        print $newfailcount . " tests newly fail\n";
        print $newpasscount . " tests newly pass\n";
    }

    print $skipfilecount . " test files skipped\n";

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
        my $config = $release ? 'Release' : 'Debug';
        setConfiguration($config);
        my $jscDir = executableProductDir();

        $jsc = $jscDir . '/jsc';
        $jsc = $jscDir . '/JavaScriptCore.framework/Resources/jsc' if (! -e $jsc);
        $jsc = $jscDir . '/bin/jsc' if (! -e $jsc);

        # Sets the Env DYLD_FRAMEWORK_PATH
        $DYLD_FRAMEWORK_PATH = dirname($jsc) if (-e $jsc);
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
    if (shouldSkip($file, $data)) {
        $resultsdata = processResult($filename, $data, "skip");
        DumpFile($resultsfh, $resultsdata);
        return;
    }

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
        @skipFeatures = @{ $config->{skip}->{features} } if defined $config->{skip}->{features};

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

    my $includesContent = getHarness(map { "$harnessDir/$_" } @{ $includes });
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

        # We have a new failure if we have loaded an expectation file
        # AND (there is no expected failure OR the failure has changed).
        my $isnewfailure = $expect
            && (!$expectedfailure || $expectedfailure ne $currentfailure);

        # Print the failure if we haven't loaded an expectation file
        # or the failure is new.
        my $printFailure = !$expect || $isnewfailure;

        my $newFail = '';
        $newFail = '! NEW ' if $isnewfailure;
        my $failMsg = '';
        $failMsg = "FAIL $file ($scenario)\n" if ($printFailure or $verbose);

        my $suffixMsg = '';

        if ($verbose) {
            my $featuresList = '';
            $featuresList = "\nFeatures: " . join(', ', @{ $data->{features} }) if $data->{features};
            $suffixMsg = "$result$featuresList\n\n";
        }

        print "$newFail$failMsg$suffixMsg";

        $resultdata{result} = 'FAIL';
        $resultdata{error} = $currentfailure;
    } elsif ($scenario ne 'skip' && !$currentfailure) {
        if ($expectedfailure) {
            print "NEW PASS $file ($scenario)\n";
            print "\n" if $verbose;
        }

        $resultdata{result} = 'PASS';
    } else {
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
    if ($contents =~ /\/\*(---[\r\n]+[\S\s]*)[\r\n]+---\*\//m) {
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
    my @files = @_;
    my $content;
    for (@files) {
        my $file = $_;

        open(my $harness_file, '<', $file)
            or die "$!, '$file'";

        $content .= join('', <$harness_file>);

        close $harness_file;
    };

    return $content;
}

sub summarizeResults {
    print "Summarizing results...\n";

    if (not @results) {
        my @rawresults = LoadFile($resultsFile) or die $!;
        @results = @{$rawresults[0]};
    }
    my %byfeature;
    my %bypath;

    foreach my $test (@results) {
        my $result = $test->{result};

        if ($test->{features}) {
            foreach my $feature (@{$test->{features}}) {

                if (not exists $byfeature{$feature}) {
                    $byfeature{$feature} = [0, 0, 0]
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
            }
        }
        my @paths = split('/', $test->{path});
        @paths = @paths[ 1 ... scalar @paths-2 ];
        foreach my $i (0..scalar @paths-1) {
            my $partialpath = join("/", @paths[0...$i]);

            if (not exists $bypath{$partialpath}) {
                $bypath{$partialpath} = [0, 0, 0];
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
        }

    }

    open(my $sfh, '>', $summaryTxtFile) or die $!;

    print $sfh sprintf("%-6s %-6s %-6s %-6s %s\n", '%PASS', 'PASS', 'FAIL', 'SKIP', 'FOLDER');
    foreach my $key (sort keys %bypath) {
        my $per = ($bypath{$key}->[0] / (
            $bypath{$key}->[0]
            + $bypath{$key}->[1]
            + $bypath{$key}->[2])) * 100;

        $per = sprintf("%.0f", $per) . "%";

        print $sfh sprintf("%-6s %-6d %-6d %-6d %s \n", $per,
                           $bypath{$key}->[0],
                           $bypath{$key}->[1],
                           $bypath{$key}->[2], $key,);
    }

    print $sfh "\n\n";
    print $sfh sprintf("%-6s %-6s %-6s %-6s %s\n", '%PASS', 'PASS', 'FAIL', 'SKIP', 'FEATURE');

    foreach my $key (sort keys %byfeature) {
        my $per = ($byfeature{$key}->[0] / (
            $byfeature{$key}->[0]
            + $byfeature{$key}->[1]
            + $byfeature{$key}->[2])) * 100;

        $per = sprintf("%.0f", $per) . "%";

        print $sfh sprintf("%-6s %-6d %-6d %-6d %s\n", $per,
                           $byfeature{$key}->[0],
                           $byfeature{$key}->[1],
                           $byfeature{$key}->[2], $key);
    }

    close($sfh);

    my %resultsyaml = (
        byFolder => \%bypath,
        byFeature => \%byfeature,
    );

    DumpFile($summaryFile, \%resultsyaml);

    print "See summarized results in $summaryTxtFile\n";
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

Specify number of child processes.

=item B<--t262, -t>

Specify root test262 directory.

=item B<--jsc, -j>

Specify JSC location. If not provided, script will attempt to look up JSC.

=item B<--release>

Use the Release build of JSC. Can only use if --jsc <path> is not provided. The Debug build of JSC is used by default.

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

=item B<--stats>

Calculate conformance statistics from results/results.yaml file or a supplied results file (--results). Saves results in results/summary.txt and results/summary.yaml.

=item B<--results, -r>

Specifies a results file for the --stats or --failing-files options.

=item B<--timeout>

Specifies a timeout execution in ms for each test. Defers the value to the jsc --watchdog argument. Disabled by default.

=back

=cut
