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
        # so callers processing CSS must not pass $stripLineComments.
        $content =~ s|//[^\n]*||g if $stripLineComments;
    }

    if ($content !~ /^\s*#/m) {
        return split(/^/m, $content);
    }

    # The file has preprocessor directives. Evaluate simple
    # #if defined(X) && X / #if !(defined(X) && X) / #else / #endif
    # patterns in Perl instead of forking clang.
    my %macros;
    if ($defines) {
        $macros{$_} = 1 for grep { $_ } split(/\s+/, $defines =~ s/"//gr);
    }
    return split(/^/m, _evaluatePreprocessorDirectives($content, \%macros));
}

# Simple #if / #else / #endif evaluator. Supports:
#   #if defined(X) && X
#   #if defined(X)
#   #if defined(X) && X && defined(Y) && Y  (conjunctions)
#   #if !(defined(X) && X)                  (negation)
#   #else
#   #endif
#   #endif // comment
#   Nesting
sub _evaluatePreprocessorDirectives
{
    my ($content, $macros) = @_;
    my $output = "";
    # Stack of [emittingBeforeThisIf, conditionWasTrue].
    my @ifStack = ();
    my $emitting = 1;

    for my $line (split(/^/m, $content)) {
        if ($line =~ /^\s*#\s*if\s+(.*)/) {
            my $cond = _evaluateCondition($1, $macros);
            push @ifStack, [$emitting, $cond];
            $emitting = $emitting && $cond;
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

sub _evaluateCondition
{
    my ($expr, $macros) = @_;

    # Handle negation: "!(defined(X) && X)"
    if ($expr =~ /^\s*!\s*\((.+)\)\s*$/) {
        return _evaluateCondition($1, $macros) ? 0 : 1;
    }

    # Handle parenthesized expression: "(defined(X) && X)"
    if ($expr =~ /^\s*\((.+)\)\s*$/) {
        return _evaluateCondition($1, $macros);
    }

    # Handle conjunctions: "defined(X) && X && defined(Y) && Y"
    my @parts = split(/\s*&&\s*/, $expr);
    for my $part (@parts) {
        $part =~ s/^\s+|\s+$//g;
        if ($part =~ /^\(?defined\((\w+)\)\)?$/) {
            return 0 unless $macros->{$1};
        } elsif ($part =~ /^(\w+)$/) {
            return 0 unless $macros->{$1};
        } else {
            # Unrecognized expression — conservatively treat as true.
            next;
        }
    }
    return 1;
}

1;
