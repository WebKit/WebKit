# Inband Comfort Noise

**Name:** "Inband Comfort Noise" ; "RTP Header Extension to signal inband comfort noise"

**Formal name:** <http://www.webrtc.org/experiments/rtp-hdrext/inband-cn>

**Status:** This extension is defined here to allow for experimentation. Once experience has shown that it is useful, we intend to make a proposal based on it for standardization in the IETF.

## Introduction

Comfort noise \(CN\) is widely used in real time communication, as it significantly reduces the frequency of RTP packets, and thus saves the network bandwidth, when participants in the communication are constantly actively speaking.

One way of deploying CN is through \[RFC 3389\]. It defines CN as a special payload, which needs to be encoded and decoded independently from the codec\(s\) applied to active speech signals. This deployment is referred to as outband CN in this context.

Some codecs, for example RFC 6716: Definition of the Opus Audio Codec, implement their own CN schemes. Basically, the encoder can notify that a CN packet is issued and/or no packet needs to be transmitted.

Since CN packets have their particularities, cloud and client may need to identify them and treat them differently. Special treatments on CN packets include but are not limited to

* Upon receiving multiple streams of CN packets, choose only one to relay or mix.
* Adapt jitter buffer wisely according to the discontinuous transmission nature of CN packets.

While RTP packets that contain outband CN can be easily identified as they bear a different payload type, inband CN cannot. Some codecs may be able to extract the information by decoding the packet, but that depends on codec implementation, not even mentioning that decoding packets is not always feasible. This document proposes using an RTP header extension to signal the inband CN.

## RTP header extension format

The inband CN extension can be encoded using either the one-byte or two-byte header defined in \[RFC 5285\]. Figures 1 and 2 show encodings with each of these header formats.

     0                   1
     0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |  ID   | len=0 |N| noise level |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

Figure 1. Encoding Using the One-Byte Header Format

     0                   1                   2
     0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |      ID       |     len=1     |N| noise level |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

Figure 2. Encoding Using the Two-Byte Header Format

Noise level is an optional data. The bit "N" being 1 indicates that there is a noise level. The noise level is defined the same way as the audio level in \[RFC 6464\] and therefore can be used to avoid the Audio Level Header Extension on the same RTP packet. This also means that this level is defined the same as the noise level in \[RFC 3389\] and therfore can be compared against outband CN.

## Further details

The existence of this header extension in an RTP packet indicates that it has inband CN, and therefore it will be used sparsely, and results in very small transmission cost.

The end receiver can utilize this RTP header extension to get notified about an upcoming discontinuous transmission. This can be useful for its jitter buffer management. This RTP header extension signals comfort noise, it can also be used by audio mixer to mix streams wisely. As an example, it can avoid mixing multiple comfort noises together.

Cloud may have the benefits of this RTP header extension as an end receiver, if it does transcoding. It may also utilize this RTP header extension to prioritize RTP packets if it does packet filtering. In both cases, this RTP header extension should not be encrypted.

## References
* \[RFC 3389\] Zopf, R., "Real-time Transport Protocol \(RTP\) Payload for Comfort Noise \(CN\)", RFC 3389, September 2002.
* \[RFC 6465\] Ivov, E., Ed., Marocco, E., Ed., and J. Lennox, "A Real-time Transport Protocol \(RTP\) Header Extension for Mixer-to-Client Audio Level Indication", RFC 6465, December 2011.
* \[RFC 5285\] Singer, D. and H. Desineni, "A General Mechanism for RTP Header Extensions", RFC 5285, July 2008.
