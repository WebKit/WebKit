# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

package Bugzilla::Extension::BmpConvert;

use 5.10.1;
use strict;
use warnings;

use parent qw(Bugzilla::Extension);

use Image::Magick;

our $VERSION = '1.0';

sub attachment_process_data {
    my ($self, $args) = @_;
    return unless $args->{attributes}->{mimetype} eq 'image/bmp';

    my $data = ${$args->{data}};
    my $img = Image::Magick->new(magick => 'bmp');

    # $data is a filehandle.
    if (ref $data) {
        $img->Read(file => \*$data);
        $img->set(magick => 'png');
        $img->Write(file => \*$data);
    }
    # $data is a blob.
    else {
        $img->BlobToImage($data);
        $img->set(magick => 'png');
        $data = $img->ImageToBlob();
    }
    undef $img;

    ${$args->{data}} = $data;
    $args->{attributes}->{mimetype} = 'image/png';
    $args->{attributes}->{filename} =~ s/^(.+)\.bmp$/$1.png/i;
}

 __PACKAGE__->NAME;
