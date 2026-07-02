package ExtUtils::MakeMaker::Locale;

use strict;
our $VERSION = "7.32";
$VERSION = eval $VERSION;

use base 'Exporter';
our @EXPORT_OK = qw(
    decode_argv env
    $ENCODING_LOCALE $ENCODING_LOCALE_FS
    $ENCODING_CONSOLE_IN $ENCODING_CONSOLE_OUT
);

use Encode ();
use Encode::Alias ();

our $ENCODING_LOCALE;
our $ENCODING_LOCALE_FS;
our $ENCODING_CONSOLE_IN;
our $ENCODING_CONSOLE_OUT;

sub DEBUG () { 0 }

sub _init {
    if ($^O eq "MSWin32") {
    unless ($ENCODING_LOCALE) {
        # Try to obtain what the Windows ANSI code page is
        eval {
        unless (defined &GetConsoleCP) {
            require Win32;
                    # manually "import" it since Win32->import refuses
            *GetConsoleCP = sub { &Win32::GetConsoleCP } if defined &Win32::GetConsoleCP;
        }
        unless (defined &GetConsoleCP) {
            require Win32::API;
            Win32::API->Import('kernel32', 'int GetConsoleCP()');
        }
        if (defined &GetConsoleCP) {
            my $cp = GetConsoleCP();
            $ENCODING_LOCALE = "cp$cp" if $cp;
        }
        };
    }

    unless ($ENCODING_CONSOLE_IN) {
            # only test one since set together
            unless (defined &GetInputCP) {
                eval {
                    require Win32;
                    eval { Win32::GetConsoleCP() };
                    # manually "import" it since Win32->import refuses
                    *GetInputCP = sub { &Win32::GetConsoleCP } if defined &Win32::GetConsoleCP;
                    *GetOutputCP = sub { &Win32::GetConsoleOutputCP } if defined &Win32::GetConsoleOutputCP;
                };
                unless (defined &GetInputCP) {
                    eval {
                        # try Win32::Console module for codepage to use
                        require Win32::Console;
                        *GetInputCP = sub { &Win32::Console::InputCP }
                            if defined &Win32::Console::InputCP;
                        *GetOutputCP = sub { &Win32::Console::OutputCP }
                            if defined &Win32::Console::OutputCP;
                    };
                }
                unless (defined &GetInputCP) {
                    # final fallback
                    *GetInputCP = *GetOutputCP = sub {
                        # another fallback that could work is:
                        # reg query HKLM\System\CurrentControlSet\Control\Nls\CodePage /v ACP
                        ((qx(chcp) || '') =~ /^Active code page: (\d+)/)
                            ? $1 : ();
                    };
                }
        }
            my $cp = GetInputCP();
            $ENCODING_CONSOLE_IN = "cp$cp" if $cp;
            $cp = GetOutputCP();
            $ENCODING_CONSOLE_OUT = "cp$cp" if $cp;
    }
    }

    unless ($ENCODING_LOCALE) {
    eval {
        require I18N::Langinfo;
        $ENCODING_LOCALE = I18N::Langinfo::langinfo(I18N::Langinfo::CODESET());

        # Workaround of Encode < v2.25.  The "646" encoding  alias was
        # introduced in Encode-2.25, but we don't want to require that version
        # quite yet.  Should avoid the CPAN testers failure reported from
        # openbsd-4.7/perl-5.10.0 combo.
        $ENCODING_LOCALE = "ascii" if $ENCODING_LOCALE eq "646";

        # https://rt.cpan.org/Ticket/Display.html?id=66373
        $ENCODING_LOCALE = "hp-roman8" if $^O eq "hpux" && $ENCODING_LOCALE eq "roman8";
    };
    $ENCODING_LOCALE ||= $ENCODING_CONSOLE_IN;
    }

    if ($^O eq "darwin") {
    $ENCODING_LOCALE_FS ||= "UTF-8";
    }

    # final fallback
    $ENCODING_LOCALE ||= $^O eq "MSWin32" ? "cp1252" : "UTF-8";
    $ENCODING_LOCALE_FS ||= $ENCODING_LOCALE;
    $ENCODING_CONSOLE_IN ||= $ENCODING_LOCALE;
    $ENCODING_CONSOLE_OUT ||= $ENCODING_CONSOLE_IN;

    unless (Encode::find_encoding($ENCODING_LOCALE)) {
    my $foundit;
    if (lc($ENCODING_LOCALE) eq "gb18030") {
        eval {
        require Encode::HanExtra;
        };
        if ($@) {
        die "Need Encode::HanExtra to be installed to support locale codeset ($ENCODING_LOCALE), stopped";
        }
        $foundit++ if Encode::find_encoding($ENCODING_LOCALE);
    }
    die "The locale codeset ($ENCODING_LOCALE) isn't one that perl can decode, stopped"
        unless $foundit;

    }

    # use Data::Dump; ddx $ENCODING_LOCALE, $ENCODING_LOCALE_FS, $ENCODING_CONSOLE_IN, $ENCODING_CONSOLE_OUT;
}

