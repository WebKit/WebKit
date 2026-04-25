# Color Space

The color space extension is used to communicate color space information and
optionally also metadata that is needed in order to properly render a high
dynamic range (HDR) video stream. Contact <kron@google.com> for more info.

**Name:** "Color space" ; "RTP Header Extension for color space"

**Formal name:** <http://www.webrtc.org/experiments/rtp-hdrext/color-space>

**Status:** This extension is defined here to allow for experimentation. Once experience
has shown that it is useful, we intend to make a proposal based on it for standardization
in the IETF.

## RTP header extension format

### Data layout overview
Data layout without HDR metadata (one-byte RTP header extension)
     1-byte header + 4 bytes of data:

      0                   1                   2                   3
      0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     |  ID   | L = 3 |   primaries   |   transfer    |    matrix     |
     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     |range+chr.sit. |
     +-+-+-+-+-+-+-+-+

Data layout of color space with HDR metadata (two-byte RTP header extension)
     2-byte header + 28 bytes of data:

      0                   1                   2                   3
      0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     |      ID       |   length=28   |   primaries   |   transfer    |
     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     |    matrix     |range+chr.sit. |         luminance_max         |
     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     |         luminance_min         |            mastering_metadata.|
     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     |primary_r.x and .y             |            mastering_metadata.|
     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     |primary_g.x and .y             |            mastering_metadata.|
     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     |primary_b.x and .y             |            mastering_metadata.|
     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     |white.x and .y                 |    max_content_light_level    |
     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     | max_frame_average_light_level |
     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

### Data layout details
The data is written in the following order,
Color space information (4 bytes):
  * Color primaries value according to ITU-T H.273 Table 2.
  * Transfer characteristic value according to ITU-T H.273 Table 3.
  * Matrix coefficients value according to ITU-T H.273 Table 4.
  * Range and chroma siting as specified at
    https://www.webmproject.org/docs/container/#colour. Range (range), horizontal (horz)
    and vertical (vert) siting are merged to one byte by the operation: (range << 4) +
    (horz << 2) + vert.

The extension may optionally include HDR metadata written in the following order,
Mastering metadata (20 bytes):
  * Luminance max, specified in nits, where 1 nit = 1 cd/m<sup>2</sup>.
    (16-bit unsigned integer)
  * Luminance min, scaled by a factor of 10000 and specified in the unit 1/10000
    nits. (16-bit unsigned integer)
  * CIE 1931 xy chromaticity coordinates of the primary red, scaled by a factor of 50000.
    (2x 16-bit unsigned integers)
  * CIE 1931 xy chromaticity coordinates of the primary green, scaled by a factor of 50000.
    (2x 16-bit unsigned integers)
  * CIE 1931 xy chromaticity coordinates of the primary blue, scaled by a factor of 50000.
    (2x 16-bit unsigned integers)
  * CIE 1931 xy chromaticity coordinates of the white point, scaled by a factor of 50000.
    (2x 16-bit unsigned integers)

Followed by max light levels (4 bytes):
  * Max content light level, specified in nits. (16-bit unsigned integer)
  * Max frame average light level, specified in nits. (16-bit unsigned integer)

Note, the byte order for all integers is big endian.

See the standard SMPTE ST 2086 for more information about these entities.

Notes: Extension should be present only in the last packet of video frames. If attached
to other packets it should be ignored.

