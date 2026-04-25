# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

package Bugzilla::UserAgent;

use 5.10.1;
use strict;
use warnings;

use parent qw(Exporter);
our @EXPORT = qw(detect_platform detect_op_sys);

use Bugzilla::Field;
use List::MoreUtils qw(natatime);

use constant DEFAULT_VALUE => 'Other';

use constant PLATFORMS_MAP => (
    # PowerPC
    qr/\(.*PowerPC.*\)/i => ["PowerPC", "Macintosh"],
    # AMD64, Intel x86_64
    qr/\(.*[ix0-9]86 (?:on |\()x86_64.*\)/ => ["IA32", "x86", "PC"],
    qr/\(.*amd64.*\)/ => ["AMD64", "x86_64", "PC"],
    qr/\(.*x86_64.*\)/ => ["AMD64", "x86_64", "PC"],
    # Intel IA64
    qr/\(.*IA64.*\)/ => ["IA64", "PC"],
    # Intel x86
    qr/\(.*Intel.*\)/ => ["IA32", "x86", "PC"],
    qr/\(.*[ix0-9]86.*\)/ => ["IA32", "x86", "PC"],
    # Versions of Windows that only run on Intel x86
    qr/\(.*Win(?:dows |)[39M].*\)/ => ["IA32", "x86", "PC"],
    qr/\(.*Win(?:dows |)16.*\)/ => ["IA32", "x86", "PC"],
    # Sparc
    qr/\(.*sparc.*\)/ => ["Sparc", "Sun"],
    qr/\(.*sun4.*\)/ => ["Sparc", "Sun"],
    # Alpha
    qr/\(.*AXP.*\)/i => ["Alpha", "DEC"],
    qr/\(.*[ _]Alpha.\D/i => ["Alpha", "DEC"],
    qr/\(.*[ _]Alpha\)/i => ["Alpha", "DEC"],
    # MIPS
    qr/\(.*IRIX.*\)/i => ["MIPS", "SGI"],
    qr/\(.*MIPS.*\)/i => ["MIPS", "SGI"],
    # 68k
    qr/\(.*68K.*\)/ => ["68k", "Macintosh"],
    qr/\(.*680[x0]0.*\)/ => ["68k", "Macintosh"],
    # HP
    qr/\(.*9000.*\)/ => ["PA-RISC", "HP"],
    # ARM
    qr/\(.*(?:iPod|iPad|iPhone).*\)/ => ["ARM"],
    qr/\(.*ARM.*\)/ => ["ARM", "PocketPC"],
    # PocketPC intentionally before PowerPC
    qr/\(.*Windows CE.*PPC.*\)/ => ["ARM", "PocketPC"],
    # PowerPC
    qr/\(.*PPC.*\)/ => ["PowerPC", "Macintosh"],
    qr/\(.*AIX.*\)/ => ["PowerPC", "Macintosh"],
    # Stereotypical and broken
    qr/\(.*Windows CE.*\)/ => ["ARM", "PocketPC"],
    qr/\(.*Macintosh.*\)/ => ["68k", "Macintosh"],
    qr/\(.*Mac OS [89].*\)/ => ["68k", "Macintosh"],
    qr/\(.*WOW64.*\)/ => ["x86_64"],
    qr/\(.*Win64.*\)/ => ["IA64"],
    qr/\(Win.*\)/ => ["IA32", "x86", "PC"],
    qr/\(.*Win(?:dows[ -])NT.*\)/ => ["IA32", "x86", "PC"],
    qr/\(.*OSF.*\)/ => ["Alpha", "DEC"],
    qr/\(.*HP-?UX.*\)/i => ["PA-RISC", "HP"],
    qr/\(.*IRIX.*\)/i => ["MIPS", "SGI"],
    qr/\(.*(SunOS|Solaris).*\)/ => ["Sparc", "Sun"],
    # Braindead old browsers who didn't follow convention:
    qr/Amiga/ => ["68k", "Macintosh"],
    qr/WinMosaic/ => ["IA32", "x86", "PC"],
);

use constant OS_MAP => (
    # Sun
    qr/\(.*Solaris.*\)/ => ["Solaris"],
    qr/\(.*SunOS 5.11.*\)/ => [("OpenSolaris", "Opensolaris", "Solaris 11")],
    qr/\(.*SunOS 5.10.*\)/ => ["Solaris 10"],
    qr/\(.*SunOS 5.9.*\)/ => ["Solaris 9"],
    qr/\(.*SunOS 5.8.*\)/ => ["Solaris 8"],
    qr/\(.*SunOS 5.7.*\)/ => ["Solaris 7"],
    qr/\(.*SunOS 5.6.*\)/ => ["Solaris 6"],
    qr/\(.*SunOS 5.5.*\)/ => ["Solaris 5"],
    qr/\(.*SunOS 5.*\)/ => ["Solaris"],
    qr/\(.*SunOS.*sun4u.*\)/ => ["Solaris"],
    qr/\(.*SunOS.*i86pc.*\)/ => ["Solaris"],
    qr/\(.*SunOS.*\)/ => ["SunOS"],
    # BSD
    qr/\(.*BSD\/(?:OS|386).*\)/ => ["BSDI"],
    qr/\(.*FreeBSD.*\)/ => ["FreeBSD"],
    qr/\(.*OpenBSD.*\)/ => ["OpenBSD"],
    qr/\(.*NetBSD.*\)/ => ["NetBSD"],
    # Misc POSIX
    qr/\(.*IRIX.*\)/ => ["IRIX"],
    qr/\(.*OSF.*\)/ => ["OSF/1"],
    qr/\(.*Linux.*\)/ => ["Linux"],
    qr/\(.*BeOS.*\)/ => ["BeOS"],
    qr/\(.*AIX.*\)/ => ["AIX"],
    qr/\(.*OS\/2.*\)/ => ["OS/2"],
    qr/\(.*QNX.*\)/ => ["Neutrino"],
    qr/\(.*VMS.*\)/ => ["OpenVMS"],
    qr/\(.*HP-?UX.*\)/ => ["HP-UX"],
    qr/\(.*Android.*\)/ => ["Android"],
    # Windows
    qr/\(.*Windows XP.*\)/ => ["Windows XP"],
    qr/\(.*Windows NT 10\.0.*\)/ => ["Windows 10"],
    qr/\(.*Windows NT 6\.4.*\)/ => ["Windows 10"],
    qr/\(.*Windows NT 6\.3.*\)/ => ["Windows 8.1"],
    qr/\(.*Windows NT 6\.2.*\)/ => ["Windows 8"],
    qr/\(.*Windows NT 6\.1.*\)/ => ["Windows 7"],
    qr/\(.*Windows NT 6\.0.*\)/ => ["Windows Vista"],
    qr/\(.*Windows NT 5\.2.*\)/ => ["Windows Server 2003"],
    qr/\(.*Windows NT 5\.1.*\)/ => ["Windows XP"],
    qr/\(.*Windows 2000.*\)/ => ["Windows 2000"],
    qr/\(.*Windows NT 5.*\)/ => ["Windows 2000"],
    qr/\(.*Win.*9[8x].*4\.9.*\)/ => ["Windows ME"],
    qr/\(.*Win(?:dows |)M[Ee].*\)/ => ["Windows ME"],
    qr/\(.*Win(?:dows |)98.*\)/ => ["Windows 98"],
    qr/\(.*Win(?:dows |)95.*\)/ => ["Windows 95"],
    qr/\(.*Win(?:dows |)16.*\)/ => ["Windows 3.1"],
    qr/\(.*Win(?:dows[ -]|)NT.*\)/ => ["Windows NT"],
    qr/\(.*Windows.*NT.*\)/ => ["Windows NT"],
    # OS X
    qr/\(.*(?:iPad|iPhone).*OS 7.*\)/ => ["iOS 7"],
    qr/\(.*(?:iPad|iPhone).*OS 6.*\)/ => ["iOS 6"],
    qr/\(.*(?:iPad|iPhone).*OS 5.*\)/ => ["iOS 5"],
    qr/\(.*(?:iPad|iPhone).*OS 4.*\)/ => ["iOS 4"],
    qr/\(.*(?:iPad|iPhone).*OS 3.*\)/ => ["iOS 3"],
    qr/\(.*(?:iPod|iPad|iPhone).*\)/ => ["iOS"],
    qr/\(.*Mac OS X (?:|Mach-O |\()10.8.*\)/ => ["Mac OS X 10.8"],
    qr/\(.*Mac OS X (?:|Mach-O |\()10.7.*\)/ => ["Mac OS X 10.7"],
    qr/\(.*Mac OS X (?:|Mach-O |\()10.6.*\)/ => ["Mac OS X 10.6"],
    qr/\(.*Mac OS X (?:|Mach-O |\()10.5.*\)/ => ["Mac OS X 10.5"],
    qr/\(.*Mac OS X (?:|Mach-O |\()10.4.*\)/ => ["Mac OS X 10.4"],
    qr/\(.*Mac OS X (?:|Mach-O |\()10.3.*\)/ => ["Mac OS X 10.3"],
    qr/\(.*Mac OS X (?:|Mach-O |\()10.2.*\)/ => ["Mac OS X 10.2"],
    qr/\(.*Mac OS X (?:|Mach-O |\()10.1.*\)/ => ["Mac OS X 10.1"],
    # Unfortunately, OS X 10.4 was the first to support Intel. This is fallback
    # support because some browsers refused to include the OS Version.
    qr/\(.*Intel.*Mac OS X.*\)/ => ["Mac OS X 10.4"],
    # OS X 10.3 is the most likely default version of PowerPC Macs
    # OS X 10.0 is more for configurations which didn't setup 10.x versions
    qr/\(.*Mac OS X.*\)/ => [("Mac OS X 10.3", "Mac OS X 10.0", "Mac OS X")],
    qr/\(.*Mac OS 9.*\)/ => [("Mac System 9.x", "Mac System 9.0")],
    qr/\(.*Mac OS 8\.6.*\)/ => [("Mac System 8.6", "Mac System 8.5")],
    qr/\(.*Mac OS 8\.5.*\)/ => ["Mac System 8.5"],
    qr/\(.*Mac OS 8\.1.*\)/ => [("Mac System 8.1", "Mac System 8.0")],
    qr/\(.*Mac OS 8\.0.*\)/ => ["Mac System 8.0"],
    qr/\(.*Mac OS 8[^.].*\)/ => ["Mac System 8.0"],
    qr/\(.*Mac OS 8.*\)/ => ["Mac System 8.6"],
    qr/\(.*Darwin.*\)/ => [("Mac OS X 10.0", "Mac OS X")],
    # Silly
    qr/\(.*Mac.*PowerPC.*\)/ => ["Mac System 9.x"],
    qr/\(.*Mac.*PPC.*\)/ => ["Mac System 9.x"],
    qr/\(.*Mac.*68k.*\)/ => ["Mac System 8.0"],
    # Evil
    qr/Amiga/i => ["Other"],
    qr/WinMosaic/ => ["Windows 95"],
    qr/\(.*32bit.*\)/ => ["Windows 95"],
    qr/\(.*16bit.*\)/ => ["Windows 3.1"],
    qr/\(.*PowerPC.*\)/ => ["Mac System 9.x"],
    qr/\(.*PPC.*\)/ => ["Mac System 9.x"],
    qr/\(.*68K.*\)/ => ["Mac System 8.0"],
);

sub detect_platform {
    my $userAgent = $ENV{'HTTP_USER_AGENT'};
    my @detected;
    my $iterator = natatime(2, PLATFORMS_MAP);
    while (my($re, $ra) = $iterator->()) {
        if ($userAgent =~ $re) {
            push @detected, @$ra;
        }
    }
    return _pick_valid_field_value('rep_platform', @detected);
}

sub detect_op_sys {
    my $userAgent = $ENV{'HTTP_USER_AGENT'} || '';
    my @detected;
    my $iterator = natatime(2, OS_MAP);
    while (my($re, $ra) = $iterator->()) {
        if ($userAgent =~ $re) {
            push @detected, @$ra;
        }
    }
    push(@detected, "Windows") if grep(/^Windows /, @detected);
    push(@detected, "Mac OS") if grep(/^Mac /, @detected);
    return _pick_valid_field_value('op_sys', @detected);
}

# Takes the name of a field and a list of possible values for that field.
# Returns the first value in the list that is actually a valid value for that
# field.
# Returns 'Other' if none of the values match.
sub _pick_valid_field_value {
    my ($field, @values) = @_;
    foreach my $value (@values) {
        return $value if check_field($field, $value, undef, 1);
    }
    return DEFAULT_VALUE;
}

1;

__END__

=head1 NAME

Bugzilla::UserAgent - UserAgent utilities for Bugzilla

=head1 SYNOPSIS

  use Bugzilla::UserAgent;
  printf "platform: %s op-sys: %s\n", detect_platform(), detect_op_sys();

=head1 DESCRIPTION

The functions exported by this module all return information derived from the
remote client's user agent.

=head1 FUNCTIONS

=over 4

=item C<detect_platform>

This function attempts to detect the remote client's platform from the
presented user-agent. If a suitable value on the I<platform> field is found,
that field value will be returned.  If no suitable value is detected,
C<detect_platform> returns I<Other>.

=item C<detect_op_sys>

This function attempts to detect the remote client's operating system from the
presented user-agent. If a suitable value on the I<op_sys> field is found, that
field value will be returned.  If no suitable value is detected,
C<detect_op_sys> returns I<Other>.

=back
