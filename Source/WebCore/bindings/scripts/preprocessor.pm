#
# Copyright (C) 2005 Nikolas Zimmermann <wildfox@kde.org>
# Copyright (C) 2011 Google Inc.
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Library General Public
# License as published by the Free Software Foundation; either
# version 2 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Library General Public License for more details.
#
# You should have received a copy of the GNU Library General Public License
# along with this library; see the file COPYING.LIB.  If not, write to
# the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
# Boston, MA 02110-1301, USA.
#

use strict;
use warnings;

BEGIN {
   use Exporter   ();
   our ($VERSION, @ISA, @EXPORT, @EXPORT_OK, %EXPORT_TAGS);
   $VERSION     = 1.00;
   @ISA         = qw(Exporter);
   @EXPORT      = qw(&applyPreprocessor);
   %EXPORT_TAGS = ( );
   @EXPORT_OK   = ();
}

# Reads a file, strips comments, and evaluates simple #if / #else / #endif
# preprocessor directives without forking an external process.
# Returns an array of lines.
sub applyPreprocessor
{
    my $fileName = shift;
    my $defines = shift;
    my $keepComments = shift;
    my $stripLineComments = shift;

    open(my $fh, '<', $fileName) or die "Failed to open $fileName";
    local $/;
    my $content = <$fh>;
    close($fh);

    unless ($keepComments) {
        # Strip block comments.
        $content =~ s|/\*.*?\*/||gs;
        # Strip // line comments only when requested. CSS files use // in URLs
        # so callers processing CSS must not pass $stripLineComments. Double
        # quoted string literals may contain // (e.g. default values that are
        # URLs) and must be preserved.
        $content =~ s{("[^"\n]*")|//[^\n]*}{defined $1 ? $1 : ""}ge if $stripLineComments;
    }

    if ($content !~ /^\s*#/m) {
        return split(/^/m, $content);
    }

    # The file has preprocessor directives. Evaluate simple
    # #if defined(X) && X / #if !(defined(X) && X) / #ifdef / #elif / #else / #endif
    # patterns in Perl instead of forking clang.
    my %macros;
    if ($defines) {
        $macros{$_} = 1 for grep { $_ } split(/\s+/, $defines =~ s/"//gr);
    }
    return split(/^/m, _evaluatePreprocessorDirectives($content, \%macros, $fileName));
}

# Simple #if / #elif / #else / #endif evaluator. Any other condition is an error. Supports:
#   #ifdef X
#   #ifndef X
#   #if defined(X) && X
#   #if defined(X) / #if !defined(X)
#   #if X / #if !X
#   #if defined(X) && X && defined(Y) && Y  (conjunctions)
#   #if !(defined(X) && X)                  (negation)
#   #elif <any of the #if conditions above>
#   #else
#   #endif
#   #endif // comment
#   Nesting
sub _evaluatePreprocessorDirectives
{
    my ($content, $macros, $fileName) = @_;
    my $output = "";
    # Stack of [emittingBeforeThisIf, anyBranchWasTaken].
    my @ifStack = ();
    my $emitting = 1;

    for my $line (split(/^/m, $content)) {
        if ($line =~ /^\s*#\s*if(n?)def\s+(\w+)/) {
            my $cond = $emitting && $macros->{$2} ? 1 : 0;
            $cond = !$cond if $1 && $emitting;
            push @ifStack, [$emitting, $cond];
            $emitting = $emitting && $cond;
        } elsif ($line =~ /^\s*#\s*if\s+(.*)/) {
            # Like the C preprocessor, leave conditions inside an excluded block unevaluated.
            my $cond = $emitting ? _evaluateCondition(_stripTrailingComment($1), $macros, $fileName) : 0;
            push @ifStack, [$emitting, $cond];
            $emitting = $emitting && $cond;
        } elsif ($line =~ /^\s*#\s*elif\s+(.*)/) {
            my $frame = $ifStack[-1];
            if ($frame->[1] || !$frame->[0]) {
                $emitting = 0;
            } else {
                my $cond = _evaluateCondition(_stripTrailingComment($1), $macros, $fileName);
                $frame->[1] = $cond;
                $emitting = $cond;
            }
        } elsif ($line =~ /^\s*#\s*else\b/) {
            my $frame = $ifStack[-1];
            $emitting = $frame->[0] && !$frame->[1];
        } elsif ($line =~ /^\s*#\s*endif\b/) {
            my $frame = pop @ifStack;
            $emitting = $frame->[0];
        } else {
            $output .= $line if $emitting;
        }
    }
    return $output;
}

sub _stripTrailingComment
{
    my $expr = shift;
    $expr =~ s{\s*//.*$}{}s;
    return $expr;
}

sub _evaluateCondition
{
    my ($expr, $macros, $fileName) = @_;

    # Handle negation: "!(defined(X) && X)"
    if ($expr =~ /^\s*!\s*\((.+)\)\s*$/) {
        return _evaluateCondition($1, $macros, $fileName) ? 0 : 1;
    }

    # Handle parenthesized expression: "(defined(X) && X)"
    if ($expr =~ /^\s*\((.+)\)\s*$/) {
        return _evaluateCondition($1, $macros, $fileName);
    }

    # Handle conjunctions: "defined(X) && X && defined(Y) && Y"
    my @parts = split(/\s*&&\s*/, $expr);
    my $result = 1;
    for my $part (@parts) {
        $part =~ s/^\s+|\s+$//g;
        if ($part =~ /^(!?)\s*\(?defined\s*\(\s*(\w+)\s*\)\)?$/) {
            $result = 0 unless $1 ? !$macros->{$2} : $macros->{$2};
        } elsif ($part =~ /^(!?)\s*(\w+)$/) {
            $result = 0 unless $1 ? !$macros->{$2} : $macros->{$2};
        } else {
            die "$fileName: cannot evaluate '$part' in '$expr'; use only the forms listed above _evaluatePreprocessorDirectives in preprocessor.pm\n";
        }
    }
    return $result;
}

1;
