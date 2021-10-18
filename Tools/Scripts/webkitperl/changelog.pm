#!/usr/bin/env perl

# Copyright (C) 2007 Apple Inc. All rights reserved.
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
# 3.  Neither the name of Apple Inc. ("Apple") nor the names of
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

package webkitperl::changelog;

use strict;
use warnings;
use Carp;
use File::Spec;
use VCSUtils;

BEGIN {
   use Exporter   ();
   our ($VERSION, @ISA, @EXPORT, @EXPORT_OK, %EXPORT_TAGS);
   $VERSION     = 1.00;
   @ISA         = qw(Exporter);
   @EXPORT      = qw(
                    actuallyGenerateFunctionLists
                    attributeCommand
                    extractLineRangeAfterChange
                    extractLineRangeBeforeChange
                    fetchBugDescriptionFromBugXMLData
                    fetchRadarURLFromBugXMLData
                );
   %EXPORT_TAGS = ();
   @EXPORT_OK   = qw(
                    get_function_line_ranges_for_cpp
                    get_selector_line_ranges_for_css
                    get_function_line_ranges_for_java
                    get_function_line_ranges_for_javascript
                    get_function_line_ranges_for_perl
                    get_function_line_ranges_for_python
                    get_function_line_ranges_for_swift
                );
}

use constant SVN => "svn";
use constant GIT => "git";

sub actuallyGenerateFunctionLists {
    my ($changedFiles, $functionLists, $gitCommit, $gitIndex, $mergeBase, $delegateHashRef) = @_;

    my %line_ranges_after_changed;
    my %line_ranges_before_changed;
    if (@$changedFiles) {
        # For each file, build a list of modified lines.
        # Use line numbers from the "after" side of each diff.
        print STDERR "  Reviewing diff to determine which lines changed.\n";
        my $file;
        my $diffFileHandle = $delegateHashRef->{openDiff}($changedFiles, $gitCommit, $gitIndex, $mergeBase);
        if (!$diffFileHandle) {
            die "The diff failed: $!.\n";
        }
        while (<$diffFileHandle>) {
            my $filePath = parseDiffStartLine($_);
            $file = $delegateHashRef->{normalizePath}($filePath) if $filePath;
            if (defined $file) {
                my ($before_start, $before_end) = extractLineRangeBeforeChange($_);
                if ($before_start >= 1 && $before_end >= 1) {
                    push @{$line_ranges_before_changed{$file}}, [ $before_start, $before_end ];
                } elsif (/DO_NOT_COMMIT/) {
                    print STDERR "WARNING: file $file contains the string DO_NOT_COMMIT, line $.\n";
                }
                my ($after_start, $after_end) = extractLineRangeAfterChange($_);
                if ($after_start >= 1 && $after_end >= 1) {
                    push @{$line_ranges_after_changed{$file}}, [ $after_start, $after_end ];
                } elsif (/DO_NOT_COMMIT/) {
                    print STDERR "WARNING: file $file contains the string DO_NOT_COMMIT, line $.\n";
                }
            }
        }
        close($diffFileHandle);
    }

    # For each source file, convert line range to function list.
    print STDERR "  Extracting affected function names from source files.\n";
    my %filesToExamine = map { $_ => 1 } (keys(%line_ranges_before_changed), keys(%line_ranges_after_changed));
    foreach my $file (keys %filesToExamine) {
        my %saw_function;

        # Find all the functions in the file.
        my $sourceFileHandle = $delegateHashRef->{openFile}($file);
        next unless $sourceFileHandle;
        my @afterChangeFunctionRanges = get_function_line_ranges($sourceFileHandle, $file);
        close($sourceFileHandle);

        # Find modified functions in the file.
        if ($line_ranges_after_changed{$file}) {
            my @change_ranges = (@{$line_ranges_after_changed{$file}}, []);
            my @functions = computeModifiedFunctions($file, \@change_ranges, \@afterChangeFunctionRanges);

            # Format the list of functions.
            if (@functions) {
                $functionLists->{$file} = "" if !defined $functionLists->{$file};
                $functionLists->{$file} .= "\n        (" . join("):\n        (", @functions) . "):";
            }
        }
        # Find the deleted functions in the original file.
        if ($line_ranges_before_changed{$file}) {
            my $originalFileHandle = $delegateHashRef->{openOriginalFile}($file, $gitCommit, $gitIndex, $mergeBase);
            next unless $originalFileHandle;
            my @beforeChangeFunctionRanges = get_function_line_ranges($originalFileHandle, $file);
            close($originalFileHandle);

            my %existsAfterChange = map { $_->[2] => 1 } @afterChangeFunctionRanges;

            my @functions;
            my %sawFunctions;
            for my $functionRange (@beforeChangeFunctionRanges) {
                my $functionName = $functionRange->[2];
                if (!$existsAfterChange{$functionName} && !$sawFunctions{$functionName}) {
                    push @functions, $functionName;
                    $sawFunctions{$functionName} = 1;
                }
            }

            # Format the list of deleted functions.
            if (@functions) {
                $functionLists->{$file} = "" if !defined $functionLists->{$file};
                $functionLists->{$file} .= "\n        (" . join("): Deleted.\n        (", @functions) . "): Deleted.";
            }
        }
    }
}

sub extractLineRangeAfterChange {
    my $string = shift;
    my $chunkRange = parseChunkRange($string);
    if (!$chunkRange) {
        return (-1, -1); # Malformed
    }
    if (!$chunkRange->{newStartingLine} || !$chunkRange->{newLineCount}) {
         # Deletion; no lines exist after change.
        return ($chunkRange->{newStartingLine}, $chunkRange->{newStartingLine});
    }
    return ($chunkRange->{newStartingLine}, $chunkRange->{newStartingLine} + $chunkRange->{newLineCount} - 1);
}

