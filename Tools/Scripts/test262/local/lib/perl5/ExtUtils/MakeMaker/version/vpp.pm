#--------------------------------------------------------------------------#
# This is a modified copy of version.pm 0.9909, bundled exclusively for
# use by ExtUtils::Makemaker and its dependencies to bootstrap when
# version.pm is not available.  It should not be used by ordinary modules.
#--------------------------------------------------------------------------#

package ExtUtils::MakeMaker::charstar;
# a little helper class to emulate C char* semantics in Perl
# so that prescan_version can use the same code as in C

use overload (
    '""'    => \&thischar,
    '0+'    => \&thischar,
    '++'    => \&increment,
    '--'    => \&decrement,
    '+'        => \&plus,
    '-'        => \&minus,
    '*'        => \&multiply,
    'cmp'    => \&cmp,
    '<=>'    => \&spaceship,
    'bool'    => \&thischar,
    '='        => \&clone,
);

sub new {
    my ($self, $string) = @_;
    my $class = ref($self) || $self;

    my $obj = {
    string  => [split(//,$string)],
    current => 0,
    };
    return bless $obj, $class;
}

sub thischar {
    my ($self) = @_;
    my $last = $#{$self->{string}};
    my $curr = $self->{current};
    if ($curr >= 0 && $curr <= $last) {
    return $self->{string}->[$curr];
    }
    else {
    return '';
    }
}

sub increment {
    my ($self) = @_;
    $self->{current}++;
}

sub decrement {
    my ($self) = @_;
    $self->{current}--;
}

sub plus {
    my ($self, $offset) = @_;
    my $rself = $self->clone;
    $rself->{current} += $offset;
    return $rself;
}

sub minus {
    my ($self, $offset) = @_;
    my $rself = $self->clone;
    $rself->{current} -= $offset;
    return $rself;
}

sub multiply {
    my ($left, $right, $swapped) = @_;
    my $char = $left->thischar();
    return $char * $right;
}

sub spaceship {
    my ($left, $right, $swapped) = @_;
    unless (ref($right)) { # not an object already
    $right = $left->new($right);
    }
    return $left->{current} <=> $right->{current};
}

sub cmp {
    my ($left, $right, $swapped) = @_;
    unless (ref($right)) { # not an object already
    if (length($right) == 1) { # comparing single character only
        return $left->thischar cmp $right;
    }
    $right = $left->new($right);
    }
    return $left->currstr cmp $right->currstr;
}

sub bool {
    my ($self) = @_;
    my $char = $self->thischar;
    return ($char ne '');
}

sub clone {
    my ($left, $right, $swapped) = @_;
    $right = {
    string  => [@{$left->{string}}],
    current => $left->{current},
    };
    return bless $right, ref($left);
}

sub currstr {
    my ($self, $s) = @_;
    my $curr = $self->{current};
    my $last = $#{$self->{string}};
    if (defined($s) && $s->{current} < $last) {
    $last = $s->{current};
    }

    my $string = join('', @{$self->{string}}[$curr..$last]);
    return $string;
}

package ExtUtils::MakeMaker::version::vpp;

use 5.006001;
use strict;

use Config;
use vars qw($VERSION $CLASS @ISA $LAX $STRICT);
$VERSION = '7.32';
$VERSION = eval $VERSION;
$CLASS = 'ExtUtils::MakeMaker::version::vpp';

require ExtUtils::MakeMaker::version::regex;
*ExtUtils::MakeMaker::version::vpp::is_strict = \&ExtUtils::MakeMaker::version::regex::is_strict;
*ExtUtils::MakeMaker::version::vpp::is_lax = \&ExtUtils::MakeMaker::version::regex::is_lax;
*LAX = \$ExtUtils::MakeMaker::version::regex::LAX;
*STRICT = \$ExtUtils::MakeMaker::version::regex::STRICT;

use overload (
    '""'       => \&stringify,
    '0+'       => \&numify,
    'cmp'      => \&vcmp,
    '<=>'      => \&vcmp,
    'bool'     => \&vbool,
    '+'        => \&vnoop,
    '-'        => \&vnoop,
    '*'        => \&vnoop,
    '/'        => \&vnoop,
    '+='        => \&vnoop,
    '-='        => \&vnoop,
    '*='        => \&vnoop,
    '/='        => \&vnoop,
    'abs'      => \&vnoop,
);

eval "use warnings";
if ($@) {
    eval '
    package
    warnings;
    sub enabled {return $^W;}
    1;
    ';
}

sub import {
    no strict 'refs';
    my ($class) = shift;

    # Set up any derived class
    unless ($class eq $CLASS) {
    local $^W;
    *{$class.'::declare'} =  \&{$CLASS.'::declare'};
    *{$class.'::qv'} = \&{$CLASS.'::qv'};
    }

    my %args;
    if (@_) { # any remaining terms are arguments
    map { $args{$_} = 1 } @_
    }
    else { # no parameters at all on use line
    %args =
    (
        qv => 1,
        'UNIVERSAL::VERSION' => 1,
    );
    }

    my $callpkg = caller();

    if (exists($args{declare})) {
    *{$callpkg.'::declare'} =
        sub {return $class->declare(shift) }
      unless defined(&{$callpkg.'::declare'});
    }

    if (exists($args{qv})) {
    *{$callpkg.'::qv'} =
        sub {return $class->qv(shift) }
      unless defined(&{$callpkg.'::qv'});
    }

    if (exists($args{'UNIVERSAL::VERSION'})) {
    local $^W;
    *UNIVERSAL::VERSION
        = \&{$CLASS.'::_VERSION'};
    }

    if (exists($args{'VERSION'})) {
    *{$callpkg.'::VERSION'} = \&{$CLASS.'::_VERSION'};
    }

    if (exists($args{'is_strict'})) {
    *{$callpkg.'::is_strict'} = \&{$CLASS.'::is_strict'}
      unless defined(&{$callpkg.'::is_strict'});
    }

    if (exists($args{'is_lax'})) {
    *{$callpkg.'::is_lax'} = \&{$CLASS.'::is_lax'}
      unless defined(&{$callpkg.'::is_lax'});
    }
}

my $VERSION_MAX = 0x7FFFFFFF;

# implement prescan_version as closely to the C version as possible
use constant TRUE  => 1;
use constant FALSE => 0;

sub isDIGIT {
    my ($char) = shift->thischar();
    return ($char =~ /\d/);
}

sub isALPHA {
    my ($char) = shift->thischar();
    return ($char =~ /[a-zA-Z]/);
}

sub isSPACE {
    my ($char) = shift->thischar();
    return ($char =~ /\s/);
}

sub BADVERSION {
    my ($s, $errstr, $error) = @_;
    if ($errstr) {
    $$errstr = $error;
    }
    return $s;
}

sub prescan_version {
    my ($s, $strict, $errstr, $sqv, $ssaw_decimal, $swidth, $salpha) = @_;
    my $qv          = defined $sqv          ? $$sqv          : FALSE;
    my $saw_decimal = defined $ssaw_decimal ? $$ssaw_decimal : 0;
    my $width       = defined $swidth       ? $$swidth       : 3;
    my $alpha       = defined $salpha       ? $$salpha       : FALSE;

    my $d = $s;

    if ($qv && isDIGIT($d)) {
    goto dotted_decimal_version;
    }

    if ($d eq 'v') { # explicit v-string
    $d++;
    if (isDIGIT($d)) {
        $qv = TRUE;
    }
    else { # degenerate v-string
        # requires v1.2.3
        return BADVERSION($s,$errstr,"Invalid version format (dotted-decimal versions require at least three parts)");
    }

dotted_decimal_version:
    if ($strict && $d eq '0' && isDIGIT($d+1)) {
        # no leading zeros allowed
        return BADVERSION($s,$errstr,"Invalid version format (no leading zeros)");
    }

    while (isDIGIT($d)) {     # integer part
        $d++;
    }

    if ($d eq '.')
    {
        $saw_decimal++;
        $d++;         # decimal point
    }
    else
    {
        if ($strict) {
        # require v1.2.3
        return BADVERSION($s,$errstr,"Invalid version format (dotted-decimal versions require at least three parts)");
        }
        else {
        goto version_prescan_finish;
        }
    }

    {
        my $i = 0;
        my $j = 0;
        while (isDIGIT($d)) {    # just keep reading
        $i++;
        while (isDIGIT($d)) {
            $d++; $j++;
            # maximum 3 digits between decimal
            if ($strict && $j > 3) {
            return BADVERSION($s,$errstr,"Invalid version format (maximum 3 digits between decimals)");
            }
        }
        if ($d eq '_') {
            if ($strict) {
            return BADVERSION($s,$errstr,"Invalid version format (no underscores)");
            }
            if ( $alpha ) {
            return BADVERSION($s,$errstr,"Invalid version format (multiple underscores)");
            }
            $d++;
            $alpha = TRUE;
        }
        elsif ($d eq '.') {
            if ($alpha) {
            return BADVERSION($s,$errstr,"Invalid version format (underscores before decimal)");
            }
            $saw_decimal++;
            $d++;
        }
        elsif (!isDIGIT($d)) {
            last;
        }
        $j = 0;
        }

        if ($strict && $i < 2) {
        # requires v1.2.3
        return BADVERSION($s,$errstr,"Invalid version format (dotted-decimal versions require at least three parts)");
        }
    }
    }                     # end if dotted-decimal
    else
    {                    # decimal versions
    my $j = 0;
    # special $strict case for leading '.' or '0'
    if ($strict) {
        if ($d eq '.') {
        return BADVERSION($s,$errstr,"Invalid version format (0 before decimal required)");
        }
        if ($d eq '0' && isDIGIT($d+1)) {
        return BADVERSION($s,$errstr,"Invalid version format (no leading zeros)");
        }
    }

    # and we never support negative version numbers
    if ($d eq '-') {
        return BADVERSION($s,$errstr,"Invalid version format (negative version number)");
    }

    # consume all of the integer part
    while (isDIGIT($d)) {
        $d++;
    }

    # look for a fractional part
    if ($d eq '.') {
        # we found it, so consume it
        $saw_decimal++;
        $d++;
    }
    elsif (!$d || $d eq ';' || isSPACE($d) || $d eq '}') {
        if ( $d == $s ) {
        # found nothing
        return BADVERSION($s,$errstr,"Invalid version format (version required)");
        }
        # found just an integer
        goto version_prescan_finish;
    }
    elsif ( $d == $s ) {
        # didn't find either integer or period
        return BADVERSION($s,$errstr,"Invalid version format (non-numeric data)");
    }
    elsif ($d eq '_') {
        # underscore can't come after integer part
        if ($strict) {
        return BADVERSION($s,$errstr,"Invalid version format (no underscores)");
        }
        elsif (isDIGIT($d+1)) {
        return BADVERSION($s,$errstr,"Invalid version format (alpha without decimal)");
        }
        else {
        return BADVERSION($s,$errstr,"Invalid version format (misplaced underscore)");
        }
    }
    elsif ($d) {
        # anything else after integer part is just invalid data
        return BADVERSION($s,$errstr,"Invalid version format (non-numeric data)");
    }

    # scan the fractional part after the decimal point
    if ($d && !isDIGIT($d) && ($strict || ! ($d eq ';' || isSPACE($d) || $d eq '}') )) {
        # $strict or lax-but-not-the-end
        return BADVERSION($s,$errstr,"Invalid version format (fractional part required)");
    }

    while (isDIGIT($d)) {
        $d++; $j++;
        if ($d eq '.' && isDIGIT($d-1)) {
        if ($alpha) {
            return BADVERSION($s,$errstr,"Invalid version format (underscores before decimal)");
        }
        if ($strict) {
            return BADVERSION($s,$errstr,"Invalid version format (dotted-decimal versions must begin with 'v')");
        }
        $d = $s; # start all over again
        $qv = TRUE;
        goto dotted_decimal_version;
        }
        if ($d eq '_') {
        if ($strict) {
            return BADVERSION($s,$errstr,"Invalid version format (no underscores)");
        }
        if ( $alpha ) {
            return BADVERSION($s,$errstr,"Invalid version format (multiple underscores)");
        }
        if ( ! isDIGIT($d+1) ) {
            return BADVERSION($s,$errstr,"Invalid version format (misplaced underscore)");
        }
        $width = $j;
        $d++;
        $alpha = TRUE;
        }
    }
    }

version_prescan_finish:
    while (isSPACE($d)) {
    $d++;
    }

    if ($d && !isDIGIT($d) && (! ($d eq ';' || $d eq '}') )) {
    # trailing non-numeric data
    return BADVERSION($s,$errstr,"Invalid version format (non-numeric data)");
    }

    if (defined $sqv) {
    $$sqv = $qv;
    }
    if (defined $swidth) {
    $$swidth = $width;
    }
    if (defined $ssaw_decimal) {
    $$ssaw_decimal = $saw_decimal;
    }
    if (defined $salpha) {
    $$salpha = $alpha;
    }
    return $d;
}

sub scan_version {
    my ($s, $rv, $qv) = @_;
    my $start;
    my $pos;
    my $last;
    my $errstr;
    my $saw_decimal = 0;
    my $width = 3;
    my $alpha = FALSE;
    my $vinf = FALSE;
    my @av;

    $s = new ExtUtils::MakeMaker::charstar $s;

    while (isSPACE($s)) { # leading whitespace is OK
    $s++;
    }

    $last = prescan_version($s, FALSE, \$errstr, \$qv, \$saw_decimal,
    \$width, \$alpha);

    if ($errstr) {
    # 'undef' is a special case and not an error
    if ( $s ne 'undef') {
        require Carp;
        Carp::croak($errstr);
    }
    }

    $start = $s;
    if ($s eq 'v') {
    $s++;
    }
    $pos = $s;

    if ( $qv ) {
    $$rv->{qv} = $qv;
    }
    if ( $alpha ) {
    $$rv->{alpha} = $alpha;
    }
    if ( !$qv && $width < 3 ) {
    $$rv->{width} = $width;
    }

    while (isDIGIT($pos)) {
    $pos++;
    }
    if (!isALPHA($pos)) {
    my $rev;

    for (;;) {
        $rev = 0;
        {
          # this is atoi() that delimits on underscores
          my $end = $pos;
          my $mult = 1;
        my $orev;

        #  the following if() will only be true after the decimal
        #  point of a version originally created with a bare
        #  floating point number, i.e. not quoted in any way
        #
         if ( !$qv && $s > $start && $saw_decimal == 1 ) {
            $mult *= 100;
             while ( $s < $end ) {
            $orev = $rev;
             $rev += $s * $mult;
             $mult /= 10;
            if (   (abs($orev) > abs($rev))
                || (abs($rev) > $VERSION_MAX )) {
                warn("Integer overflow in version %d",
                       $VERSION_MAX);
                $s = $end - 1;
                $rev = $VERSION_MAX;
                $vinf = 1;
            }
             $s++;
            if ( $s eq '_' ) {
                $s++;
            }
             }
          }
         else {
             while (--$end >= $s) {
            $orev = $rev;
             $rev += $end * $mult;
             $mult *= 10;
            if (   (abs($orev) > abs($rev))
                || (abs($rev) > $VERSION_MAX )) {
                warn("Integer overflow in version");
                $end = $s - 1;
                $rev = $VERSION_MAX;
                $vinf = 1;
            }
             }
         }
          }

          # Append revision
        push @av, $rev;
        if ( $vinf ) {
        $s = $last;
        last;
        }
        elsif ( $pos eq '.' ) {
        $s = ++$pos;
        }
        elsif ( $pos eq '_' && isDIGIT($pos+1) ) {
        $s = ++$pos;
        }
        elsif ( $pos eq ',' && isDIGIT($pos+1) ) {
        $s = ++$pos;
        }
        elsif ( isDIGIT($pos) ) {
        $s = $pos;
        }
        else {
        $s = $pos;
        last;
        }
        if ( $qv ) {
        while ( isDIGIT($pos) ) {
            $pos++;
        }
        }
        else {
        my $digits = 0;
        while ( ( isDIGIT($pos) || $pos eq '_' ) && $digits < 3 ) {
            if ( $pos ne '_' ) {
            $digits++;
            }
            $pos++;
        }
        }
    }
    }
    if ( $qv ) { # quoted versions always get at least three terms
    my $len = $#av;
    #  This for loop appears to trigger a compiler bug on OS X, as it
    #  loops infinitely. Yes, len is negative. No, it makes no sense.
    #  Compiler in question is:
    #  gcc version 3.3 20030304 (Apple Computer, Inc. build 1640)
    #  for ( len = 2 - len; len > 0; len-- )
    #  av_push(MUTABLE_AV(sv), newSViv(0));
    #
    $len = 2 - $len;
    while ($len-- > 0) {
        push @av, 0;
    }
    }

    # need to save off the current version string for later
    if ( $vinf ) {
    $$rv->{original} = "v.Inf";
    $$rv->{vinf} = 1;
    }
    elsif ( $s > $start ) {
    $$rv->{original} = $start->currstr($s);
    if ( $qv && $saw_decimal == 1 && $start ne 'v' ) {
        # need to insert a v to be consistent
        $$rv->{original} = 'v' . $$rv->{original};
    }
    }
    else {
    $$rv->{original} = '0';
    push(@av, 0);
    }

    # And finally, store the AV in the hash
    $$rv->{version} = \@av;

    # fix RT#19517 - special case 'undef' as string
    if ($s eq 'undef') {
    $s += 5;
    }

    return $s;
}

sub new {
    my $class = shift;
    unless (defined $class or $#_ > 1) {
    require Carp;
    Carp::croak('Usage: version::new(class, version)');
    }

    my $self = bless ({}, ref ($class) || $class);
    my $qv = FALSE;

    if ( $#_ == 1 ) { # must be CVS-style
    $qv = TRUE;
    }
    my $value = pop; # always going to be the last element

    if ( ref($value) && eval('$value->isa("version")') ) {
    # Can copy the elements directly
    $self->{version} = [ @{$value->{version} } ];
    $self->{qv} = 1 if $value->{qv};
    $self->{alpha} = 1 if $value->{alpha};
    $self->{original} = ''.$value->{original};
    return $self;
    }

    if ( not defined $value or $value =~ /^undef$/ ) {
    # RT #19517 - special case for undef comparison
    # or someone forgot to pass a value
    push @{$self->{version}}, 0;
    $self->{original} = "0";
    return ($self);
    }


    if (ref($value) =~ m/ARRAY|HASH/) {
    require Carp;
    Carp::croak("Invalid version format (non-numeric data)");
    }

    $value = _un_vstring($value);

    if ($Config{d_setlocale} && eval { require POSIX } ) {
      require locale;
    my $currlocale = POSIX::setlocale(&POSIX::LC_ALL);

    # if the current locale uses commas for decimal points, we
    # just replace commas with decimal places, rather than changing
    # locales
    if ( POSIX::localeconv()->{decimal_point} eq ',' ) {
        $value =~ tr/,/./;
    }
    }

    # exponential notation
    if ( $value =~ /\d+.?\d*e[-+]?\d+/ ) {
    $value = sprintf("%.9f",$value);
    $value =~ s/(0+)$//; # trim trailing zeros
    }

    my $s = scan_version($value, \$self, $qv);

    if ($s) { # must be something left over
    warn("Version string '%s' contains invalid data; "
           ."ignoring: '%s'", $value, $s);
    }

    return ($self);
}

*parse = \&new;

sub numify {
    my ($self) = @_;
    unless (_verify($self)) {
    require Carp;
    Carp::croak("Invalid version object");
    }
    my $width = $self->{width} || 3;
    my $alpha = $self->{alpha} || "";
    my $len = $#{$self->{version}};
    my $digit = $self->{version}[0];
    my $string = sprintf("%d.", $digit );

    for ( my $i = 1 ; $i < $len ; $i++ ) {
    $digit = $self->{version}[$i];
    if ( $width < 3 ) {
        my $denom = 10**(3-$width);
        my $quot = int($digit/$denom);
        my $rem = $digit - ($quot * $denom);
        $string .= sprintf("%0".$width."d_%d", $quot, $rem);
    }
    else {
        $string .= sprintf("%03d", $digit);
    }
    }

    if ( $len > 0 ) {
    $digit = $self->{version}[$len];
    if ( $alpha && $width == 3 ) {
        $string .= "_";
    }
    $string .= sprintf("%0".$width."d", $digit);
    }
    else # $len = 0
    {
    $string .= sprintf("000");
    }

    return $string;
}

sub normal {
    my ($self) = @_;
    unless (_verify($self)) {
    require Carp;
    Carp::croak("Invalid version object");
    }
    my $alpha = $self->{alpha} || "";
    my $len = $#{$self->{version}};
    my $digit = $self->{version}[0];
    my $string = sprintf("v%d", $digit );

    for ( my $i = 1 ; $i < $len ; $i++ ) {
    $digit = $self->{version}[$i];
    $string .= sprintf(".%d", $digit);
    }

    if ( $len > 0 ) {
    $digit = $self->{version}[$len];
    if ( $alpha ) {
        $string .= sprintf("_%0d", $digit);
    }
    else {
        $string .= sprintf(".%0d", $digit);
    }
    }

    if ( $len <= 2 ) {
    for ( $len = 2 - $len; $len != 0; $len-- ) {
        $string .= sprintf(".%0d", 0);
    }
    }

    return $string;
}

sub stringify {
    my ($self) = @_;
    unless (_verify($self)) {
    require Carp;
    Carp::croak("Invalid version object");
    }
    return exists $self->{original}
        ? $self->{original}
    : exists $self->{qv}
        ? $self->normal
        : $self->numify;
}

sub vcmp {
    require UNIVERSAL;
    my ($left,$right,$swap) = @_;
    my $class = ref($left);
    unless ( UNIVERSAL::isa($right, $class) ) {
    $right = $class->new($right);
    }

    if ( $swap ) {
    ($left, $right) = ($right, $left);
    }
    unless (_verify($left)) {
    require Carp;
    Carp::croak("Invalid version object");
    }
    unless (_verify($right)) {
    require Carp;
    Carp::croak("Invalid version format");
    }
    my $l = $#{$left->{version}};
    my $r = $#{$right->{version}};
    my $m = $l < $r ? $l : $r;
    my $lalpha = $left->is_alpha;
    my $ralpha = $right->is_alpha;
    my $retval = 0;
    my $i = 0;
    while ( $i <= $m && $retval == 0 ) {
    $retval = $left->{version}[$i] <=> $right->{version}[$i];
    $i++;
    }

    # tiebreaker for alpha with identical terms
    if ( $retval == 0
    && $l == $r
    && $left->{version}[$m] == $right->{version}[$m]
    && ( $lalpha || $ralpha ) ) {

    if ( $lalpha && !$ralpha ) {
        $retval = -1;
    }
    elsif ( $ralpha && !$lalpha) {
        $retval = +1;
    }
    }

    # possible match except for trailing 0's
    if ( $retval == 0 && $l != $r ) {
    if ( $l < $r ) {
        while ( $i <= $r && $retval == 0 ) {
        if ( $right->{version}[$i] != 0 ) {
            $retval = -1; # not a match after all
        }
        $i++;
        }
    }
    else {
        while ( $i <= $l && $retval == 0 ) {
        if ( $left->{version}[$i] != 0 ) {
            $retval = +1; # not a match after all
        }
        $i++;
        }
    }
    }

    return $retval;
}

sub vbool {
    my ($self) = @_;
    return vcmp($self,$self->new("0"),1);
}

sub vnoop {
    require Carp;
    Carp::croak("operation not supported with version object");
}

sub is_alpha {
    my ($self) = @_;
    return (exists $self->{alpha});
}

sub qv {
    my $value = shift;
    my $class = $CLASS;
    if (@_) {
    $class = ref($value) || $value;
    $value = shift;
    }

    $value = _un_vstring($value);
    $value = 'v'.$value unless $value =~ /(^v|\d+\.\d+\.\d)/;
    my $obj = $CLASS->new($value);
    return bless $obj, $class;
}

*declare = \&qv;

sub is_qv {
    my ($self) = @_;
    return (exists $self->{qv});
}


sub _verify {
    my ($self) = @_;
    if ( ref($self)
    && eval { exists $self->{version} }
    && ref($self->{version}) eq 'ARRAY'
    ) {
    return 1;
    }
    else {
    return 0;
    }
}

sub _is_non_alphanumeric {
    my $s = shift;
    $s = new ExtUtils::MakeMaker::charstar $s;
    while ($s) {
    return 0 if isSPACE($s); # early out
    return 1 unless (isALPHA($s) || isDIGIT($s) || $s =~ /[.-]/);
    $s++;
    }
    return 0;
}

sub _un_vstring {
    my $value = shift;
    # may be a v-string
    if ( length($value) >= 3 && $value !~ /[._]/
    && _is_non_alphanumeric($value)) {
    my $tvalue;
    if ( $] ge 5.008_001 ) {
        $tvalue = _find_magic_vstring($value);
        $value = $tvalue if length $tvalue;
    }
    elsif ( $] ge 5.006_000 ) {
        $tvalue = sprintf("v%vd",$value);
        if ( $tvalue =~ /^v\d+(\.\d+){2,}$/ ) {
        # must be a v-string
        $value = $tvalue;
        }
    }
    }
    return $value;
}

