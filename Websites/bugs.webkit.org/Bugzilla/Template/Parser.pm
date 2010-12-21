# -*- Mode: perl; indent-tabs-mode: nil -*-
#
# The contents of this file are subject to the Mozilla Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
#
# The Original Code is the Bugzilla Bug Tracking System.
#
# The Initial Developer of the Original Code is Marc Schumann.
# Portions created by Marc Schumann are Copyright (C) 2008 Marc Schumann.
# All Rights Reserved.
#
# Contributor(s): Marc Schumann <wurblzap@gmail.com>


package Bugzilla::Template::Parser;

use strict;

use base qw(Template::Parser);

sub parse {
    my ($self, $text, @params) = @_;
    if (Bugzilla->params->{'utf8'}) {
        utf8::is_utf8($text) || utf8::decode($text);
    }
    return $self->SUPER::parse($text, @params);
}

1;

__END__

=head1 NAME

Bugzilla::Template::Parser - Wrapper around the Template Toolkit
C<Template::Parser> object

=head1 DESCRIPTION

This wrapper makes the Template Toolkit aware of UTF-8 templates.

=head1 SUBROUTINES

=over

=item C<parse($options)>

Description: Parses template text using Template::Parser::parse(),
converting the text to UTF-8 encoding before, if necessary.

Params:      C<$text> - Text to pass to Template::Parser::parse().

Returns:     Parsed text as returned by Template::Parser::parse().

=back

=head1 SEE ALSO

L<Template>
