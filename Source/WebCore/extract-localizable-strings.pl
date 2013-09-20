#!/usr/bin/perl -w

# Copyright (C) 2006, 2007, 2009, 2010, 2013 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer. 
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution. 
# 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
#     its contributors may be used to endorse or promote products derived
#     from this software without specific prior written permission. 
#
# THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# This script is like the genstrings tool (minus most of the options) with these differences.
#
#    1) It uses the names UI_STRING and UI_STRING_WITH_KEY for the macros, rather than the macros
#       from NSBundle.h, and doesn't support tables (although they would be easy to add).
#    2) It supports UTF-8 in key strings (and hence uses "" strings rather than @"" strings;
#       @"" strings only reliably support ASCII since they are decoded based on the system encoding
#       at runtime, so give different results on US and Japanese systems for example).
#    3) It looks for strings that are not marked for localization, using both macro names that are
#       known to be used for debugging in Intrigue source code and an exceptions file.
#    4) It finds the files to work on rather than taking them as parameters, and also uses a
#       hardcoded location for both the output file and the exceptions file.
#       It would have been nice to use the project to find the source files, but it's too hard to
#       locate source files after parsing a .pbxproj file.

# The exceptions file has a list of strings in quotes, filenames, and filename/string pairs separated by :.

use strict;
use Getopt::Long;
no warnings 'deprecated';

sub UnescapeHexSequence($);

my %isDebugMacro = ( ASSERT_WITH_MESSAGE => 1, LOG_ERROR => 1, ERROR => 1, NSURL_ERROR => 1, FATAL => 1, LOG => 1, LOG_WARNING => 1, UI_STRING_LOCALIZE_LATER => 1, UI_STRING_LOCALIZE_LATER_KEY => 1, LPCTSTR_UI_STRING_LOCALIZE_LATER => 1, UNLOCALIZED_STRING => 1, UNLOCALIZED_LPCTSTR => 1, dprintf => 1, NSException => 1, NSLog => 1, printf => 1 );

my $verify;
my $exceptionsFile;
my @directoriesToSkip = ();

my %options = (
    'verify' => \$verify,
    'exceptions=s' => \$exceptionsFile,
    'skip=s' => \@directoriesToSkip,
);

GetOptions(%options);

@ARGV >= 2 or die "Usage: extract-localizable-strings [--verify] [--exceptions <exceptions file>] <file to update> [--skip directory | directory]...\nDid you mean to run update-webkit-localizable-strings instead?\n";

-f $exceptionsFile or die "Couldn't find exceptions file $exceptionsFile\n" unless !defined $exceptionsFile;

my $fileToUpdate = shift @ARGV;
-f $fileToUpdate or die "Couldn't find file to update $fileToUpdate\n";

my $warnAboutUnlocalizedStrings = defined $exceptionsFile;

my @directories = ();
if (@ARGV < 1) {
    push(@directories, ".");
} else {
    for my $dir (@ARGV) {
        push @directories, $dir;
    }
}

my $sawError = 0;

my $localizedCount = 0;
my $keyCollisionCount = 0;
my $notLocalizedCount = 0;
my $NSLocalizeCount = 0;

my %exception;
my %usedException;

if (defined $exceptionsFile && open EXCEPTIONS, $exceptionsFile) {
    while (<EXCEPTIONS>) {
        chomp;
        if (/^"([^\\"]|\\.)*"$/ or /^[-_\/\w\s.]+.(h|m|mm|c|cpp)$/ or /^[-_\/\w\s.]+.(h|m|mm|c|cpp):"([^\\"]|\\.)*"$/) {
            if ($exception{$_}) {
                print "$exceptionsFile:$.: warning: exception for $_ appears twice\n";
                print "$exceptionsFile:$exception{$_}: warning: first appearance\n";
            } else {
                $exception{$_} = $.;
            }
        } else {
            print "$exceptionsFile:$.: warning: syntax error\n";
        }
    }
    close EXCEPTIONS;
}

