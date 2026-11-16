# Corruption Detection

**Name:**
"Corruption Detection"; "Extension for Automatic Detection of Video Corruptions"

**Formal name:**
<http://www.webrtc.org/experiments/rtp-hdrext/corruption-detection>

**Status:** This extension is defined here to allow for experimentation.

**Contact:** <sprang@google.com>

NOTE: This explainer is a work in progress and may change without notice.

The Corruption Detection (sometimes referred to as automatic corruption
detection or ACD) extension is intended to be a part of a system that allows
estimating a likelihood that a video transmission is in a valid state. That is,
the input to the video encoder on the send side corresponds to the output of the
video decoder on the receive side with the only difference being the expected
distortions from lossy compression.

The goal is to be able to detect outright coding errors caused by things such as
bugs in encoder/decoders, malformed packetization data, incorrect relay
decisions in SFU-type servers, incorrect handling of packet loss/reordering, and
so forth. We want to accomplish this with a high signal-to-noise ratio while
consuming a minimum of resources in terms of bandwidth and/or computation. It
should be noted that it is _not_ a goal to be able to e.g. gauge general video
quality using this method.

This explainer contains two parts:

1) A definition of the RTP header extension itself and how it is to be parsed.
2) The intended usage and implementation details for a WebRTC sender and
   receiver respectively.

If this extension has been negotiated, all the client behavior outlined in this
doc MUST be adhered to.

## RTP Header Extension Format

### Data Layout Overview

The message format of the header extension:

      0                   1                   2                   3
      0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     |B|  seq index  |    std dev    | Y err | UV err|    sample 0   |
     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     |    sample 1   |   sample 2    |    …   up to sample <=12
     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

### Data Layout Details

* B (1 bit): If the sequence number should be interpreted as the MSB or LSB
  of the full size 14 bit sequence index described in the next point.
* seq index (7 bits): The index into the Halton sequence (used to locate
  where the samples should be drawn from).
  * If B is set: the 7 most significant bits of the true index. The 7 least
    significant bits of the true index shall be interpreted as 0. This is
    because this is the point where we can guarantee that the sender and
    receiver has the same full index. B MUST be set on keyframes. On droppable
    frames B MUST NOT be set.
  * If B is not set: The 7 LSB of the true index. The 7 most significant bits
    should be inferred based on the most recent message.
* std dev (8 bits):  The standard deviation of the Gaussian filter used
  to weigh the samples. The value is scaled using a linear map:
  0 = 0.0 to 255 = 40.0. A std dev of 0 is interpreted as directly using
  just the sample value at the desired coordinate, without any weighting.
* Y err (4 bits): The allowed error for the luma channel.
* UV err (4 bits): The allowed error for the chroma channels.
* Sample N (8 bits): The N:th filtered sample from the input image. Each
  sample represents a new point in one of the image planes, the plane and
  coordinates being determined by index into the Halton sequence (starting at
  seq# index and is incremented by one for each sample). Each sample has gone
  through a Gaussian filter with the std dev specified above. The samples
  have been floored to the nearest integer.

A special case is the so-called "synchronization" message. Such a message
only contains the first byte. They are used to keep the sender and receiver in
sync even if no "full" message has been received for a while. Such messages
MUST NOT be sent on droppable frames.

### A note on encryption

Privacy and security are core parts of nearly every WebRTC-based application,
which means that some sort of encryption needs to be present. The most common
form of encryption is SRTP, defined in RFC 3711. However, as mentioned in
section 9.4 of that RFC, RTP header extensions are considered part of the header
and are thus not encrypted.

The automatic corruption detection header extension is different from most other
header extensions in that it provides not only metadata about the media stream
being transmitted but in practice comprises an extremely sparse representation
of the actual video stream itself. Given a static scene and enough time, a crude
image of the encrypted video can rather trivially be constructed.

As such, most applications should use this extension with SRTP only if
additional security is present to protect it. That could be for example in the
form of explicit header extension encryption provided by RFC 6904/RFC 9335, or
by encapsulating the entire RTP stream in an additional layer such as IPSec. 

## Usage & Guidelines

In this section we’ll first look at a general overview of the intended usage of
this header extensions, followed by more details around the expected
implementation.

### Overview

The premise of the extension described here is that we can validate the state of
the video pipeline by quasi-randomly selecting a few samples from the raw input
frame to an encoder, and then checking them against the output of a decoder.
Assuming that a lossless codec is used we can follow these steps:

1) In an image that is to be encoded, quasi-randomly select N sampling positions
   and store the samples values for those positions from the raw input image. 