sub extractLineRangeBeforeChange {
    my $string = shift;
    my $chunkRange = parseChunkRange($string);
    if (!$chunkRange) {
        return (-1, -1); # Malformed
    }
    if (!$chunkRange->{startingLine} || !$chunkRange->{lineCount}) {
        # Addition; no lines existed before change.
        return ($chunkRange->{startingLine}, $chunkRange->{startingLine});
    }
    return ($chunkRange->{startingLine}, $chunkRange->{startingLine} + $chunkRange->{lineCount} - 1);
}

sub attributeCommand {
    my ($attributeCache, $file, $attr) = @_;

    my $result;
    if (isSVN()) {
        my $devNull = File::Spec->devnull();
        my $foundAttribute = 0;
        my $subPath = ".";
        my (@directoryParts) = File::Spec->splitdir($file);
        foreach my $part (@directoryParts) {
            if ($part eq ".") {
                next;
            }
            $subPath = File::Spec->join($subPath, $part);
            $subPath =~ s/^\.\///;
            if ($foundAttribute || exists $attributeCache->{$attr}{$subPath} && $attributeCache->{$attr}{$subPath} eq "1") {
                $attributeCache->{$attr}{$subPath} = "1";
                $foundAttribute = 1;
                next;
            }
            my $command = SVN . " propget $attr '$subPath'";
            my $attrib = $attributeCache->{$attr}{$subPath} || `$command 2> $devNull`;
            chomp $attrib;
            if ($attrib eq "1") {
                $foundAttribute = 1;
            }
            $attributeCache->{$attr}{$subPath} = $attrib || "0";
        }
        $result = $attributeCache->{$attr}{$file};
    } elsif (isGit()) {
        my $command = GIT . " check-attr $attr -- '$file'";
        $result = `$command`;
        chomp $result;
        $result =~ s/.*\W(\w)/$1/;
    }

    $result =~ s/\D//g;
    return int($result || 0);
}

sub fetchBugDescriptionFromBugXMLData {
    my ($bugURL, $bugNumber, $bugXMLData) = @_;

    if ($bugXMLData !~ /<short_desc>(.*)<\/short_desc>/) {
        print STDERR "  Bug $bugNumber has no bug description. Maybe you set wrong bug ID?\n";
        print STDERR "  The bug URL: $bugURL\n";
        exit 1;
    }

    my $bugDescription = decodeEntities($1);
    print STDERR "  Description from bug $bugNumber:\n    \"$bugDescription\".\n";
    return $bugDescription;
}