my $quotedDirectoriesString = '"' . join('" "', @directories) . '"';
for my $dir (@directoriesToSkip) {
    $quotedDirectoriesString .= ' -path "' . $dir . '" -prune -o';
}

my @files = ( split "\n", `find $quotedDirectoriesString \\( -name "*.h" -o -name "*.m" -o -name "*.mm" -o -name "*.c" -o -name "*.cpp" \\)` );

for my $file (sort @files) {
    next if $file =~ /\/\w+LocalizableStrings\w*\.h$/ || $file =~ /\/LocalizedStrings\.h$/;

    $file =~ s-^./--;

    open SOURCE, $file or die "can't open $file\n";
    
    my $inComment = 0;
    
    my $expected = "";
    my $macroLine;
    my $macro;
    my $UIString;
    my $key;
    my $comment;
    
    my $string;
    my $stringLine;
    my $nestingLevel;
    
    my $previousToken = "";

    while (<SOURCE>) {
        chomp;
        
        # Handle continued multi-line comment.
        if ($inComment) {
            next unless s-.*\*/--;
            $inComment = 0;
        }

        next unless defined $nestingLevel or /(\"|\/\*)/;
    
        # Handle all the tokens in the line.
        while (s-^\s*([#\w]+|/\*|//|[^#\w/'"()\[\],]+|.)--) {
            my $token = $1;
            
            if ($token eq "\"") {
                if ($expected and $expected ne "a quoted string") {
                    print "$file:$.: found a quoted string but expected $expected\n";
                    $sawError = 1;
                    $expected = "";
                }
                if (s-^(([^\\$token]|\\.)*?)$token--) {
                    if (!defined $string) {
                        $stringLine = $.;
                        $string = $1;
                    } else {
                        $string .= $1;
                    }
                } else {
                    print "$file:$.: mismatched quotes\n";
                    $sawError = 1;
                    $_ = "";
                }
                next;
            }
            
            if (defined $string) {
handleString:
                if ($expected) {
                    if (!defined $UIString) {
                        # FIXME: Validate UTF-8 here?
                        $UIString = $string;
                        $expected = ",";
                    } elsif (($macro =~ /(WEB_)?UI_STRING_KEY(_INTERNAL)?$/) and !defined $key) {
                        # FIXME: Validate UTF-8 here?
                        $key = $string;
                        $expected = ",";
                    } elsif (!defined $comment) {
                        # FIXME: Validate UTF-8 here?
                        $comment = $string;
                        $expected = ")";
                    }
                } else {
                    if (defined $nestingLevel) {
                        # In a debug macro, no need to localize.
                    } elsif ($previousToken eq "#include" or $previousToken eq "#import") {
                        # File name, no need to localize.
                    } elsif ($previousToken eq "extern" and $string eq "C") {
                        # extern "C", no need to localize.
                    } elsif ($string eq "") {
                        # Empty string can sometimes be localized, but we need not complain if not.
                    } elsif ($exception{$file}) {
                        $usedException{$file} = 1;
                    } elsif ($exception{"\"$string\""}) {
                        $usedException{"\"$string\""} = 1;
                    } elsif ($exception{"$file:\"$string\""}) {
                        $usedException{"$file:\"$string\""} = 1;
                    } else {
                        print "$file:$stringLine: warning: \"$string\" is not marked for localization\n" if $warnAboutUnlocalizedStrings;
                        $notLocalizedCount++;
                    }
                }
                $string = undef;
                last if !defined $token;
            }
            
            $previousToken = $token;

            if ($token =~ /^NSLocalized/ && $token !~ /NSLocalizedDescriptionKey/ && $token !~ /NSLocalizedStringFromTableInBundle/ && $token !~ /NSLocalizedFileSizeDescription/ && $token !~ /NSLocalizedDescriptionKey/ && $token !~ /NSLocalizedRecoverySuggestionErrorKey/) {
                print "$file:$.: found a use of an NSLocalized macro ($token); not supported\n";
                $nestingLevel = 0 if !defined $nestingLevel;
                $sawError = 1;
                $NSLocalizeCount++;
            } elsif ($token eq "/*") {
                if (!s-^.*?\*/--) {
                    $_ = ""; # If the comment doesn't end, discard the result of the line and set flag
                    $inComment = 1;
                }
            } elsif ($token eq "//") {
                $_ = ""; # Discard the rest of the line
            } elsif ($token eq "'") {
                if (!s-([^\\]|\\.)'--) { #' <-- that single quote makes the Project Builder editor less confused
                    print "$file:$.: mismatched single quote\n";
                    $sawError = 1;
                    $_ = "";
                }
            } else {
                if ($expected and $expected ne $token) {
                    print "$file:$.: found $token but expected $expected\n";
                    $sawError = 1;
                    $expected = "";
                }
                if ($token =~ /(WEB_)?UI_STRING(_KEY)?(_INTERNAL)?$/) {
                    $expected = "(";
                    $macro = $token;
                    $UIString = undef;
                    $key = undef;
                    $comment = undef;
                    $macroLine = $.;
                } elsif ($token eq "(" or $token eq "[") {
                    ++$nestingLevel if defined $nestingLevel;
                    $expected = "a quoted string" if $expected;
                } elsif ($token eq ",") {
                    $expected = "a quoted string" if $expected;
                } elsif ($token eq ")" or $token eq "]") {
                    $nestingLevel = undef if defined $nestingLevel && !--$nestingLevel;
                    if ($expected) {
                        $key = $UIString if !defined $key;
                        HandleUIString($UIString, $key, $comment, $file, $macroLine);
                        $macro = "";
                        $expected = "";
                        $localizedCount++;
                    }
                } elsif ($isDebugMacro{$token}) {
                    $nestingLevel = 0 if !defined $nestingLevel;
                }
            }
        }
            
    }
    
    goto handleString if defined $string;
    
    if ($expected) {
        print "$file: reached end of file but expected $expected\n";
        $sawError = 1;
    }
    
    close SOURCE;
}

# Unescapes C language hexadecimal escape sequences.
sub UnescapeHexSequence($)
{
    my ($originalStr) = @_;

    my $escapedStr = $originalStr;
    my $unescapedStr = "";

    for (;;) {
        if ($escapedStr =~ s-^\\x([[:xdigit:]]+)--) {
            if (256 <= hex($1)) {
                print "Hexadecimal escape sequence out of range: \\x$1\n";
                return undef;
            }
            $unescapedStr .= pack("H*", $1);
        } elsif ($escapedStr =~ s-^(.)--) {
            $unescapedStr .= $1;
        } else {
            return $unescapedStr;
        }
    }
}

my %stringByKey;
my %commentByKey;
my %fileByKey;
my %lineByKey;

sub HandleUIString
{
    my ($string, $key, $comment, $file, $line) = @_;

    my $bad = 0;
    $string = UnescapeHexSequence($string);
    if (!defined($string)) {
        print "$file:$line: string has an illegal hexadecimal escape sequence\n";
        $bad = 1;
    }
    $key = UnescapeHexSequence($key);
    if (!defined($key)) {
        print "$file:$line: key has an illegal hexadecimal escape sequence\n";
        $bad = 1;
    }
    $comment = UnescapeHexSequence($comment);
    if (!defined($comment)) {
        print "$file:$line: comment has an illegal hexadecimal escape sequence\n";
        $bad = 1;
    }
    if (grep { $_ == 0xFFFD } unpack "U*", $string) {
        print "$file:$line: string for translation has illegal UTF-8 -- most likely a problem with the Text Encoding of the source file\n";
        $bad = 1;
    }
    if ($string ne $key && grep { $_ == 0xFFFD } unpack "U*", $key) {
        print "$file:$line: key has illegal UTF-8 -- most likely a problem with the Text Encoding of the source file\n";
        $bad = 1;
    }
    if (grep { $_ == 0xFFFD } unpack "U*", $comment) {
        print "$file:$line: comment for translation has illegal UTF-8 -- most likely a problem with the Text Encoding of the source file\n";
        $bad = 1;
    }
    if ($bad) {
        $sawError = 1;
        return;
    }
    
    if ($stringByKey{$key} && $stringByKey{$key} ne $string) {
        print "$file:$line: warning: encountered the same key, \"$key\", twice, with different strings\n";
        print "$fileByKey{$key}:$lineByKey{$key}: warning: previous occurrence\n";
        $keyCollisionCount++;
        return;
    }
    if ($commentByKey{$key} && $commentByKey{$key} ne $comment) {
        print "$file:$line: warning: encountered the same key, \"$key\", twice, with different comments\n";
        print "$fileByKey{$key}:$lineByKey{$key}: warning: previous occurrence\n";
        $keyCollisionCount++;
        return;
    }

    $fileByKey{$key} = $file;
    $lineByKey{$key} = $line;
    $stringByKey{$key} = $string;
    $commentByKey{$key} = $comment;
}

print "\n" if $sawError || $notLocalizedCount || $NSLocalizeCount;

my @unusedExceptions = sort grep { !$usedException{$_} } keys %exception;
if (@unusedExceptions) {
    for my $unused (@unusedExceptions) {
        print "$exceptionsFile:$exception{$unused}: warning: exception $unused not used\n";
    }
    print "\n";
}

print "$localizedCount localizable strings\n" if $localizedCount;
print "$keyCollisionCount key collisions\n" if $keyCollisionCount;
print "$notLocalizedCount strings not marked for localization\n" if $notLocalizedCount;
print "$NSLocalizeCount uses of NSLocalize\n" if $NSLocalizeCount;
print scalar(@unusedExceptions), " unused exceptions\n" if @unusedExceptions;

if ($sawError) {
    print "\nErrors encountered. Exiting without writing to $fileToUpdate.\n";
    exit 1;
}

my $localizedStrings = "";

for my $key (sort keys %commentByKey) {
    $localizedStrings .= "/* $commentByKey{$key} */\n\"$key\" = \"$stringByKey{$key}\";\n\n";
}

if (-e "$fileToUpdate") {
    if (!$verify) {
        # Write out the strings file as UTF-8
        open STRINGS, ">", "$fileToUpdate" or die;
        print STRINGS $localizedStrings;
        close STRINGS;
    } else {
        open STRINGS, $fileToUpdate or die;

        my $lastComment;
        my $line;

        while (<STRINGS>) {
            chomp;

            next if (/^\s*$/);

            if (/^\/\* (.*) \*\/$/) {
                $lastComment = $1;
            } elsif (/^"((?:[^\\]|\\[^"])*)"\s*=\s*"((?:[^\\]|\\[^"])*)";$/) #
            {
                my $string = delete $stringByKey{$1};
                if (!defined $string) {
                    print "$fileToUpdate:$.: unused key \"$1\"\n";
                    $sawError = 1;
                } else {
                    if (!($string eq $2)) {
                        print "$fileToUpdate:$.: unexpected value \"$2\" for key \"$1\"\n";
                        print "$fileByKey{$1}:$lineByKey{$1}: expected value \"$string\" defined here\n";
                        $sawError = 1;
                    }
                    if (!($lastComment eq $commentByKey{$1})) {
                        print "$fileToUpdate:$.: unexpected comment /* $lastComment */ for key \"$1\"\n";
                        print "$fileByKey{$1}:$lineByKey{$1}: expected comment /* $commentByKey{$1} */ defined here\n";
                        $sawError = 1;
                    }
                }
            } else {
                print "$fileToUpdate:$.: line with unexpected format: $_\n";
                $sawError = 1;
            }
        }

        for my $missing (keys %stringByKey) {
            print "$fileByKey{$missing}:$lineByKey{$missing}: missing key \"$missing\"\n";
            $sawError = 1;
        }

        if ($sawError) {
            print "\n$fileToUpdate:0: file is not up to date.\n";
            exit 1;
        }
    }
} else {
    print "error: $fileToUpdate does not exist\n";
    exit 1;
}