2) Encode the image, and attach the selected sample values to the RTP packets
   containing the encoded bitstream of that image.
3) Transmit the RTP packets to a remote receiver.
4) At the receiver, collect the attached sample values from the RTP packets when
   assembling the frame, and then pass the bitstream to a decoder.
5) Using the same quasi-random sequence as in (1), calculate the corresponding N
   sampling positions.
6) Take the output of the decoder and check the values of the samples from the
   RTP packets. If they differ significantly, it is likely that an image
   corruption has occurred.

Lossless encoding is however rarely used in practice, and that introduces
problems for the above algorithm.

* Quantization causes values to be different from the desired value.
* Whole blocks of pixels might be shifted somewhat due to inaccuracies in motion
  vectors.
* Inaccuracies caused by in-loop or post-process filtering.
* etc.

We must therefore take these distortions into consideration, as they are merely
a natural side-effect of the compression and their effect is not to be
considered an “invalid state”. We aim to accomplish this using two tools.

First, instead of a sample being a single raw sample value let it be a filtered
one: a weighted average of samples in the vicinity of the desired location, with
the weights being a 2D Gaussian centered at that location and the variance
adjusted depending on the magnitude of the expected distortions
(higher distortion => higher variance). This smoothes out inaccuracies caused by
both quantization and motion compensation.

Secondly, even with a very large filter kernel the new sample might not converge
towards the exact desired value. For that reason, set an “allowed error
threshold” that removes small magnitude differences. Since chroma and luma
channels have different scales, separate error thresholds are available for
them.

### Sequence Index Handling

