<?% config.freshness.owner = 'minyue' %?>
<?% config.freshness.reviewed = '2021-04-13' %?>

# The WebRTC Audio Coding Module

WebRTC audio coding module can handle both audio sending and receiving. Folder
[`acm2`][acm2] contains implementations of the APIs.

*   Audio Sending Audio frames, each of which should always contain 10 ms worth
    of data, are provided to the audio coding module through
    [`Add10MsData()`][Add10MsData]. The audio coding module uses a provided
    audio encoder to encoded audio frames and deliver the data to a
    pre-registered audio packetization callback, which is supposed to wrap the
    encoded audio into RTP packets and send them over a transport. Built-in
    audio codecs are included the [`codecs`][codecs] folder. The
    [audio network adaptor][ANA] provides an add-on functionality to an audio
    encoder (currently limited to Opus) to make the audio encoder adaptive to
    network conditions (bandwidth, packet loss rate, etc).

*   Audio Receiving Audio packets are provided to the audio coding module
    through [`IncomingPacket()`][IncomingPacket], and are processed by an audio
    jitter buffer ([NetEq][NetEq]), which includes decoding of the packets.
    Audio decoders are provided by an audio decoder factory. Decoded audio
    samples should be queried by calling [`PlayoutData10Ms()`][PlayoutData10Ms].

[acm2]: https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/modules/audio_coding/acm2/;drc=854d59f7501aac9e9bccfa7b4d1f7f4db7842719
[Add10MsData]: https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/modules/audio_coding/include/audio_coding_module.h;l=136;drc=d82a02c837d33cdfd75121e40dcccd32515e42d6
[codecs]: https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/modules/audio_coding/codecs/;drc=883fea1548d58e0080f98d66fab2e0c744dfb556
[ANA]: https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/modules/audio_coding/audio_network_adaptor/;drc=1f99551775cd876c116d1d90cba94c8a4670d184
[IncomingPacket]: https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/modules/audio_coding/include/audio_coding_module.h;l=192;drc=d82a02c837d33cdfd75121e40dcccd32515e42d6
[NetEq]: https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/modules/audio_coding/neteq/;drc=213dc2cfc5f1b360b1c6fc51d393491f5de49d3d
[PlayoutData10Ms]: https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/modules/audio_coding/include/audio_coding_module.h;l=216;drc=d82a02c837d33cdfd75121e40dcccd32515e42d6