sub _find_magic_vstring {
    my $value = shift;
    my $tvalue = '';
    require B;
    my $sv = B::svref_2object(\$value);
    my $magic = ref($sv) eq 'B::PVMG' ? $sv->MAGIC : undef;
    while ( $magic ) {
    if ( $magic->TYPE eq 'V' ) {
        $tvalue = $magic->PTR;
        $tvalue =~ s/^v?(.+)$/v$1/;
        last;
    }
    else {
        $magic = $magic->MOREMAGIC;
    }
    }
    return $tvalue;
}

sub _VERSION {
    my ($obj, $req) = @_;
    my $class = ref($obj) || $obj;

    no strict 'refs';
    if ( exists $INC{"$class.pm"} and not %{"$class\::"} and $] >= 5.008) {
     # file but no package
    require Carp;
    Carp::croak( "$class defines neither package nor VERSION"
        ."--version check failed");
    }

    my $version = eval "\$$class\::VERSION";
    if ( defined $version ) {
    local $^W if $] <= 5.008;
    $version = ExtUtils::MakeMaker::version::vpp->new($version);
    }

    if ( defined $req ) {
    unless ( defined $version ) {
        require Carp;
        my $msg =  $] < 5.006
        ? "$class version $req required--this is only version "
        : "$class does not define \$$class\::VERSION"
          ."--version check failed";

        if ( $ENV{VERSION_DEBUG} ) {
        Carp::confess($msg);
        }
        else {
        Carp::croak($msg);
        }
    }

    $req = ExtUtils::MakeMaker::version::vpp->new($req);

    if ( $req > $version ) {
        require Carp;
        if ( $req->is_qv ) {
        Carp::croak(
            sprintf ("%s version %s required--".
            "this is only version %s", $class,
            $req->normal, $version->normal)
        );
        }
        else {
        Carp::croak(
            sprintf ("%s version %s required--".
            "this is only version %s", $class,
            $req->stringify, $version->stringify)
        );
        }
    }
    }

    return defined $version ? $version->stringify : undef;
}

1; #this line is important and will help the module return a true value
