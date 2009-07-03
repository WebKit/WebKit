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
# The Initial Developer of the Original Code is Everything Solved.
# Portions created by Everything Solved are Copyright (C) 2006
# Everything Solved. All Rights Reserved.
#
# Contributor(s): Max Kanat-Alexander <mkanat@bugzilla.org>

package Pod::Simple::HTML::Bugzilla;

use strict;
use base qw(Pod::Simple::HTML);

# Without this constant, HTMLBatch will throw undef warnings.
use constant VERSION    => $Pod::Simple::HTML::VERSION;
use constant CODE_CLASS => ' class="code"';
use constant META_CT  => '<meta http-equiv="Content-Type" content="text/html;'
                         . ' charset=UTF-8">';
use constant DOCTYPE  => '<!DOCTYPE html PUBLIC "-//W3C//DTD HTML 4.01'
    . ' Transitional//EN" "http://www.w3.org/TR/html4/loose.dtd">';

sub new {
    my $self    = shift->SUPER::new(@_);

    my $doctype      = $self->DOCTYPE;
    my $content_type = $self->META_CT;

    my $html_pre_title = <<END_HTML;
$doctype
<html>
  <head>
    <title>
END_HTML

    my $html_post_title = <<END_HTML;
</title>
    $content_type
  </head>
  <body id="pod">
END_HTML

    $self->html_header_before_title($html_pre_title);
    $self->html_header_after_title($html_post_title);

    # Fix some tags to have classes so that we can adjust them.
    my $code = CODE_CLASS;
    $self->{'Tagmap'}->{'Verbatim'} = "\n<pre $code>";
    $self->{'Tagmap'}->{'VerbatimFormatted'} = "\n<pre $code>";
    $self->{'Tagmap'}->{'F'} = "<em $code>";
    $self->{'Tagmap'}->{'C'} = "<code $code>";

    # Don't put head4 tags into the Table of Contents. We have this
    delete $Pod::Simple::HTML::ToIndex{'head4'};

    return $self;
}

# Override do_beginning to put the name of the module at the top
sub do_beginning {
    my $self = shift;
    $self->SUPER::do_beginning(@_);
    print {$self->{'output_fh'}} "<h1>" . $self->get_short_title . "</h1>";
    return 1;
}

1;
