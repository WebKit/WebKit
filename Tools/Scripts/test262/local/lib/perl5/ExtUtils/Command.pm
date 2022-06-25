package ExtUtils::Command;

use 5.00503;
use strict;
require Exporter;
use vars qw(@ISA @EXPORT @EXPORT_OK $VERSION);
@ISA       = qw(Exporter);
@EXPORT    = qw(cp rm_f rm_rf mv cat eqtime mkpath touch test_f test_d chmod
                dos2unix);
$VERSION = '7.32';
$VERSION = eval $VERSION;

my $Is_VMS   = $^O eq 'VMS';
my $Is_VMS_mode = $Is_VMS;
my $Is_VMS_noefs = $Is_VMS;
my $Is_Win32 = $^O eq 'MSWin32';

if( $Is_VMS ) {
    my $vms_unix_rpt;
    my $vms_efs;
    my $vms_case;

    if (eval { local $SIG{__DIE__};
               local @INC = @INC;
               pop @INC if $INC[-1] eq '.';
               require VMS::Feature; }) {
        $vms_unix_rpt = VMS::Feature::current("filename_unix_report");
        $vms_efs = VMS::Feature::current("efs_charset");
        $vms_case = VMS::Feature::current("efs_case_preserve");
    } else {
        my $unix_rpt = $ENV{'DECC$FILENAME_UNIX_REPORT'} || '';
        my $efs_charset = $ENV{'DECC$EFS_CHARSET'} || '';
        my $efs_case = $ENV{'DECC$EFS_CASE_PRESERVE'} || '';
        $vms_unix_rpt = $unix_rpt =~ /^[ET1]/i;
        $vms_efs = $efs_charset =~ /^[ET1]/i;
        $vms_case = $efs_case =~ /^[ET1]/i;
    }
    $Is_VMS_mode = 0 if $vms_unix_rpt;
    $Is_VMS_noefs = 0 if ($vms_efs);
}


=head1 NAME

ExtUtils::Command - utilities to replace common UNIX commands in Makefiles etc.

=head1 SYNOPSIS

  perl -MExtUtils::Command -e cat files... > destination
  perl -MExtUtils::Command -e mv source... destination
  perl -MExtUtils::Command -e cp source... destination
  perl -MExtUtils::Command -e touch files...
  perl -MExtUtils::Command -e rm_f files...
  perl -MExtUtils::Command -e rm_rf directories...
  perl -MExtUtils::Command -e mkpath directories...
  perl -MExtUtils::Command -e eqtime source destination
  perl -MExtUtils::Command -e test_f file
  perl -MExtUtils::Command -e test_d directory
  perl -MExtUtils::Command -e chmod mode files...
  ...

=head1 DESCRIPTION

The module is used to replace common UNIX commands.  In all cases the
functions work from @ARGV rather than taking arguments.  This makes
them easier to deal with in Makefiles.  Call them like this:

  perl -MExtUtils::Command -e some_command some files to work on

and I<NOT> like this:

  perl -MExtUtils::Command -e 'some_command qw(some files to work on)'

For that use L<Shell::Command>.

Filenames with * and ? will be glob expanded.


=head2 FUNCTIONS

=over 4

=cut

# VMS uses % instead of ? to mean "one character"
my $wild_regex = $Is_VMS ? '*%' : '*?';
sub expand_wildcards
{
 @ARGV = map(/[$wild_regex]/o ? glob($_) : $_,@ARGV);
}


=item cat

    cat file ...

Concatenates all files mentioned on command line to STDOUT.

=cut

sub cat ()
{
 expand_wildcards();
 print while (<>);
}

=item eqtime

    eqtime source destination

Sets modified time of destination to that of source.

=cut

sub eqtime
{
 my ($src,$dst) = @ARGV;
 local @ARGV = ($dst);  touch();  # in case $dst doesn't exist
 utime((stat($src))[8,9],$dst);
}

=item rm_rf

    rm_rf files or directories ...

Removes files and directories - recursively (even if readonly)

=cut

sub rm_rf
{
 expand_wildcards();
 require File::Path;
 File::Path::rmtree([grep -e $_,@ARGV],0,0);
}

=item rm_f

    rm_f file ...

Removes files (even if readonly)

=cut

sub rm_f {
    expand_wildcards();

    foreach my $file (@ARGV) {
        next unless -f $file;

        next if _unlink($file);

        chmod(0777, $file);

        next if _unlink($file);

        require Carp;
        Carp::carp("Cannot delete $file: $!");
    }
}

sub _unlink {
    my $files_unlinked = 0;
    foreach my $file (@_) {
        my $delete_count = 0;
        $delete_count++ while unlink $file;
        $files_unlinked++ if $delete_count;
    }
    return $files_unlinked;
}


