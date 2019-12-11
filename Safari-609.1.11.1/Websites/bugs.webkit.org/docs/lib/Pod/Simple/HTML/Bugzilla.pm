# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

package Pod::Simple::HTML::Bugzilla;

use 5.10.1;
use strict;
use warnings;

use parent qw(Pod::Simple::HTML);

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
