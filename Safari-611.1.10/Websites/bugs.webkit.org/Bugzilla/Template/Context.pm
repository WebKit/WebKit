# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

# This exists to implement the template-before_process hook.
package Bugzilla::Template::Context;

use 5.10.1;
use strict;
use warnings;

use parent qw(Template::Context);

use Bugzilla::Hook;
use Scalar::Util qw(blessed);

sub process {
    my $self = shift;
    # We don't want to run the template_before_process hook for
    # template hooks (but we do want it to run if a hook calls
    # PROCESS inside itself). The problem is that the {component}->{name} of
    # hooks is unreliable--sometimes it starts with ./ and it's the
    # full path to the hook template, and sometimes it's just the relative
    # name (like hook/global/field-descs-end.none.tmpl). Also, calling
    # template_before_process for hook templates doesn't seem too useful,
    # because that's already part of the extension and they should be able
    # to modify their hook if they want (or just modify the variables in the
    # calling template).
    if (not delete $self->{bz_in_hook}) {
        $self->{bz_in_process} = 1;
    }
    my $result = $self->SUPER::process(@_);
    delete $self->{bz_in_process};
    return $result;
}

# This method is called by Template-Toolkit exactly once per template or
# block (look at a compiled template) so this is an ideal place for us to
# modify the variables before a template or block runs.
#
# We don't do it during Context::process because at that time
# our stash hasn't been set correctly--the parameters we were passed
# in the PROCESS or INCLUDE directive haven't been set, and if we're
# in an INCLUDE, the stash is not yet localized during process().
sub stash {
    my $self = shift;
    my $stash = $self->SUPER::stash(@_);

    my $name = $stash->{component}->{name};
    my $pre_process = $self->config->{PRE_PROCESS};

    # Checking bz_in_process tells us that we were indeed called as part of a
    # Context::process, and not at some other point. 
    #
    # Checking $name makes sure that we're processing a file, and not just a
    # block, by checking that the name has a period in it. We don't allow
    # blocks because their names are too unreliable--an extension could have
    # a block with the same name, or multiple files could have a same-named
    # block, and then your extension would malfunction.
    #
    # We also make sure that we don't run, ever, during the PRE_PROCESS
    # templates, because if somebody calls Throw*Error globally inside of
    # template_before_process, that causes an infinite recursion into
    # the PRE_PROCESS templates (because Bugzilla, while inside 
    # global/intialize.none.tmpl, loads the template again to create the
    # template object for Throw*Error).
    #
    # Checking Bugzilla::Hook::in prevents infinite recursion on this hook.
    if ($self->{bz_in_process} and $name =~ /\./
        and !grep($_ eq $name, @$pre_process)
        and !Bugzilla::Hook::in('template_before_process'))
    {
        Bugzilla::Hook::process("template_before_process",
                                { vars => $stash, context => $self,
                                  file => $name });
    }

    # This prevents other calls to stash() that might somehow happen
    # later in the file from also triggering the hook.
    delete $self->{bz_in_process};

    return $stash;
}

sub filter {
    my ($self, $name, $args) = @_;
    # If we pass an alias for the filter name, the filter code is cached
    # instead of looking for it at each call.
    # If the filter has arguments, then we can't cache it.
    $self->SUPER::filter($name, $args, $args ? undef : $name);
}

# We need a DESTROY sub for the same reason that Bugzilla::CGI does.
sub DESTROY {
    my $self = shift;
    $self->SUPER::DESTROY(@_);
};

1;

=head1 B<Methods in need of POD>

=over

=item stash

=item filter

=item process

=back
