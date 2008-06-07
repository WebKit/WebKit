#                     Copyright 2007 David Cantrell
#                        All rights reserved.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of either:
#
#    a) the GNU General Public License as published by the Free
#    Software Foundation; either version 1, or (at your option) any
#    later version, or
#    b) the "Artistic License" which comes with this Kit.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See either
# the GNU General Public License or the Artistic License for more details.

# You should have received a copy of the Artistic License with this
# Kit, in the file named "Artistic".  If not, I'll be glad to provide one.

# You should also have received a copy of the GNU General Public License
# along with this program in the file named "Copying". If not, write to the
# Free Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
# 02111-1307, USA or visit their web page on the internet at
# http://www.gnu.org/copyleft/gpl.html.

# For those of you that choose to use the GNU General Public License,
# my interpretation of the GNU General Public License is that no Perl
# script falls under the terms of the GPL unless you explicitly put
# said script under the terms of the GPL yourself.  Furthermore, any
# object code linked with perl does not automatically fall under the
# terms of the GPL, provided such object code only adds definitions
# of subroutines and variables, and does not otherwise impair the
# resulting interpreter from executing any standard Perl script.  I
# consider linking in C subroutines in this manner to be the moral
# equivalent of defining subroutines in the Perl language itself.  You
# may sell such an object file as proprietary provided that you provide  
# or offer to provide the Perl source, as specified by the GNU General
# Public License.  (This is merely an alternate way of specifying input
# to the program.)  You may also sell a binary produced by the dumping of
# a running Perl script that belongs to you, provided that you provide or
# offer to provide the Perl source as specified by the GPL.  (The
# fact that a Perl interpreter and your code are in the same binary file
# is, in this case, a form of mere aggregation.)  This is my interpretation
# of the GPL.  If you still have concerns or difficulties understanding
# my intent, feel free to contact me.  Of course, the Artistic License
# spells all this out for your protection, so you may prefer to use that.

# Original package name XML::Tiny (http://search.cpan.org/~dcantrell/XML-Tiny/)
package XMLTiny;

use strict;

require Exporter;

use vars qw($VERSION @EXPORT_OK @ISA);

$VERSION = '1.11';
@EXPORT_OK = qw(parsefile);
@ISA = qw(Exporter);

# localising prevents the warningness leaking out of this module
local $^W = 1;    # can't use warnings as that's a 5.6-ism

my %regexps = (
    name => '[:a-z][\\w:\\.-]*'
);

my $strict_entity_parsing; # mmm, global. don't worry, parsefile sets it
                           # explicitly every time