=item touch

    touch file ...

Makes files exist, with current timestamp

=cut

sub touch {
    my $t    = time;
    expand_wildcards();
    foreach my $file (@ARGV) {
        open(FILE,">>$file") || die "Cannot write $file:$!";
        close(FILE);
        utime($t,$t,$file);
    }
}

=item mv

    mv source_file destination_file
    mv source_file source_file destination_dir

Moves source to destination.  Multiple sources are allowed if
destination is an existing directory.

Returns true if all moves succeeded, false otherwise.

=cut

sub mv {
    expand_wildcards();
    my @src = @ARGV;
    my $dst = pop @src;

    if (@src > 1 && ! -d $dst) {
        require Carp;
        Carp::croak("Too many arguments");
    }

    require File::Copy;
    my $nok = 0;
    foreach my $src (@src) {
        $nok ||= !File::Copy::move($src,$dst);
    }
    return !$nok;
}

=item cp

    cp source_file destination_file
    cp source_file source_file destination_dir

Copies sources to the destination.  Multiple sources are allowed if
destination is an existing directory.

Returns true if all copies succeeded, false otherwise.

=cut

sub cp {
    expand_wildcards();
    my @src = @ARGV;
    my $dst = pop @src;

    if (@src > 1 && ! -d $dst) {
        require Carp;
        Carp::croak("Too many arguments");
    }

    require File::Copy;
    my $nok = 0;
    foreach my $src (@src) {
        $nok ||= !File::Copy::copy($src,$dst);

        # Win32 does not update the mod time of a copied file, just the
        # created time which make does not look at.
        utime(time, time, $dst) if $Is_Win32;
    }
    return $nok;
}

=item chmod

    chmod mode files ...

Sets UNIX like permissions 'mode' on all the files.  e.g. 0666

=cut

sub chmod {
    local @ARGV = @ARGV;
    my $mode = shift(@ARGV);
    expand_wildcards();

    if( $Is_VMS_mode && $Is_VMS_noefs) {
        require File::Spec;
        foreach my $idx (0..$#ARGV) {
            my $path = $ARGV[$idx];
            next unless -d $path;

            # chmod 0777, [.foo.bar] doesn't work on VMS, you have to do
            # chmod 0777, [.foo]bar.dir
            my @dirs = File::Spec->splitdir( $path );
            $dirs[-1] .= '.dir';
            $path = File::Spec->catfile(@dirs);

            $ARGV[$idx] = $path;
        }
    }

    chmod(oct $mode,@ARGV) || die "Cannot chmod ".join(' ',$mode,@ARGV).":$!";
}

=item mkpath

    mkpath directory ...

Creates directories, including any parent directories.

=cut

sub mkpath
{
 expand_wildcards();
 require File::Path;
 File::Path::mkpath([@ARGV],0,0777);
}

=item test_f

    test_f file

Tests if a file exists.  I<Exits> with 0 if it does, 1 if it does not (ie.
shell's idea of true and false).

=cut

sub test_f
{
 exit(-f $ARGV[0] ? 0 : 1);
}

=item test_d

    test_d directory

Tests if a directory exists.  I<Exits> with 0 if it does, 1 if it does
not (ie. shell's idea of true and false).

=cut

sub test_d
{
 exit(-d $ARGV[0] ? 0 : 1);
}

=item dos2unix

    dos2unix files or dirs ...

Converts DOS and OS/2 linefeeds to Unix style recursively.

=cut

sub dos2unix {
    require File::Find;
    File::Find::find(sub {
        return if -d;
        return unless -w _;
        return unless -r _;
        return if -B _;

        local $\;

    my $orig = $_;
    my $temp = '.dos2unix_tmp';
    open ORIG, $_ or do { warn "dos2unix can't open $_: $!"; return };
    open TEMP, ">$temp" or
        do { warn "dos2unix can't create .dos2unix_tmp: $!"; return };
        binmode ORIG; binmode TEMP;
        while (my $line = <ORIG>) {
            $line =~ s/\015\012/\012/g;
            print TEMP $line;
        }
    close ORIG;
    close TEMP;
    rename $temp, $orig;

    }, @ARGV);
}

=back

=head1 SEE ALSO

Shell::Command which is these same functions but take arguments normally.


=head1 AUTHOR

Nick Ing-Simmons C<ni-s@cpan.org>

Maintained by Michael G Schwern C<schwern@pobox.com> within the
ExtUtils-MakeMaker package and, as a separate CPAN package, by
Randy Kobes C<r.kobes@uwinnipeg.ca>.

=cut

