<!-- go/cmark -->
<!--* freshness: {owner: 'alessiob' reviewed: '2021-04-21'} *-->

# The WebRTC Audio Mixer Module

The WebRTC audio mixer module is responsible for mixing multiple incoming audio
streams (sources) into a single audio stream (mix). It works with 10 ms frames,
it supports sample rates up to 48 kHz and up to 8 audio channels. The API is
defined in
[`api/audio/audio_mixer.h`](https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/api/audio/audio_mixer.h)
and it includes the definition of
[`AudioMixer::Source`](https://source.chromium.org/search?q=symbol:AudioMixer::Source%20file:third_party%2Fwebrtc%2Fapi%2Faudio%2Faudio_mixer.h),
which describes an incoming audio stream, and the definition of
[`AudioMixer`](https://source.chromium.org/search?q=symbol:AudioMixer%20file:third_party%2Fwebrtc%2Fapi%2Faudio%2Faudio_mixer.h),
which operates on a collection of
[`AudioMixer::Source`](https://source.chromium.org/search?q=symbol:AudioMixer::Source%20file:third_party%2Fwebrtc%2Fapi%2Faudio%2Faudio_mixer.h)
objects to produce a mix.

## AudioMixer::Source

A source has different characteristic (e.g., sample rate, number of channels,
muted state) and it is identified by an SSRC[^1].
[`AudioMixer::Source::GetAudioFrameWithInfo()`](https://source.chromium.org/search?q=symbol:AudioMixer::Source::GetAudioFrameWithInfo%20file:third_party%2Fwebrtc%2Fapi%2Faudio%2Faudio_mixer.h)
is used to retrieve the next 10 ms chunk of audio to be mixed.

[^1]: A synchronization source (SSRC) is the source of a stream of RTP packets,
    identified by a 32-bit numeric SSRC identifier carried in the RTP header
    so as not to be dependent upon the network address (see
    [RFC 3550](https://tools.ietf.org/html/rfc3550#section-3)).

## AudioMixer

The interface allows to add and remove sources and the
[`AudioMixer::Mix()`](https://source.chromium.org/search?q=symbol:AudioMixer::Mix%20file:third_party%2Fwebrtc%2Fapi%2Faudio%2Faudio_mixer.h)
method allows to generates a mix with the desired number of channels.

## WebRTC implementation

The interface is implemented in different parts of WebRTC:

*   [`AudioMixer::Source`](https://source.chromium.org/search?q=symbol:AudioMixer::Source%20file:third_party%2Fwebrtc%2Fapi%2Faudio%2Faudio_mixer.h):
    [`audio/audio_receive_stream.h`](https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/audio/audio_receive_stream.h)
*   [`AudioMixer`](https://source.chromium.org/search?q=symbol:AudioMixer%20file:third_party%2Fwebrtc%2Fapi%2Faudio%2Faudio_mixer.h):
    [`modules/audio_mixer/audio_mixer_impl.h`](https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/modules/audio_mixer/audio_mixer_impl.h)

[`AudioMixer`](https://source.chromium.org/search?q=symbol:AudioMixer%20file:third_party%2Fwebrtc%2Fapi%2Faudio%2Faudio_mixer.h)
is thread-safe. The output sample rate of the generated mix is automatically
assigned depending on the sample rate of the sources; whereas the number of
output channels is defined by the caller[^2]. Samples from the non-muted sources
are summed up and then a limiter is used to apply soft-clipping when needed.

[^2]: [`audio/utility/channel_mixer.h`](https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/audio/utility/channel_mixer.h)
    is used to mix channels in the non-trivial cases - i.e., if the number of
    channels for a source or the mix is greater than 3.