sub parsefile {
    my($arg, %params) = @_;
    my($file, $elem) = ('', { content => [] });
    local $/; # sluuuuurp

    $strict_entity_parsing = $params{strict_entity_parsing};

    if(ref($arg) eq '') { # we were passed a filename or a string
        if($arg =~ /^_TINY_XML_STRING_(.*)/) { # it's a string
            $file = $1;
        } else {
            local *FH;
            open(FH, $arg) || die(__PACKAGE__."::parsefile: Can't open $arg\n");
            $file = <FH>;
            close(FH);
        }
    } else { $file = <$arg>; }
    die("No elements\n") if (!defined($file) || $file =~ /^\s*$/);

    # illegal low-ASCII chars
    die("Not well-formed\n") if($file =~ /[\x00-\x08\x0b\x0c\x0e-\x1f]/);

    # turn CDATA into PCDATA
    $file =~ s{<!\[CDATA\[(.*?)]]>}{
        $_ = $1.chr(0);          # this makes sure that empty CDATAs become
        s/([&<>])/               # the empty string and aren't just thrown away.
            $1 eq '&' ? '&amp;' :
            $1 eq '<' ? '&lt;'  :
                        '&gt;'
        /eg;
        $_;
    }egs;

    die("Not well-formed\n") if(
        $file =~ /]]>/ ||                          # ]]> not delimiting CDATA
    $file =~ /<!--(.*?)--->/s ||               # ---> can't end a comment
    grep { $_ && /--/ } ($file =~ /^\s+|<!--(.*?)-->|\s+$/gs) # -- in comm
    );

    # strip leading/trailing whitespace and comments (which don't nest - phew!)
    $file =~ s/^\s+|<!--(.*?)-->|\s+$//gs;
    
    # turn quoted > in attribs into &gt;
    # double- and single-quoted attrib values get done seperately
    while($file =~ s/($regexps{name}\s*=\s*"[^"]*)>([^"]*")/$1&gt;$2/gsi) {}
    while($file =~ s/($regexps{name}\s*=\s*'[^']*)>([^']*')/$1&gt;$2/gsi) {}

    if($params{fatal_declarations} && $file =~ /<!(ENTITY|DOCTYPE)/) {
        die("I can't handle this document\n");
    }

    # ignore empty tokens/whitespace tokens
    foreach my $token (grep { length && $_ !~ /^\s+$/ }
      split(/(<[^>]+>)/, $file)) {
        if(
        $token =~ /<\?$regexps{name}.*?\?>/is ||  # PI
        $token =~ /^<!(ENTITY|DOCTYPE)/i          # entity/doctype decl
    ) {
        next;
        } elsif($token =~ m!^</($regexps{name})\s*>!i) {     # close tag
        die("Not well-formed\n\tat $token\n") if($elem->{name} ne $1);
        $elem = delete $elem->{parent};
        } elsif($token =~ /^<$regexps{name}(\s[^>]*)*(\s*\/)?>/is) {   # open tag
        my($tagname, $attribs_raw) = ($token =~ m!<(\S*)(.*?)(\s*/)?>!s);
        # first make attribs into a list so we can spot duplicate keys
        my $attrib  = [
            # do double- and single- quoted attribs seperately
            $attribs_raw =~ /\s($regexps{name})\s*=\s*"([^"]*?)"/gi,
            $attribs_raw =~ /\s($regexps{name})\s*=\s*'([^']*?)'/gi
        ];
        if(@{$attrib} == 2 * keys %{{@{$attrib}}}) {
            $attrib = { @{$attrib} }
        } else { die("Not well-formed - duplicate attribute\n"); }
        
        # now trash any attribs that we *did* manage to parse and see
        # if there's anything left
        $attribs_raw =~ s/\s($regexps{name})\s*=\s*"([^"]*?)"//gi;
        $attribs_raw =~ s/\s($regexps{name})\s*=\s*'([^']*?)'//gi;
        die("Not well-formed\n$attribs_raw") if($attribs_raw =~ /\S/ || grep { /</ } values %{$attrib});

        unless($params{no_entity_parsing}) {
            foreach my $key (keys %{$attrib}) {
                $attrib->{$key} = _fixentities($attrib->{$key})
                }
            }
        $elem = {
                content => [],
                name => $tagname,
                type => 'e',
                attrib => $attrib,
                parent => $elem
            };
        push @{$elem->{parent}->{content}}, $elem;
        # now handle self-closing tags
            if($token =~ /\s*\/>$/) {
                $elem->{name} =~ s/\/$//;
            $elem = delete $elem->{parent};
            }
        } elsif($token =~ /^</) { # some token taggish thing
            die("I can't handle this document\n\tat $token\n");
        } else {                          # ordinary content
        $token =~ s/\x00//g; # get rid of our CDATA marker
            unless($params{no_entity_parsing}) { $token = _fixentities($token); }
            push @{$elem->{content}}, { content => $token, type => 't' };
        }
    }
    die("Not well-formed\n") if(exists($elem->{parent}));
    die("Junk after end of document\n") if($#{$elem->{content}} > 0);
    die("No elements\n") if(
        $#{$elem->{content}} == -1 || $elem->{content}->[0]->{type} ne 'e'
    );
    return $elem->{content};
}

sub _fixentities {
    my $thingy = shift;

    my $junk = ($strict_entity_parsing) ? '|.*' : '';
    $thingy =~ s/&((#(\d+|x[a-fA-F0-9]+);)|lt;|gt;|quot;|apos;|amp;$junk)/
        $3 ? (
        substr($3, 0, 1) eq 'x' ?     # using a =~ match here clobbers $3
            chr(hex(substr($3, 1))) : # so don't "fix" it!
        chr($3)
    ) :
        $1 eq 'lt;'   ? '<' :
        $1 eq 'gt;'   ? '>' :
        $1 eq 'apos;' ? "'" :
        $1 eq 'quot;' ? '"' :
        $1 eq 'amp;'  ? '&' :
                        die("Illegal ampersand or entity\n\tat $1\n")
    /ge;
    $thingy;
}

1;
