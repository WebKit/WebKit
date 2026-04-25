#--------------------------------------------------------------------------#
# This is a modified copy of version.pm 0.9909, bundled exclusively for
# use by ExtUtils::Makemaker and its dependencies to bootstrap when
# version.pm is not available.  It should not be used by ordinary modules.
#
# When loaded, it will try to load version.pm.  If that fails, it will load
# ExtUtils::MakeMaker::version::vpp and alias various *version functions
# to functions in that module.  It will also override UNIVERSAL::VERSION.
#--------------------------------------------------------------------------#

package ExtUtils::MakeMaker::version;

use 5.006001;
use strict;

use vars qw(@ISA $VERSION $CLASS $STRICT $LAX *declare *qv);

$VERSION = '7.32';
$VERSION = eval $VERSION;
$CLASS = 'version';

{
    local $SIG{'__DIE__'};
    eval "use version";
    if ( $@ ) { # don't have any version.pm installed
        eval "use ExtUtils::MakeMaker::version::vpp";
        die "$@" if ( $@ );
        local $^W;
        delete $INC{'version.pm'};
        $INC{'version.pm'} = $INC{'ExtUtils/MakeMaker/version.pm'};
        push @version::ISA, "ExtUtils::MakeMaker::version::vpp";
        $version::VERSION = $VERSION;
        *version::qv = \&ExtUtils::MakeMaker::version::vpp::qv;
        *version::declare = \&ExtUtils::MakeMaker::version::vpp::declare;
        *version::_VERSION = \&ExtUtils::MakeMaker::version::vpp::_VERSION;
        *version::vcmp = \&ExtUtils::MakeMaker::version::vpp::vcmp;
        *version::new = \&ExtUtils::MakeMaker::version::vpp::new;
        if ($] >= 5.009000) {
            no strict 'refs';
            *version::stringify = \&ExtUtils::MakeMaker::version::vpp::stringify;
            *{'version::(""'} = \&ExtUtils::MakeMaker::version::vpp::stringify;
            *{'version::(<=>'} = \&ExtUtils::MakeMaker::version::vpp::vcmp;
            *version::parse = \&ExtUtils::MakeMaker::version::vpp::parse;
        }
        require ExtUtils::MakeMaker::version::regex;
        *version::is_lax = \&ExtUtils::MakeMaker::version::regex::is_lax;
        *version::is_strict = \&ExtUtils::MakeMaker::version::regex::is_strict;
        *LAX = \$ExtUtils::MakeMaker::version::regex::LAX;
        *STRICT = \$ExtUtils::MakeMaker::version::regex::STRICT;
    }
    elsif ( ! version->can('is_qv') ) {
        *version::is_qv = sub { exists $_[0]->{qv} };
    }
}

1;