The quasi-random sequence of choice for this extension is a 2D
[Halton Sequence](https://en.wikipedia.org/wiki/Halton_sequence).

The index into the Halton Sequence is indicated by the header extension and
results in a 14 bit unsigned integer which on overflow will wrap around back to
0.

For each sample contained within the extension, the sequence index should be
considered to be incremented by one. Thus the sequence index at the start of the
header should be considered “the sequence index for the next sample to be
drawn”.

The ACD extension may be sent containing either the 7 most significant bits
(B = true) or the 7 least significant bits (B = false) of the sequence index.

Key-frames MUST be populated with the ACD extension, and those MUST use B = true
indicating only the 7 most significant bits are transmitted.

The sender may choose any arbitrary starting point. The biggest reason to not
always start with (B = true, seq index = 0) is that with frequent/periodic
keyframes you might end up always sampling the same small subset of image
locations over and over.

If B = false and the LSB seq index + number of samples exceeds the capacity of
the 7-bit field (i.e. > 0x7F), then the most significant bits of the 14 bit
sequence counter should be considered to be implicitly incremented by the
overflow.

Delta-frames may be encoded as “droppable” or “non-droppable”. Consider for
example temporal layering using the
[L1T3](https://www.w3.org/TR/webrtc-svc/#L1T3*) mode. In that scenario,
key-frames and all T0 frames are non-droppable, while all T1 and T2 frames are
droppable.

For non-droppable frames, B MAY be set to true even though there is often little
utility for it.
For droppable frames B MUST NOT be set to true, since a receiver could otherwise
easily end up out of sync with the sender.

A receiver must store a state containing the last sequence index used. If an ACD
extension is receiver with B = false but the LSB does not match the last known
sequence index state, this indicates that an instrumented frame has been
dropped. The receiver should recover from this by incrementing the last known
sequence index until the 7 least significant bits match.

Because of this, the sender MUST send ACD messages on non-droppable frames such
that the delta between their sequence indexing (from the last sample of the
previous packet to the first of the next) indexing does not exceed 0x7F. A
synchronization message may be used for this purpose if there is no wish to
instrument the non-droppable frame.

It is not required to add the ACD extension to every frame. Indeed, for
performance reasons it may be reasonable to only instrument a small subset of
frames, for example using only one frame per second.

Additionally, when encoding a structure that has independent decode targets
(e.g. L3T3_KEY) - the sender should generate an independent stream ACD sequence
per target resolution so that a receiver can validate the state of the
sub-stream they receive.

// TODO: Add concrete examples.

### Sample Selection

As mentioned above, a Halton Sequence is used to generate sampling coordinates.
Base 2 is used for selecting the rows, and base 3 is used for selecting columns.

Each sample in the ACD extension represents a single image sample, meaning it
belongs to a single channel rather than e.g. being an RGB pixel.

The initial version of the ACD extension supports only the I420 chroma
subsampling format. When determining which plane a location belongs to, it is
easiest to visualize it as the chroma planes being “stacked” to the side of the
luma plane:

    +------+---+
    |      | U |
    +  Y   +---+
    |      | V |
    +------+---+

In pseudo code:
```
  row = GetHaltonSequence(seq_index, /*base=*/2) * image_height;
  col = GetHaltonSequence(seq_index, /*base=*/3) * image_width * 1.5;

  if (col < image_width) {
    HandleSample(Y_PLANE, row, col);
  } else if (row < image_height / 2) {
    HandleSample(U_PLANE, row, col - image_width);
  } else {
    HandleSample(V_PLANE, row - (image_height / 2), col - image_width);
  }

  seq_index++;
```
Support for other layout types may be added in later versions of this extension.

Note that the image dimensions are not explicitly a part of the ACD extension -
that has to be inferred from the raw image itself.

### Sample Filtering

As mentioned above, when filtering a sample we create a weighted average around
the desired location. Only samples in the same plane are considered. The
weighting consists of a 2D Gaussian centered on the desired location, with the
standard deviation specified in the ACD extension header.

If the standard deviation is specified as 0.0 - we consider only a singular
sample. Otherwise, we first determine a cutoff distance below which the weights
are considered too small to matter. For now, we have set the weight cutoff to
0.2 - meaning the maximum distance from the center sample we need to consider is
max_d = ceil(sqrt(-2.0 * ln(0.2) * stddev^2) - 1.

Any samples outside the plane are considered to have weight 0.

In pseudo-code, that means we get the following:
```
  sample_sum = 0;
  weight_sum = 0;
  for (y = max(0, row - max_d) to min(plane_height, row + max_d) {
    for (x = max(0, col - max_d) to min(plane_width, col + max_d) {
      weight = e^(-1 * ((y - row)^2 + (x - col)^2) / (2 * stddev^2));
      sample_sum += SampleAt(x, y) * weight;
      weight_sum += weight;
    }
  }
  filtered_sample = sample_sum / weight_sum;
```
### Receive Side Considerations

When a frame has been decoded and an ACD message is present, the receiver
performs the following steps:

* Update the sequence index so that it is consistent with the ACD message.
* Calculate the sample positions from the Halton sequence.
* Filter each sample of the decoded image using the standard deviation provided
  in the ACD message.

We then need to compare the actual samples present in the ACD message and the
samples generated from the locally decoded frame, and take the allowed error
into account:

```
for (i = 0 to num_samples) {
  // Allowed error from ACD message, depending on which plane sample i is in.
  allowed_error = SampleType(i) == Y_PLANE ? Y_ERR : UV_ERR;
  delta_i = max(0, abs(RemoteSample(i) - LocalSample(i)) - allowed_error);
}
```

It is then up to the receiver how to interpret these deltas. A suggested method
is to calculate a “corruption score” by calculating sum(delta(i)^2), where
delta(i) is the delta for i:th sample in the message, and then scaling and
capping that result to a maximum of 1.0. By squaring the sample, we make sure
that even singular samples that are way outside their expected values cause a
noticeable shift in the score. Another possible way is to calculate the distance
and cap it using a sigmoid function.

This extension message format does not make recommendations about what a
receiver should do with the corruption scores, but some possibilities are:

* Expose it as a statistics connected to the video receive stream. Let the
  application decide what to do with the information.
* Let the WebRTC application use a corruption signal to take proactive measures.
  E.g. request a key-frame in order to recover, or try to switch to another
  codec type or implementation.

### Determining Filter Settings & Error Thresholds

It is up to the sender to estimate how large the filter kernel and the allowed
error thresholds should be.

One method to do this is to analyze example outputs from different encoders and
map the average frame QP to suitable settings. There will of course have to be
different such mapping for e.g. AV1 compared to VP8 - but it’s also possible to
get “tighter” values with knowledge of the exact implementation used. E.g. a
mapping designed just for libaom encoder version X running with speed setting Y.

Another method is to use the actual reconstructor state from the encoder. That
of course means the encoder has to expose that state, which is not common.
A benefit of doing it that way is that the filter size and allowed error can be
very small (really only post-processing could introduce distortions in that
scenario). A drawback is if the reconstructed state already contains corruption
due to an encoder bug - then we would not be able to detect that corruption at
all.

There are also possibly more accurate but probably much more costly alternatives
as well, such as training an ML model to determine the settings based on both
the content of the source frame and any metadata present in the encoded
bitstream.

Regardless of method, the implementation at the send side SHOULD strive to set
the filter size and error thresholds such that 99.5% of filtered samples end up
with a delta <= the error threshold for that plane, based on a representative
set of test clips and bandwidth constraints.

Notes: The extension must not be present in more than 1 packet per video frame.