sub fetchRadarURLFromBugXMLData {
    my ($bugNumber, $bugXMLData) = @_;

    return "" if $bugXMLData !~ m|<thetext>\s*((&lt;)?rdar://(problem/)?\d+(&gt;)?)|;

    my $bugRadarURL = decodeEntities($1);
    print STDERR "  Radar URL from bug $bugNumber:\n    \"$bugRadarURL\".\n";
    return $bugRadarURL;
}

sub computeModifiedFunctions {
    my ($file, $changedLineRanges, $functionRanges) = @_;

    my %sawFunction;

    # Find all the modified functions.
    my @functions;
    my @change_ranges = @{$changedLineRanges};
    my @change_range = (0, 0);
    FUNCTION: foreach my $function_range_ref (@{$functionRanges}) {
        my @function_range = @{$function_range_ref};

        # FIXME: This is a hack. If the function name is empty, skip it.
        # The cpp, python, javascript, perl, css and java parsers
        # are not perfectly implemented and sometimes function names cannot be retrieved
        # correctly. As you can see in get_function_line_ranges_XXXX(), those parsers
        # are not intended to implement real parsers but intended to just retrieve function names
        # for most practical syntaxes.
        next unless $function_range[2];

        # Advance to successive change ranges.
        for (;; @change_range = @{shift @change_ranges}) {
            last FUNCTION unless @change_range;

            # If past this function, move on to the next one.
            next FUNCTION if $change_range[0] > $function_range[1];

            # If an overlap with this function range, record the function name.
            if ($change_range[1] >= $function_range[0]
                and $change_range[0] <= $function_range[1]) {
                if (!$sawFunction{$function_range[2]}) {
                    $sawFunction{$function_range[2]} = 1;
                    push @functions, $function_range[2];
                }
                next FUNCTION;
            }
        }
    }

    return @functions;
}

sub get_function_line_ranges {
    my ($file_handle, $file_name) = @_;

    # Try to determine the source language based on the file extension.

    return get_function_line_ranges_for_cpp($file_handle, $file_name) if $file_name =~ /\.(c|cpp|m|mm|h)$/;
    return get_function_line_ranges_for_java($file_handle, $file_name) if $file_name =~ /\.java$/;
    return get_function_line_ranges_for_javascript($file_handle, $file_name) if $file_name =~ /\.js$/;
    return get_selector_line_ranges_for_css($file_handle, $file_name) if $file_name =~ /\.css$/;
    return get_function_line_ranges_for_perl($file_handle, $file_name) if $file_name =~ /\.p[lm]$/;
    return get_function_line_ranges_for_python($file_handle, $file_name) if $file_name =~ /\.py$/ or $file_name =~ /master\.cfg$/;
    return get_function_line_ranges_for_swift($file_handle, $file_name) if $file_name =~ /\.swift$/;

    # Try to determine the source language based on the script interpreter.

    my $first_line = <$file_handle>;
    seek($file_handle, 0, 0);

    return () unless $first_line =~ m|^#!(?:/usr/bin/env\s+)?(\S+)|;
    my $interpreter = $1;

    return get_function_line_ranges_for_perl($file_handle, $file_name) if $interpreter =~ /perl$/;
    return get_function_line_ranges_for_python($file_handle, $file_name) if $interpreter =~ /python$/;

    return ();
}

# Read a file and get all the line ranges of the things that look like C functions.
# A function name is the last word before an open parenthesis before the outer
# level open brace. A function starts at the first character after the last close
# brace or semicolon before the function name and ends at the close brace.
# Comment handling is simple-minded but will work for all but pathological cases.
#
# Result is a list of triples: [ start_line, end_line, function_name ].

sub get_function_line_ranges_for_cpp {
    my ($file_handle, $file_name) = @_;

    my @ranges;

    my $in_comment = 0;
    my $in_macro = 0;
    my $in_method_declaration = 0;
    my $in_parentheses = 0;
    my $quotation_mark;
    my $in_braces = 0;
    my $in_toplevel_array_brace = 0;
    my $brace_start = 0;
    my $brace_end = 0;
    my $namespace_start = -1;
    my $skip_til_brace_or_semicolon = 0;
    my $equal_observed = 0;

    my $word = "";
    my $interface_name = "";

    my $potential_method_char = "";
    my $potential_method_spec = "";

    my $potential_start = 0;
    my $potential_name = "";

    my $start = 0;
    my $name = "";

    my $next_word_could_be_namespace = 0;
    my $potential_namespace = "";
    my @namespaces;
    my @all_namespaces;

    while (<$file_handle>) {
        # Handle continued quoted string.
        if ($quotation_mark) {
            if (!s-([^\\]|\\.)*$quotation_mark--) {
                if (!m-\\$-) {
                    warn "mismatched quotes at line $. in $file_name\n";
                    undef $quotation_mark;
                }
                next;
            }
            undef $quotation_mark;
        }

        # Handle continued multi-line comment.
        if ($in_comment) {
            next unless s-.*\*/--;
            $in_comment = 0;
        }

        # Handle continued macro.
        if ($in_macro) {
            $in_macro = 0 unless /\\$/;
            next;
        }

        # Handle start of macro (or any preprocessor directive).
        if (/^\s*\#/) {
            $in_macro = 1 if /^([^\\]|\\.)*\\$/;
            next;
        }

        # Handle comments and quoted text.
        while (m-(/\*|//|\'|\")-) { # \' and \" keep emacs perl mode happy
            my $match = $1;
            if ($match eq "/*") {
                if (!s-/\*.*?\*/--) {
                    s-/\*.*--;
                    $in_comment = 1;
                }
            } elsif ($match eq "//") {
                s-//.*--;
            } else { # ' or "
                if (!s-$match([^\\]|\\.)*?$match--) {
                    if (!s-$match.*\\$--) {
                        warn "mismatched quotes at line $. in $file_name\n";
                        s-$match.*--;
                    } else {
                        $quotation_mark = $match;
                    }
                }
            }
        }


        # continued method declaration
        if ($in_method_declaration) {
              my $original = $_;
              my $method_cont = $_;

              chomp $method_cont;
              $method_cont =~ s/[;\{].*//;
              $potential_method_spec = "${potential_method_spec} ${method_cont}";

              $_ = $original;
              if (/;/) {
                  $potential_start = 0;
                  $potential_method_spec = "";
                  $potential_method_char = "";
                  $in_method_declaration = 0;
                  s/^[^;\{]*//;
              } elsif (/{/) {
                  my $selector = method_decl_to_selector ($potential_method_spec);
                  $potential_name = "${potential_method_char}\[${interface_name} ${selector}\]";

                  $potential_method_spec = "";
                  $potential_method_char = "";
                  $in_method_declaration = 0;

                  $_ = $original;
                  s/^[^;{]*//;
              } elsif (/\@end/) {
                  $in_method_declaration = 0;
                  $interface_name = "";
                  $_ = $original;
              } else {
                  next;
              }
        }


        # start of method declaration
        if ((my $method_char, my $method_spec) = m&^([-+])([^0-9;][^;]*);?$&) {
            my $original = $_;

            if ($interface_name) {
                chomp $method_spec;
                $method_spec =~ s/\{.*//;

                $potential_method_char = $method_char;
                $potential_method_spec = $method_spec;
                $potential_start = $.;
                $in_method_declaration = 1;
            } else {
                warn "declaring a method but don't have interface on line $. in $file_name\n";
            }
            $_ = $original;
            if (/\{/) {
              my $selector = method_decl_to_selector ($potential_method_spec);
              $potential_name = "${potential_method_char}\[${interface_name} ${selector}\]";

              $potential_method_spec = "";
              $potential_method_char = "";
              $in_method_declaration = 0;
              $_ = $original;
              s/^[^{]*//;
            } elsif (/\@end/) {
              $in_method_declaration = 0;
              $interface_name = "";
              $_ = $original;
            } else {
              next;
            }
        }


        # Find function, interface and method names.
        while (m&((?:[[:word:]]+::)*operator(?:[ \t]*\(\)|[^()]*)|[[:word:]<>:~]+|[(){}:;=])|\@(?:implementation|interface|protocol)\s+(\w+)[^{]*&g) {
            # Skip an array definition at the top level.
            # e.g. static int arr[] = { 1, 2, 3 };
            if ($1) {
                if ($1 eq "=" and !$in_parentheses and !$in_braces) {
                    $equal_observed = 1;
                } elsif ($1 eq "{" and $equal_observed) {
                    # This '{' is the beginning of an array definition, not the beginning of a method.
                    $in_toplevel_array_brace = 1;
                    $in_braces++;
                    $equal_observed = 0;
                    next;
                } elsif ($1 !~ /[ \t]/) {
                    $equal_observed = 0;
                }
            }

            # interface name
            if ($2) {
                $interface_name = $2;
                next;
            }

            # Open parenthesis.
            if ($1 eq "(") {
                $potential_name = $word unless $in_parentheses || $skip_til_brace_or_semicolon || grep { $word eq $_ } ("CF_ENUM", "CF_OPTIONS", "NS_ENUM", "NS_OPTIONS");
                $in_parentheses++;
                next;
            }

            # Close parenthesis.
            if ($1 eq ")") {
                $in_parentheses--;
                next;
            }

            if ($1 eq "const" and !$in_parentheses) {
                $potential_name .= " const";
                next;
            }

            if ($1 eq "volatile" and !$in_parentheses) {
                $potential_name .= " volatile";
                next;
            }

            # C++ auto function() -> type
            if ($1 eq ">") {
                $skip_til_brace_or_semicolon = 1 unless ($in_parentheses || $in_braces);
                next;
            }

            # C++ constructor initializers
            if ($1 eq ":") {
                $skip_til_brace_or_semicolon = 1 unless ($in_parentheses || $in_braces);
            }

            # Open brace.
            if ($1 eq "{") {
                $skip_til_brace_or_semicolon = 0;

                if (!$in_braces) {
                    if ($namespace_start >= 0 and $namespace_start < $potential_start) {
                        push @ranges, [ $namespace_start . "", $potential_start - 1, $name ];
                    }

                    if ($potential_namespace) {
                        push @namespaces, $potential_namespace;
                        push @all_namespaces, $potential_namespace;
                        $potential_namespace = "";
                        $name = $namespaces[-1];
                        $namespace_start = $. + 1;
                        next;
                    }

                    # Promote potential name to real function name at the
                    # start of the outer level set of braces (function body?).
                    if ($potential_start) {
                        $start = $potential_start;
                        $name = $potential_name;
                        if (@namespaces && $name && (length($name) < 2 || substr($name,1,1) ne "[")) {
                            $name = join ('::', @namespaces, $name);
                        }
                    }
                }

                $in_method_declaration = 0;

                $brace_start = $. if (!$in_braces);
                $in_braces++;
                next;
            }

            # Close brace.
            if ($1 eq "}") {
                if (!$in_braces && @namespaces) {
                    if ($namespace_start >= 0 and $namespace_start < $.) {
                        push @ranges, [ $namespace_start . "", $. - 1, $name ];
                    }

                    pop @namespaces;
                    if (@namespaces) {
                        $name = $namespaces[-1];
                        $namespace_start = $. + 1;
                    } else {
                        $name = "";
                        $namespace_start = -1;
                    }
                    next;
                }

                $in_braces--;
                $brace_end = $. if (!$in_braces);

                # End of an outer level set of braces.
                # This could be a function body.
                if (!$in_braces and $name) {
                    # This is the end of an array definition at the top level, not the end of a method.
                    if ($in_toplevel_array_brace) {
                        $in_toplevel_array_brace = 0;
                        next;
                    }

                    push @ranges, [ $start, $., $name ];
                    if (@namespaces) {
                        $name = $namespaces[-1];
                        $namespace_start = $. + 1;
                    } else {
                        $name = "";
                        $namespace_start = -1;
                    }
                }

                $potential_start = 0;
                $potential_name = "";
                next;
            }

            # Semicolon.
            if ($1 eq ";") {
                $skip_til_brace_or_semicolon = 0;
                $potential_start = 0;
                $potential_name = "";
                $in_method_declaration = 0;
                next;
            }

            # Ignore "const" method qualifier.
            if ($1 eq "const") {
                next;
            }

            if ($1 eq "namespace" || $1 eq "class" || $1 eq "struct") {
                $next_word_could_be_namespace = 1;
                next;
            }

            # Word.
            $word = $1;
            if (!$skip_til_brace_or_semicolon) {
                if ($next_word_could_be_namespace) {
                    $potential_namespace = $word;
                    $next_word_could_be_namespace = 0;
                } elsif ($potential_namespace) {
                    $potential_namespace = "";
                }

                if (!$in_parentheses) {
                    $potential_start = 0;
                    $potential_name = "";
                }
                if (!$potential_start) {
                    $potential_start = $.;
                    $potential_name = "";
                }
            }
        }
    }

    warn "missing close braces in $file_name (probable start at $brace_start)\n" if ($in_braces > 0);
    warn "too many close braces in $file_name (probable start at $brace_end)\n" if ($in_braces < 0);

    warn "mismatched parentheses in $file_name\n" if $in_parentheses;

    return delete_namespaces_from_ranges_for_cpp(\@ranges, \@all_namespaces);
}


# Take in references to an array of line ranges for C functions in a given file
# and an array of namespaces declared in that file and return an updated
# list of line ranges with the namespaces removed.

sub delete_namespaces_from_ranges_for_cpp {
    my ($ranges_ref, $namespaces_ref) = @_;
    return grep {!is_function_in_namespace($namespaces_ref, $$_[2])} @$ranges_ref;
}


sub is_function_in_namespace {
    my ($namespaces_ref, $function_name) = @_;
    return grep {$_ eq $function_name} @$namespaces_ref;
}


# Read a file and get all the line ranges of the things that look like Java
# classes, interfaces and methods.
#
# A class or interface name is the word that immediately follows
# `class' or `interface' when followed by an open curly brace and not
# a semicolon. It can appear at the top level, or inside another class
# or interface block, but not inside a function block
#
# A class or interface starts at the first character after the first close
# brace or after the function name and ends at the close brace.
#
# A function name is the last word before an open parenthesis before
# an open brace rather than a semicolon. It can appear at top level or
# inside a class or interface block, but not inside a function block.
#
# A function starts at the first character after the first close
# brace or after the function name and ends at the close brace.
#
# Comment handling is simple-minded but will work for all but pathological cases.
#
# Result is a list of triples: [ start_line, end_line, function_name ].

sub get_function_line_ranges_for_java {
    my ($file_handle, $file_name) = @_;

    my @current_scopes;

    my @ranges;

    my $in_comment = 0;
    my $in_macro = 0;
    my $in_parentheses = 0;
    my $in_braces = 0;
    my $in_non_block_braces = 0;
    my $class_or_interface_just_seen = 0;
    my $in_class_declaration = 0;

    my $word = "";

    my $potential_start = 0;
    my $potential_name = "";
    my $potential_name_is_class_or_interface = 0;

    my $start = 0;
    my $name = "";
    my $current_name_is_class_or_interface = 0;

    while (<$file_handle>) {
        # Handle continued multi-line comment.
        if ($in_comment) {
            next unless s-.*\*/--;
            $in_comment = 0;
        }

        # Handle continued macro.
        if ($in_macro) {
            $in_macro = 0 unless /\\$/;
            next;
        }

        # Handle start of macro (or any preprocessor directive).
        if (/^\s*\#/) {
            $in_macro = 1 if /^([^\\]|\\.)*\\$/;
            next;
        }

        # Handle comments and quoted text.
        while (m-(/\*|//|\'|\")-) { # \' and \" keep emacs perl mode happy
            my $match = $1;
            if ($match eq "/*") {
                if (!s-/\*.*?\*/--) {
                    s-/\*.*--;
                    $in_comment = 1;
                }
            } elsif ($match eq "//") {
                s-//.*--;
            } else { # ' or "
                if (!s-$match([^\\]|\\.)*?$match--) {
                    warn "mismatched quotes at line $. in $file_name\n";
                    s-$match.*--;
                }
            }
        }

        # Find function names.
        while (m-(\w+|[(){};])-g) {
            # Open parenthesis.
            if ($1 eq "(") {
                if (!$in_parentheses) {
                    $potential_name = $word;
                    $potential_name_is_class_or_interface = 0;
                }
                $in_parentheses++;
                next;
            }

            # Close parenthesis.
            if ($1 eq ")") {
                $in_parentheses--;
                next;
            }

            # Open brace.
            if ($1 eq "{") {
                $in_class_declaration = 0;

                # Promote potential name to real function name at the
                # start of the outer level set of braces (function/class/interface body?).
                if (!$in_non_block_braces
                    and (!$in_braces or $current_name_is_class_or_interface)
                    and $potential_start) {
                    if ($name) {
                          push @ranges, [ $start, ($. - 1),
                                          join ('.', @current_scopes) ];
                    }


                    $current_name_is_class_or_interface = $potential_name_is_class_or_interface;

                    $start = $potential_start;
                    $name = $potential_name;

                    push (@current_scopes, $name);
                } else {
                    $in_non_block_braces++;
                }

                $potential_name = "";
                $potential_start = 0;

                $in_braces++;
                next;
            }

            # Close brace.
            if ($1 eq "}") {
                $in_braces--;

                # End of an outer level set of braces.
                # This could be a function body.
                if (!$in_non_block_braces) {
                    if ($name) {
                        push @ranges, [ $start, $.,
                                        join ('.', @current_scopes) ];

                        pop (@current_scopes);

                        if (@current_scopes) {
                            $current_name_is_class_or_interface = 1;

                            $start = $. + 1;
                            $name =  $current_scopes[$#current_scopes-1];
                        } else {
                            $current_name_is_class_or_interface = 0;
                            $start = 0;
                            $name =  "";
                        }
                    }
                } else {
                    $in_non_block_braces-- if $in_non_block_braces;
                }

                $potential_start = 0;
                $potential_name = "";
                next;
            }

            # Semicolon.
            if ($1 eq ";") {
                $potential_start = 0;
                $potential_name = "";
                next;
            }

            if ($1 eq "class") {
                $in_class_declaration = 1;
            }
            if ($1 eq "class" or (!$in_class_declaration and $1 eq "interface")) {
                $class_or_interface_just_seen = 1;
                next;
            }

            # Word.
            $word = $1;
            if (!$in_parentheses) {
                if ($class_or_interface_just_seen) {
                    $potential_name = $word;
                    $potential_start = $.;
                    $class_or_interface_just_seen = 0;
                    $potential_name_is_class_or_interface = 1;
                    next;
                }
            }
            if (!$potential_start) {
                $potential_start = $.;
                $potential_name = "";
            }
            $class_or_interface_just_seen = 0;
        }
    }

    warn "mismatched braces in $file_name\n" if $in_braces;
    warn "mismatched parentheses in $file_name\n" if $in_parentheses;

    return @ranges;
}



# Read a file and get all the line ranges of the things that look like
# JavaScript functions or methods.
#
# A function name is the word that immediately follows `function' when
# followed by an open curly brace. It can appear at the top level,
# or inside other functions. For example:
#
#    function name() { // (name)
#        function inner() { } // (name.inner)
#    }
#
# An anonymous function name is the identifier on the left hand side of
# an assignment with the equals operator or object notation that has a
# value starting with `function' followed an open curly brace.
# For example:
#
#    namespace = {
#        name: function() {} // (namespace.name)
#    }
#    namespace.Foo = function() {} // (namespace.Foo)
#
# A getter or setter name is the word that immediately follows `get' or
# `set' when followed by params and an open curly brace. For example:
#
#    namespace = {
#      get foo() {} // (namespace.get foo)
#    }
#
# A method name is the word immediately before parenthesis, with an open
# curly brace immediately following closing parenthesis. For a class expression
# we take the assignment identifier instead of the class name for namespacing.
#
#    namespace.Foo = class DoesNotMatter extends Bar {
#        constructor() {} // (namespace.Foo)
#        static staticMethod() {} // (namespace.Foo.staticMethod)
#        instanceMethod() {} // (namespace.Foo.prototype.instanceMethod)
#        get getter() {} // (namespace.Foo.prototype.get getter)
#    }
#    class ClassName {
#        constructor() {} // (ClassName)
#        method() {} // (ClassName.prototype.method)
#    }
#
# Methods may exist in object literals, outside of classes.
#
#   Foo.prototype = {
#       method() {}, // (Foo.prototype.method)
#       otherMethod() {} // (Foo.prototype.otherMethod)
#   }
#
# Comment handling is simple-minded but will work for all but pathological cases.
#
# Result is a list of triples: [ start_line, end_line, function_name ].

sub get_function_line_ranges_for_javascript {
    my ($fileHandle, $fileName) = @_;

    my @currentScopes;
    my @currentIdentifiers;
    my @currentParsingMode = ("global");
    my @currentFunctionNames;
    my @currentFunctionDepths;
    my @currentFunctionStartLines;

    my @ranges;

    my $inComment = 0;
    my $inQuotedText = "";
    my $inExtends = 0;
    my $inMethod = 0;
    my $inAnonymousFunctionParameters = 0;
    my $parenthesesDepth = 0;
    my $globalParenthesesDepth = 0;
    my $bracesDepth = 0;

    my $classJustSeen = 0;
    my $parenthesisJustSeen = 0;
    my $functionJustSeen = 0;
    my $getterJustSeen = 0;
    my $setterJustSeen = 0;
    my $asyncJustSeen = 0;
    my $assignmentJustSeen = 0;
    my $staticOrContructorSeen = 0;

    my $currentToken = "";
    my $lastToken = "";
    my $possibleMethodName = "";
    my $word = "";

    while (<$fileHandle>) {
        # Handle continued multi-line comment.
        if ($inComment) {
            next unless s-.*\*/--;
            $inComment = 0;
        }

        # Handle continued quoted text.
        if ($inQuotedText ne "") {
            next if /\\$/;
            s-([^\\]|\\.)*?$inQuotedText--;
            $inQuotedText = "";
        }

        # Handle comments and quoted text.
        while (m-(/\*|//|\'|\")-) { # \' and \" keep emacs perl mode happy
            my $match = $1;
            if ($match eq '/*') {
                if (!s-/\*.*?\*/--) {
                    s-/\*.*--;
                    $inComment = 1;
                }
            } elsif ($match eq '//') {
                s-//.*--;
            } else { # ' or "
                if (!s-$match([^\\]|\\.)*?$match-string_appeared_here-) {
                    $inQuotedText = $match if /\\$/;
                    warn "mismatched quotes at line $. in $fileName\n" if $inQuotedText eq "";
                    s-$match.*--;
                }
            }
        }

        # Find function names.
        while (m-(\w+|[(){}=:;,.])-g) {
            # Skip everything until "{" after extends.
            if ($inExtends) {
                next if $1 ne '{';
                $inExtends = 0;
            }

            $lastToken = $currentToken;
            $currentToken = $1;

            # Open parenthesis.
            if ($1 eq '(') {
                $parenthesesDepth++;
                $globalParenthesesDepth++ if $currentParsingMode[$#currentParsingMode] eq "global";
                $possibleMethodName = join('.', @currentIdentifiers);
                $inAnonymousFunctionParameters = 1 if $functionJustSeen;
                $functionJustSeen = 0;
                next;
            }

            # Close parenthesis.
            if ($1 eq ')') {
                $parenthesesDepth--;
                $globalParenthesesDepth-- if $currentParsingMode[$#currentParsingMode] eq "global";
                @currentIdentifiers = () if $inAnonymousFunctionParameters;
                $inAnonymousFunctionParameters = 0;
                $parenthesisJustSeen = 1;
                next;
            }

            # Open brace.
            if ($1 eq '{') {
                my $methodName = "";
                my $mode = $currentParsingMode[$#currentParsingMode];

                # Method.
                if (($mode eq 'class' or $mode eq 'global') and $parenthesisJustSeen and ($staticOrContructorSeen or $possibleMethodName)) {
                    if ($mode eq 'class') {
                        $methodName = join('.', $staticOrContructorSeen ? "" : "prototype", $possibleMethodName);
                    } else {
                        $methodName = $possibleMethodName;
                    }

                    $methodName =~ s/\.{2,}/\./g; # Removes consecutive periods.
                    $methodName =~ s/\.$//; # Remove trailing period.

                    my $currentMethod = join('.', @currentScopes, $methodName);
                    $currentMethod =~ s/\.{2,}/\./g; # Removes consecutive periods.
                    $currentMethod =~ s/\.$//; # Remove trailing period.

                    push(@currentParsingMode, "method");
                    push(@currentFunctionNames, $currentMethod);
                    push(@currentFunctionDepths, $bracesDepth);
                    push(@currentFunctionStartLines, $.);
                }

                $bracesDepth++;
                $functionJustSeen = 0;

                push(@currentScopes, join('.', $methodName ? $methodName : @currentIdentifiers));
                @currentIdentifiers = ();

                $staticOrContructorSeen = 0;
                next;
            }

            # Close brace.
            if ($1 eq '}') {
                $bracesDepth--;
                $functionJustSeen = 0;

                if (@currentFunctionDepths and $bracesDepth == $currentFunctionDepths[$#currentFunctionDepths]) {
                    pop(@currentFunctionDepths);
                    pop(@currentParsingMode);

                    my $currentName = pop(@currentFunctionNames);
                    my $start = pop(@currentFunctionStartLines);

                    $currentName =~ s/^\.//g; # Removes leading periods.

                    push(@ranges, [$start, $., $currentName]);
                }

                pop(@currentScopes);
                @currentIdentifiers = ();

                next;
            }

            # Dot.
            if ($1 eq '.') {
                next;
            }

            # Semicolon or comma.
            if ($1 eq ';' or $1 eq ',') {
                @currentIdentifiers = ();
                next;
            }

            # Class.
            if ($1 eq 'class') {
                $classJustSeen = 1;
                next;
            }

            # Extends.
            if ($1 eq 'extends') {
                $inExtends = 1;
                next;
            }

            # Function.
            if ($1 eq 'function') {
                $functionJustSeen = 1;

                if ($assignmentJustSeen) {
                    my $currentFunction = join('.', (@currentScopes, @currentIdentifiers));
                    $currentFunction =~ s/\.{2,}/\./g; # Removes consecutive periods.

                    push(@currentParsingMode, "function");
                    push(@currentFunctionNames, $currentFunction);
                    push(@currentFunctionDepths, $bracesDepth);
                    push(@currentFunctionStartLines, $.);
                }

                next;
            }

            # Getter prefix.
            if ($1 eq 'get') {
                next if $lastToken eq '.'; # Avoid map.get(...).
                $getterJustSeen = 1;
                next;
            }

            # Setter prefix.
            if ($1 eq 'set') {
                next if $lastToken eq '.'; # Avoid map.set(...).
                $setterJustSeen = 1;
                next;
            }

            # Async prefix.
            if ($1 eq 'async') {
                next if $lastToken eq '.';
                $asyncJustSeen = 1;
                next;
            }

            # Static.
            if ($1 eq 'static' or $1 eq 'constructor') {
                $staticOrContructorSeen = 1;
                next;
            }

            # Assignment operator.
            if ($1 eq '=' or $1 eq ':') {
                $assignmentJustSeen = 1;
                next;
            }

            next if $parenthesesDepth > $globalParenthesesDepth;

            # Word.
            $word = $1;

            if ($classJustSeen) {
                push(@currentIdentifiers, $word) if !$assignmentJustSeen;

                my $currentClass = join('.', (@currentScopes, @currentIdentifiers));
                $currentClass =~ s/\.{2,}/\./g; # Removes consecutive periods.

                push(@currentParsingMode, "class");
                push(@currentFunctionNames, $currentClass);
                push(@currentFunctionDepths, $bracesDepth);
                push(@currentFunctionStartLines, $.);
            } elsif ($getterJustSeen or $setterJustSeen or $asyncJustSeen) {
                $word = "get $word" if $getterJustSeen;
                $word = "set $word" if $setterJustSeen;
                $word = "async $word" if $asyncJustSeen;

                push(@currentIdentifiers, $word);

                my $mode = $currentParsingMode[$#currentParsingMode];
                my $currentFunction = join('.', (@currentScopes, ($mode eq 'class' and !$staticOrContructorSeen) ? "prototype" : "", @currentIdentifiers));
                $currentFunction =~ s/\.{2,}/\./g; # Removes consecutive periods.

                push(@currentParsingMode, "function");
                push(@currentFunctionNames, $currentFunction);
                push(@currentFunctionDepths, $bracesDepth);
                push(@currentFunctionStartLines, $.);
            } elsif ($functionJustSeen and !$assignmentJustSeen) {
                push(@currentIdentifiers, $word);

                my $currentFunction = join('.', (@currentScopes, @currentIdentifiers));
                $currentFunction =~ s/\.{2,}/\./g; # Removes consecutive periods.

                push(@currentParsingMode, "function");
                push(@currentFunctionNames, $currentFunction);
                push(@currentFunctionDepths, $bracesDepth);
                push(@currentFunctionStartLines, $.);
            } elsif ($word ne 'if' and $word ne 'for' and $word ne 'do' and $word ne 'while' and $word ne 'which' and $word ne 'var') {
                push(@currentIdentifiers, $word);
            }

            $classJustSeen = 0;
            $parenthesisJustSeen = 0;
            $functionJustSeen = 0;
            $getterJustSeen = 0;
            $setterJustSeen = 0;
            $asyncJustSeen = 0;
            $assignmentJustSeen = 0;
        }
    }

    warn "mismatched braces in $fileName\n" if $bracesDepth;
    warn "mismatched parentheses in $fileName\n" if $parenthesesDepth;

    return @ranges;
}

# Read a file and get all the line ranges of the things that look like Perl functions. Functions
# start on a line that starts with "sub ", and end on the first line starting with "}" thereafter.
#
# Result is a list of triples: [ start_line, end_line, function ].

sub get_function_line_ranges_for_perl {
    my ($fileHandle, $fileName) = @_;

    my @ranges;

    my $currentFunction = "";
    my $start = 0;
    my $hereDocumentIdentifier = "";

    while (<$fileHandle>) {
        chomp;
        if (!$hereDocumentIdentifier) {
            if (/^sub\s+([\w_][\w\d_]*)/) {
                # Skip over forward declarations, which don't contain a brace and end with a semicolon.
                next if /;\s*$/;

                if ($currentFunction) {
                    warn "nested functions found at top-level at $fileName:$.\n";
                    next;
                }
                $currentFunction = $1;
                $start = $.;
            }
            if (/<<\s*[\"\']?([\w_][\w_\d]*)/) {
                # Enter here-document.
                $hereDocumentIdentifier = $1;
            }
            if (index($_, "}") == 0) {
                next unless $start;
                push(@ranges, [$start, $., $currentFunction]);
                $currentFunction = "";
                $start = 0;
            }
        } elsif ($_ eq $hereDocumentIdentifier) {
            # Escape from here-document.
            $hereDocumentIdentifier = "";
        }
    }

    return @ranges;
}

# Read a file and get all the line ranges of the things that look like Python classes, methods, or functions.
#
# FIXME: Maybe we should use Python's ast module to do the parsing for us?
#
# Result is a list of triples: [ start_line, end_line, function ].

sub get_function_line_ranges_for_python {
    my ($fileHandle, $fileName) = @_;

    my @ranges;

    my $multilineStringLiteralSentinelRegEx = qr#(?:"""|''')#;
    my $multilineStringLiteralStartRegEx = qr#\s*$multilineStringLiteralSentinelRegEx#;
    my $multilineStringLiteralEndRegEx = qr#$multilineStringLiteralSentinelRegEx\s*$#;

    my @scopeStack = ({ line => 0, indent => -1, name => undef });
    my $lastLine = 0;
    my $inComment = 0;
    until ($lastLine) {
        $_ = <$fileHandle>;
        unless ($_) {
            # To pop out all popped scopes, run the loop once more after
            # we encountered the end of the file.
            $_ = "pass\n";
            $.++;
            $lastLine = 1;
        }
        chomp;
        next unless /^(\s*)([^#].*)$/; # Skip non-indented lines that begin with a comment.

        my $indent = length $1;
        my $rest = $2;
        my $scope = $scopeStack[-1];

        if ($indent <= $scope->{indent}) {
            # Find all the scopes that we have just exited.
            my $i = 0;
            for (; $i < @scopeStack; ++$i) {
                last if $indent <= $scopeStack[$i]->{indent};
            }
            my @poppedScopes = splice @scopeStack, $i;

            # For each scope that was just exited, add a range that goes from the start of that
            # scope to the start of the next nested scope, or to the line just before this one for
            # the innermost scope.
            for ($i = 0; $i < @poppedScopes; ++$i) {
                my $lineAfterEnd = $i + 1 == @poppedScopes ? $. : $poppedScopes[$i + 1]->{line};
                push @ranges, [$poppedScopes[$i]->{line}, $lineAfterEnd - 1, $poppedScopes[$i]->{name}];
            }
            @scopeStack or warn "Popped off last scope at $fileName:$.\n";

            # Set the now-current scope to start at the current line. Any lines within this scope
            # before this point should already have been added to @ranges.
            $scope = $scopeStack[-1];
            $scope->{line} = $.;
        }

        # Skip multi-line string literals and docstrings.
        next if /$multilineStringLiteralStartRegEx.*$multilineStringLiteralEndRegEx/;
        if (!$inComment && /$multilineStringLiteralStartRegEx/) {
            $inComment = 1;
        } elsif ($inComment && /$multilineStringLiteralEndRegEx/) {
            $inComment = 0;
        }
        next if $inComment;

        next if /^\s*[#'"]/; # Skip indented and non-indented lines that begin with a comment or string literal (includes docstrings).

        next unless $rest =~ /(?:class|def)\s+(\w+)/;
        my $name = $1;
        my $fullName = $scope->{name} ? join('.', $scope->{name}, $name) : $name;
        push @scopeStack, { line => $., indent => $indent, name => $fullName };

        if ($scope->{indent} >= 0) {
            push @ranges, [$scope->{line}, $. - 1, $scope->{name}];
        }
    }

    return @ranges;
}

# Read a file and get all the line ranges of the things that look like CSS selectors.  A selector is
# anything before an opening brace on a line. A selector starts at the line containing the opening
# brace and ends at the closing brace.
#
# Result is a list of triples: [ start_line, end_line, selector ].

sub get_selector_line_ranges_for_css {
    my ($fileHandle, $fileName) = @_;

    my @ranges;

    my $inComment = 0;
    my $inBrace = 0;
    my @stack;
    my $context;
    my @currentParseMode = ("global");
    my @groupingStack;
    my $selectorBraces = 0;

    while (<$fileHandle>) {
        foreach my $token (split m-(\{|\}|/\*|\*/)-, $_) {
            if ($token eq "{") {
                if (!$inComment) {
                    $inBrace += 1;
                    $selectorBraces += 1 if $currentParseMode[$#currentParseMode] eq "selector";
                    warn "mismatched opening brace found in $fileName:$.\n" if $selectorBraces > 1;
                }
            } elsif ($token eq "}") {
                if (!$inComment) {
                    if (!$inBrace or $currentParseMode[$#currentParseMode] eq "global") {
                        warn "mismatched closing brace found in $fileName:$.\n";
                        next;
                    }

                    $inBrace -= 1;

                    my $parseMode = pop(@currentParseMode);
                    if ($parseMode eq "selector") {
                        my $name = pop(@stack);
                        my $startLine = pop(@stack);
                        my $endLine = $.;
                        my $groupingPrefix = join(" ", @groupingStack);
                        if (length $groupingPrefix) {
                            $groupingPrefix .= " "
                        }
                        push(@ranges, [$startLine, $endLine, $groupingPrefix . $name]);
                    } elsif ($parseMode eq "media") {
                        pop(@groupingStack);
                    }

                    $selectorBraces = 0;
                }
            } elsif ($token eq "/*") {
                $inComment = 1;
            } elsif ($token eq "*/") {
                warn "mismatched comment found in $fileName:$.\n" if !$inComment;
                $inComment = 0;
            } else {
                if (!$inComment and $currentParseMode[$#currentParseMode] ne "selector" and $token !~ /^[\s\t]*$/) {
                    $token =~ s/^[\s\t]*|[\s\t]*$//g;
                    my $startLine = $.;
                    if ($token =~ /^\@media/) {
                        push(@currentParseMode, "media");
                        push(@groupingStack, $token);
                    } else {
                        push(@currentParseMode, "selector");
                        push(@stack, ($startLine, $token));
                    }
                }
            }
        }
    }

    # Sort by start line.
    return sort {$a->[0] <=> $b->[0]} @ranges;
}

# Read a file and get all the line ranges of the things that look like Swift classes, methods,
# or functions.
#
# Result is a list of triples: [ start_line, end_line, function ].

sub get_function_line_ranges_for_swift {
    my ($fileHandle, $fileName) = @_;

    my @ranges;

    my $currentFunction = "";
    my $currentType = "";
    my $functionStart = 0;
    my $typeStart = 0;
    my $functionScopeDepth = 0;
    my $typeScopeDepth = 0;
    my $scopeDepth = 0;

    while (<$fileHandle>) {
        chomp;
        next if (/^\s*\/\/.*/);
        if (/func\s+([\w_][\w\d_]*)\((.*)\)/ || /var\s+([\w_][\w\d_]*):\s+/) {
            $functionScopeDepth = $scopeDepth;
            $currentFunction = $1;
            if ($2) {
                $currentFunction = "$currentFunction(". parseSwiftFunctionArgs($2) . ")";
            }
            if ($currentType) {
                $currentFunction = "$currentType.$currentFunction";
            }
            $functionStart = $.;
        } elsif (/(?:class|struct|enum|protocol|extension)\s+([\w_][\w\d_]*)/) {
            $typeScopeDepth = $scopeDepth;
            $currentType = $1;
            $typeStart = $.;
        }
        if (index($_, "{") > -1) {
            $scopeDepth++;
        }
        if (index($_, "}") > -1) {
            $scopeDepth--;
        }
        if ($scopeDepth == $functionScopeDepth) {
            next unless $functionStart;
            push(@ranges, [$functionStart, $., $currentFunction]);
            $currentFunction = "";
            $functionStart = 0;
        } elsif ($scopeDepth == $typeScopeDepth) {
            next unless $typeStart;
            $currentType = "";
            $typeStart = 0;
        }
    }

    return @ranges;
}

sub parseSwiftFunctionArgs {
    my $functionArgs = shift;

    my @words = split /, /, $functionArgs;
    my $argCount = scalar(@words);
    if ($argCount == 0) {
        return "";
    } elsif ($argCount > 0) {
        # If the first argument is unnamed, give it the name "_"
        $words[0] =~ s/^(\w+: .*)/_ $1/;
        return join("", map { $_ =~ s/^(\w+).*/$1/; "$_:" } @words);
    } else {
        warn "Unknown argument count.\n";
    }
}

sub decodeEntities {
    my $text = shift;
    $text =~ s/\&lt;/</g;
    $text =~ s/\&gt;/>/g;
    $text =~ s/\&quot;/\"/g;
    $text =~ s/\&apos;/\'/g;
    $text =~ s/\&amp;/\&/g;
    return $text;
}

1;
