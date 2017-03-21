# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

package Bugzilla::BugUrl;

use 5.10.1;
use strict;
use warnings;

use parent qw(Bugzilla::Object);

use Bugzilla::Util;
use Bugzilla::Error;
use Bugzilla::Constants;
use Bugzilla::Hook;

use URI;
use URI::QueryParam;

###############################
####    Initialization     ####
###############################

use constant DB_TABLE   => 'bug_see_also';
use constant NAME_FIELD => 'value';
use constant LIST_ORDER => 'id';
# See Also is tracked in bugs_activity.
use constant AUDIT_CREATES => 0;
use constant AUDIT_UPDATES => 0;
use constant AUDIT_REMOVES => 0;

use constant DB_COLUMNS => qw(
    id
    bug_id
    value
    class
);

# This must be strings with the names of the validations,
# instead of coderefs, because subclasses override these
# validators with their own.
use constant VALIDATORS => {
    value  => '_check_value',
    bug_id => '_check_bug_id',
    class  => \&_check_class,
};

# This is the order we go through all of subclasses and
# pick the first one that should handle the url. New
# subclasses should be added at the end of the list.
use constant SUB_CLASSES => qw(
    Bugzilla::BugUrl::Bugzilla::Local
    Bugzilla::BugUrl::Bugzilla
    Bugzilla::BugUrl::Launchpad
    Bugzilla::BugUrl::Google
    Bugzilla::BugUrl::Chromium
    Bugzilla::BugUrl::Debian
    Bugzilla::BugUrl::JIRA
    Bugzilla::BugUrl::Trac
    Bugzilla::BugUrl::MantisBT
    Bugzilla::BugUrl::SourceForge
    Bugzilla::BugUrl::GitHub
);

###############################
####      Accessors      ######
###############################

sub class  { return $_[0]->{class}  }
sub bug_id { return $_[0]->{bug_id} }

###############################
####        Methods        ####
###############################

sub new {
    my $class = shift;
    my $param = shift;

    if (ref $param) {
        my $bug_id = $param->{bug_id};
        my $name   = $param->{name} || $param->{value};
        if (!defined $bug_id) {
            ThrowCodeError('bad_arg',
                { argument => 'bug_id',
                  function => "${class}::new" });
        }
        if (!defined $name) {
            ThrowCodeError('bad_arg',
                { argument => 'name',
                  function => "${class}::new" });
        }

        my $condition = 'bug_id = ? AND value = ?';
        my @values = ($bug_id, $name);
        $param = { condition => $condition, values => \@values };
    }

    unshift @_, $param;
    return $class->SUPER::new(@_);
}

sub _do_list_select {
    my $class = shift;
    my $objects = $class->SUPER::_do_list_select(@_);

    foreach my $object (@$objects) {
        eval "use " . $object->class;
        # If the class cannot be loaded, then we build a generic object.
        bless $object, ($@ ? 'Bugzilla::BugUrl' : $object->class);
    }

    return $objects
}

# This is an abstract method. It must be overridden
# in every subclass.
sub should_handle {
    my ($class, $input) = @_;
    ThrowCodeError('unknown_method',
        { method => "${class}::should_handle" });
}

sub class_for {
    my ($class, $value) = @_;

    my @sub_classes = $class->SUB_CLASSES;
    Bugzilla::Hook::process("bug_url_sub_classes",
        { sub_classes => \@sub_classes });

    my $uri = URI->new($value);
    foreach my $subclass (@sub_classes) {
        eval "use $subclass";
        die $@ if $@;
        return wantarray ? ($subclass, $uri) : $subclass
            if $subclass->should_handle($uri);
    }

    ThrowUserError('bug_url_invalid', { url => $value });
}

sub _check_class {
    my ($class, $subclass) = @_;
    eval "use $subclass"; die $@ if $@;
    return $subclass;
}

sub _check_bug_id {
    my ($class, $bug_id) = @_;

    my $bug;
    if (blessed $bug_id) {
        # We got a bug object passed in, use it
        $bug = $bug_id;
        $bug->check_is_visible;
    }
    else {
        # We got a bug id passed in, check it and get the bug object
        $bug = Bugzilla::Bug->check({ id => $bug_id });
    }

    return $bug->id;
}

sub _check_value {
    my ($class, $uri) = @_;

    my $value = $uri->as_string;

    if (!$value) {
        ThrowCodeError('param_required',
                       { function => 'add_see_also', param => '$value' });
    }

    # We assume that the URL is an HTTP URL if there is no (something):// 
    # in front.
    if (!$uri->scheme) {
        # This works better than setting $uri->scheme('http'), because
        # that creates URLs like "http:domain.com" and doesn't properly
        # differentiate the path from the domain.
        $uri = new URI("http://$value");
    }
    elsif ($uri->scheme ne 'http' && $uri->scheme ne 'https') {
        ThrowUserError('bug_url_invalid', { url => $value, reason => 'http' });
    }

    # This stops the following edge cases from being accepted:
    # * show_bug.cgi?id=1
    # * /show_bug.cgi?id=1
    # * http:///show_bug.cgi?id=1
    if (!$uri->authority or $uri->path !~ m{/}) {
        ThrowUserError('bug_url_invalid',
                       { url => $value, reason => 'path_only' });
    }

    if (length($uri->path) > MAX_BUG_URL_LENGTH) {
        ThrowUserError('bug_url_too_long', { url => $uri->path });
    }

    return $uri;
}

1;

=head1 B<Methods in need of POD>

=over

=item should_handle

=item class_for

=item class

=item bug_id

=back
