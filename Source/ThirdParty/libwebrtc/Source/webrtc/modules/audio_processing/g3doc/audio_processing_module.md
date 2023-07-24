<!-- go/cmark -->
<!--* freshness: {owner: 'peah' reviewed: '2021-04-13'} *-->

# Audio Processing Module (APM)

## Overview

The APM is responsible for applying speech enhancements effects to the
microphone signal. These effects are required for VoIP calling and some
examples include echo cancellation (AEC), noise suppression (NS) and
automatic gain control (AGC).

The API for APM resides in [`/modules/audio_processing/include`][https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/modules/audio_processing/include].
APM is created using the [`AudioProcessingBuilder`][https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/modules/audio_processing/include/audio_processing.h]
builder that allows it to be customized and configured.

Some specific aspects of APM include that:
*  APM is fully thread-safe in that it can be accessed concurrently from
   different threads.
*  APM handles for any input sample rates < 384 kHz and achieves this by
   automatic reconfiguration whenever a new sample format is observed.
*  APM handles any number of microphone channels and loudspeaker channels, with
   the same automatic reconfiguration as for the sample rates.


APM can either be used as part of the WebRTC native pipeline, or standalone.
