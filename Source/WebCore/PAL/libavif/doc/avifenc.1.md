% AVIFENC(1) | General Commands Manual
%
% 2022-04-30

<!--
This man page is written in pandoc's Markdown.
See: https://pandoc.org/MANUAL.html#pandocs-markdown
-->

# NAME

avifenc - compress an image file to an AVIF file

# SYNOPSIS

**avifenc** [_options_] _input._[_jpg_|_jpeg_|_png_|_y4m_] _output.avif_

# DESCRIPTION

**avifenc** compresses an image file to an AVIF file.
Input format can be either JPEG, PNG or YUV4MPEG2 (Y4M).

# OPTIONS

**-h**, **\--help**
:   Show syntax help.

**-V**, **\--version**
:   Show the version number.

**-j**, **\--jobs** _J_
:   Number of jobs (worker threads).
    1 or less means single-threaded.
    Default is 1.
    Use **all** to use all available cores.

**-o**, **\--output** _FILENAME_
:   Instead of using the last filename given as output, use this filename.

**-l**, **\--lossless**
:   Set all defaults to encode losslessly, and emit warnings when
    settings/input don't allow for it.

**-d**, **\--depth** _D_
:   Output depth.
    This is available if the input format is JPEG/PNG, and for y4m or stdin,
    depth is retained.

    Possible values are:

    :   - **8**
        - **10**
        - **12**

**-y**, **\--yuv** _FORMAT_
:   Output format.
    Ignored for y4m or stdin (y4m format is retained).
    For JPEG, auto honors the JPEG's internal format, if possible.
    For all other cases, auto defaults to 444.

    Possible values are:

    :   - **auto** (default)
        - **444**
        - **422**
        - **420**
        - **400**

**-p**, **\--premultiply**
:   Premultiply color by the alpha channel and signal this in the AVIF.

**\--sharpyuv**
:   Use sharp RGB to YUV420 conversion (if supported). Ignored for y4m or if
    output is not 420.

**\--stdin**
:   Read y4m frames from stdin instead of files.
    No input filenames allowed, must be set before specifying the output
    filename.

**\--cicp**, **\--nclx** *P***/***T***/***M*
:   Specify CICP values (nclx colr box) by 3 raw numbers.
    Use **2** for any you wish to leave unspecified.

    - _P_ = color primaries
    - _T_ = transfer characteristics
    - _M_ = matrix coefficients

**-r**, **\--range** _RANGE_
:   YUV range.
    This is available if the input format is JPEG/PNG, and for y4m or stdin,
    range is retained.

    Possible values are:

    :   - **full**, **f** (default)
        - **limited**, **l**

**\--min** _Q_
:   Set min quantizer for color.
    Possible values are in the range **0**-**63**, where 0 is lossless.

**\--max** _Q_
:   Set max quantizer for color.
    Possible values are in the range **0**-**63**, where 0 is lossless.

**\--minalpha** _Q_
:   Set min quantizer for alpha.
    Possible values are in the range **0**-**63**, where 0 is lossless.

**\--maxalpha** _Q_
:   Set max quantizer for alpha.
    Possible values are in the range **0**-**63**, where 0 is lossless.

**\--tilerowslog2** _R_
:   Set log2 of number of tile rows.
    Possible values are in the range **0**-**6**.
    Default is 0.

**\--tilecolslog2** _C_
:   Set log2 of number of tile columns.
    Possible values are in the range **0**-**6**.
    Default is 0.

**\--autotiling**
:   Set **\--tilerowslog2** and **\--tilecolslog2** automatically.

**-g**, **\--grid** *M***x***N*
:   Encode a single-image grid AVIF with _M_ cols and _N_ rows.
    Either supply MxN images of the same width, height and depth, or a single
    image that can be evenly split into the MxN grid and follow AVIF grid image
    restrictions.
    The grid will adopt the color profile of the first image supplied.
    Possible values for _M_ and _N_ are in the range **1**-**256**.