_init();
Encode::Alias::define_alias(sub {
    no strict 'refs';
    no warnings 'once';
    return ${"ENCODING_" . uc(shift)};
}, "locale");

sub _flush_aliases {
    no strict 'refs';
    for my $a (sort keys %Encode::Alias::Alias) {
    if (defined ${"ENCODING_" . uc($a)}) {
        delete $Encode::Alias::Alias{$a};
        warn "Flushed alias cache for $a" if DEBUG;
    }
    }
}

sub reinit {
    $ENCODING_LOCALE = shift;
    $ENCODING_LOCALE_FS = shift;
    $ENCODING_CONSOLE_IN = $ENCODING_LOCALE;
    $ENCODING_CONSOLE_OUT = $ENCODING_LOCALE;
    _init();
    _flush_aliases();
}

sub decode_argv {
    die if defined wantarray;
    for (@ARGV) {
    $_ = Encode::decode(locale => $_, @_);
    }
}

sub env {
    my $k = Encode::encode(locale => shift);
    my $old = $ENV{$k};
    if (@_) {
    my $v = shift;
    if (defined $v) {
        $ENV{$k} = Encode::encode(locale => $v);
    }
    else {
        delete $ENV{$k};
    }
    }
    return Encode::decode(locale => $old) if defined wantarray;
}

1;

__END__

=head1 NAME

ExtUtils::MakeMaker::Locale - bundled Encode::Locale

=head1 SYNOPSIS

  use Encode::Locale;
  use Encode;

  $string = decode(locale => $bytes);
  $bytes = encode(locale => $string);

  if (-t) {
      binmode(STDIN, ":encoding(console_in)");
      binmode(STDOUT, ":encoding(console_out)");
      binmode(STDERR, ":encoding(console_out)");
  }

  # Processing file names passed in as arguments
  my $uni_filename = decode(locale => $ARGV[0]);
  open(my $fh, "<", encode(locale_fs => $uni_filename))
     || die "Can't open '$uni_filename': $!";
  binmode($fh, ":encoding(locale)");
  ...

=head1 DESCRIPTION

In many applications it's wise to let Perl use Unicode for the strings it
processes.  Most of the interfaces Perl has to the outside world are still byte
based.  Programs therefore need to decode byte strings that enter the program
from the outside and encode them again on the way out.

The POSIX locale system is used to specify both the language conventions
requested by the user and the preferred character set to consume and
output.  The C<Encode::Locale> module looks up the charset and encoding (called
a CODESET in the locale jargon) and arranges for the L<Encode> module to know
this encoding under the name "locale".  It means bytes obtained from the
environment can be converted to Unicode strings by calling C<<
Encode::encode(locale => $bytes) >> and converted back again with C<<
Encode::decode(locale => $string) >>.

Where file systems interfaces pass file names in and out of the program we also
need care.  The trend is for operating systems to use a fixed file encoding
that don't actually depend on the locale; and this module determines the most
appropriate encoding for file names. The L<Encode> module will know this
encoding under the name "locale_fs".  For traditional Unix systems this will
be an alias to the same encoding as "locale".

For programs running in a terminal window (called a "Console" on some systems)
the "locale" encoding is usually a good choice for what to expect as input and
output.  Some systems allows us to query the encoding set for the terminal and
C<Encode::Locale> will do that if available and make these encodings known
under the C<Encode> aliases "console_in" and "console_out".  For systems where
we can't determine the terminal encoding these will be aliased as the same
encoding as "locale".  The advice is to use "console_in" for input known to
come from the terminal and "console_out" for output to the terminal.

In addition to arranging for various Encode aliases the following functions and
variables are provided:

=over

=item decode_argv( )

=item decode_argv( Encode::FB_CROAK )