**-s**, **\--speed** _S_
:   Encoder speed.
    Default is 6.

    Possible values are:

    :   - **0**-**10** (slowest-fastest)
        - **default**, **d** (codec internal defaults)

**-c**, **\--codec** _C_
:   AV1 codec to use.
    Possible values depend on the codecs enabled at build time (see **\--help**
    or **\--version** for the available codecs).
    Default is auto-selected from the available codecs.

    Possible values are:

    :   - **aom**
        - **rav1e**
        - **svt**

**\--exif** _FILENAME_
:   Provide an Exif metadata payload to be associated with the primary item
    (implies --ignore-exif).

**\--xmp** _FILENAME_
:   Provide an XMP metadata payload to be associated with the primary item
    (implies --ignore-xmp).

**\--icc** _FILENAME_
:   Provide an ICC profile payload to be associated with the primary item
    (implies --ignore-icc).

**-a**, **\--advanced** _KEY_[_=VALUE_]
:   Pass an advanced, codec-specific key/value string pair directly to the
    codec.
    **avifenc** will warn on any unused by the codec.
    The aom-specific advanced options can be used if the AOM codec is available
    (see **\--help** for details).

**\--duration** _D_
:   Set all following frame durations (in timescales) to _D_.
    Can be set multiple times (before supplying each filename).
    Default is 1.

**\--timescale**, **\--fps** _V_
:   Set the timescale to _V_.
    If all frames are 1 timescale in length, this is equivalent to frames per
    second.
    If neither duration nor timescale are set, **avifenc** will attempt to use
    the framerate stored in a y4m header, if present.
    Default is 30.

**-k**, **\--keyframe** _INTERVAL_
:   Set the forced keyframe interval (maximum frames between keyframes).
    Set to **0** to disable.
    Default is 0.

**\--ignore-exif**
:   If the input file contains embedded Exif metadata, ignore it (no-op if
    absent).

**\--ignore-xmp**
:   If the input file contains embedded XMP metadata, ignore it (no-op if
    absent).

**\--ignore-icc**
:   If the input file contains an embedded ICC profile, ignore it (no-op if
    absent).

**\--pasp** *H***,***V*
:   Add pasp property (aspect ratio).

    - _H_ = horizontal spacing
    - _V_ = vertical spacing

**\--crop** *CROPX***,***CROPY***,***CROPW***,***CROPH*
:   Add clap property (clean aperture), but calculated from a crop rectangle.

    - _CROPX_ = X-axis of a crop rectangle
    - _CROPY_ = Y-axis of a crop rectangle
    - _CROPW_ = width of a crop rectangle
    - _CROPH_ = height of a crop rectangle

**\--clap** *WN***,***WD***,***HN***,***HD***,***HON***,***HOD***,***VON***,***VOD*
:   Add clap property (clean aperture).

    - _WN_ = numerator of width
    - _WD_ = denominator of width
    - _HN_ = numerator of height
    - _HD_ = denominator of height
    - _HON_ = numerator of horizontal offset
    - _HOD_ = denominator of horizontal offset
    - _VON_ = numerator of vertical offset
    - _VOD_ = denominator of vertical offset

**\--irot** _ANGLE_
:   Add irot property (rotation).
    Possible values are in the range **0**-**3**, and makes (90 * _ANGLE_)
    degree rotation anti-clockwise.

**\--imir** _MODE_
:   Add imir property (mirroring).

    Note: Rotation is applied before mirroring at rendering.

    Possible values are:

    :   - **0** (top-to-bottom)
        - **1** (left-to-right)

**\--**
:   Signals the end of options. Everything after this is interpreted as file names.

# EXAMPLES

Compress a PNG file to an AVIF file:
:   $ **avifenc input.png output.avif**

# REPORTING BUGS

Bugs can be reported on GitHub at:
:   <https://github.com/AOMediaCodec/libavif/issues>

# SEE ALSO

**avifdec**(1)