This will decode the command line arguments to perl (the C<@ARGV> array) in-place.

The function will by default replace characters that can't be decoded by
"\x{FFFD}", the Unicode replacement character.

Any argument provided is passed as CHECK to underlying Encode::decode() call.
Pass the value C<Encode::FB_CROAK> to have the decoding croak if not all the
command line arguments can be decoded.  See L<Encode/"Handling Malformed Data">
for details on other options for CHECK.

=item env( $uni_key )

=item env( $uni_key => $uni_value )

Interface to get/set environment variables.  Returns the current value as a
Unicode string. The $uni_key and $uni_value arguments are expected to be
Unicode strings as well.  Passing C<undef> as $uni_value deletes the
environment variable named $uni_key.

The returned value will have the characters that can't be decoded replaced by
"\x{FFFD}", the Unicode replacement character.

There is no interface to request alternative CHECK behavior as for
decode_argv().  If you need that you need to call encode/decode yourself.
For example:

    my $key = Encode::encode(locale => $uni_key, Encode::FB_CROAK);
    my $uni_value = Encode::decode(locale => $ENV{$key}, Encode::FB_CROAK);

=item reinit( )

=item reinit( $encoding )

Reinitialize the encodings from the locale.  You want to call this function if
you changed anything in the environment that might influence the locale.

This function will croak if the determined encoding isn't recognized by
the Encode module.

With argument force $ENCODING_... variables to set to the given value.

=item $ENCODING_LOCALE

The encoding name determined to be suitable for the current locale.
L<Encode> know this encoding as "locale".

=item $ENCODING_LOCALE_FS

The encoding name determined to be suitable for file system interfaces
involving file names.
L<Encode> know this encoding as "locale_fs".

=item $ENCODING_CONSOLE_IN

=item $ENCODING_CONSOLE_OUT

The encodings to be used for reading and writing output to the a console.
L<Encode> know these encodings as "console_in" and "console_out".

=back

=head1 NOTES

This table summarizes the mapping of the encodings set up
by the C<Encode::Locale> module:

  Encode      |         |              |
  Alias       | Windows | Mac OS X     | POSIX
  ------------+---------+--------------+------------
  locale      | ANSI    | nl_langinfo  | nl_langinfo
  locale_fs   | ANSI    | UTF-8        | nl_langinfo
  console_in  | OEM     | nl_langinfo  | nl_langinfo
  console_out | OEM     | nl_langinfo  | nl_langinfo

=head2 Windows

Windows has basically 2 sets of APIs.  A wide API (based on passing UTF-16
strings) and a byte based API based a character set called ANSI.  The
regular Perl interfaces to the OS currently only uses the ANSI APIs.
Unfortunately ANSI is not a single character set.

The encoding that corresponds to ANSI varies between different editions of
Windows.  For many western editions of Windows ANSI corresponds to CP-1252
which is a character set similar to ISO-8859-1.  Conceptually the ANSI
character set is a similar concept to the POSIX locale CODESET so this module
figures out what the ANSI code page is and make this available as
$ENCODING_LOCALE and the "locale" Encoding alias.

Windows systems also operate with another byte based character set.
It's called the OEM code page.  This is the encoding that the Console
takes as input and output.  It's common for the OEM code page to
differ from the ANSI code page.

=head2 Mac OS X

On Mac OS X the file system encoding is always UTF-8 while the locale
can otherwise be set up as normal for POSIX systems.

File names on Mac OS X will at the OS-level be converted to
NFD-form.  A file created by passing a NFC-filename will come
in NFD-form from readdir().  See L<Unicode::Normalize> for details
of NFD/NFC.

Actually, Apple does not follow the Unicode NFD standard since not all
character ranges are decomposed.  The claim is that this avoids problems with
round trip conversions from old Mac text encodings.  See L<Encode::UTF8Mac> for
details.

=head2 POSIX (Linux and other Unixes)

File systems might vary in what encoding is to be used for
filenames.  Since this module has no way to actually figure out
what the is correct it goes with the best guess which is to
assume filenames are encoding according to the current locale.
Users are advised to always specify UTF-8 as the locale charset.

=head1 SEE ALSO

L<I18N::Langinfo>, L<Encode>, L<Term::Encoding>

=head1 AUTHOR

Copyright 2010 Gisle Aas <gisle@aas.no>.

This library is free software; you can redistribute it and/or
modify it under the same terms as Perl itself.

=cut
