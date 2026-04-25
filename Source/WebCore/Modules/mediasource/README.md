# Media Source Extensions

This directory contains cross-platform implementations of the objects specified by the [Media Source Extensions Specification](https://www.w3.org/TR/media-source/).

## Basic Concepts

The Media Source Extensions specification defines a set of classes which allows clients to implement their own loading, buffering, and variant switching behavior, as opposed to requiring the UA to handle same.

Clients `fetch()` media initialization segments and media segments, typically subsets of a single [fragmented MP4 file](https://www.w3.org/TR/mse-byte-stream-format-isobmff/) or [WebM file](https://www.w3.org/TR/mse-byte-stream-format-webm/), and append those segments into a SourceBuffer object, which is associated with a HTMLMediaElement through a [MediaSource](#mediasource) object.

## Relevant Classes

### MediaSource ###

_([.idl](MediaSource.idl), [.h](MediaSource.h), [.cpp](MediaSource.cpp))_

MediaSource serves two purposes:
* Creating SourceBuffer objects.
* Associating those SourceBuffer objects with a HTMLMediaElement.

Once created, clients can create query for container and codec support via `isTypeSupported(type)`, [SourceBuffer](#sourcebuffer) objects via `addSourceBuffer(type)`, explicitly set the MediaSource's `duration`, and signal an end of the stream via `endOfStream(error)`.

Before creating any [SourceBuffer](#sourcebuffer) objects, the MediaSource must be associated with a HTMLMediaElement. The MediaSource can be set directly as the HTMLMediaElement's `srcObject`. Alternatively, an [extension](DOMURL+MediaSource.idl) to DOMURL allows an ObjectURL to be created from a MediaSource object, and that ObjectURL can be set as the HTMLMediaElement's `src`.

A MediaSource object will fire a `"sourceopen"` event when successfully associated with a HTMLMediaElement, and a `"sourceclose"` event when disassociated. The state of the MediaSource object can be queried via its `readyState` property.

### SourceBuffer ###

_([.idl](SourceBuffer.idl), [.h](SourceBuffer.h), [.cpp](SourceBuffer.cpp))_

SourceBuffer accepts buffers of initialization segments and media segments, which are then parsed into media tracks and media samples. Those samples are cached within the SourceBuffer (inside its [SourceBufferPrivate](../platform/graphics/SourceBufferPrivate.h) object) and enqueued into platform-specific decoders on demand. The primary storage mechanism for these samples is a [SampleMap](#samplemap), which orders those samples both in terms of each sample's DecodeTime and PresentationTime. These two times can differ for codecs that support frame reordering, typically MPEG video codecs such as h.264 and HEVC.

Clients append these segments via `appendBuffer()`, which sets an internal `updating` flag, fires the `"updatestart"` event, and subsequently fires the `"updateend"` event and clears the `updating` flag once parsing is complete. The results of the append are visible by querying the `buffered` property, or by querying the `audioTracks`, `videoTracks`, and `textTracks` TrackList objects.

### MediaSourcePrivate ###

_([.h](../platform/graphics/MediaSourcePrivate.h), [.cpp](../platform/graphics/MediaSourcePrivate.cpp))_

MediaSourcePrivate is an abstract base class which allows [MediaSource](#mediasource) to communicate through the platform boundary to a platform-specific implementation of MediaSource.

When the GPU Process is enabled, the MediaSourcePrivate in the WebContent process is typically a [MediaSourcePrivateRemote](../../WebKit/WebProcess/GPU/media/MediaSourcePrivateRemote.h), which will pass commands and properties across the WebContent/GPU process boundary.

For Apple ports, the MediaSourcePrivate is typically a [MediaSourcePrivateAVFObjC](../platform/graphics/avfoundation/objc/MediaSourcePrivateAVFObjC.h).

For GStreamer-based ports, the MediaSourcePrivate is typically a [MediaSourcePrivateGStreamer](../platform/graphics/gstreamer/mse/MediaSourcePrivateGStreamer.h).

When running in DumpRenderTree/WebKitTestRunner, a "mock" MediaSourcePrivate can be enabled, and a [MockMediaSourcePrivate](../platform/mock/mediasource/MockMediaSourcePrivate.h) can be created. This is useful for writing platform-independent tests which exercise the platform-independent [MediaSource](#mediasource) and [SourceBuffer](#sourcebuffer) objects directly.

### SourceBufferPrivate ###

_([.h](../platform/graphics/SourceBufferPrivate.h), [.cpp](../platform/graphics/SourceBufferPrivate.cpp))_

SourceBufferPrivate is a semi-abstract base class which accepts initialization segment and media segment buffers, parse those buffers with platform-specific parsers, and enqueue the resulting samples into platform-specific decoders. SourceBufferPrivate is also responsible for caching parsed samples in a [SampleMap](#samplemap).

### MediaTime ### 

_([.h](../../WTF/wtf/MediaTime.h), [.cpp](../../WTF/wtf/MediaTime.cpp))_

MediaTime is a rational-number time class for manipulating time values commonly found in media files. The unit of MediaTime is seconds.

Media containers such as mp4 and WebM represent time values as a ratio between a "time base" and a "time value". These values cannot necessarily be accurately represented as floating-point values without incurring cumulative rounding errors.  For example, a common frame rate in video formats is 29.97fps, however that value is an approximation of 30000/1001. So a media file containing a video track with a 29.97fps content will declare a "time base" scalar of 30000, and each frame will have a "time value" duration of 1001.

Media Source Extension algorithms are very sensitive to small gaps between samples, and due to its rational-number behavior, MediaTime guarantees samples are contiguous by avoiding floating-point rounding errors.

MediaTime offers convenience methods to convert from (`createTimeWithDouble()`) and to (`toDouble()`) floating-point values.

### MediaSample ###

_([.h](../platform/MediaSample.h), [.cpp](../platform/MediaSample.cpp))_

MediaSample is an abstract base class representing a sample parsed from a media segment. MediaSamples have `presentationTime()`, `decodeTime()`, and `duration()`, each of which are [MediaTime](#mediatime) values, which are used to order these samples relative to one another in a [SampleMap](#samplemap). For codecs which support frame reordering, `presentationTime()` and `decodeTime()` for each sample may differ.

### SampleMap ###

_([.h](SampleMap.h), [.cpp](SampleMap.cpp))_

SampleMap is a high-performance, binary-tree, storage structure for holding MediaSamples.

Because decoders typically require frames to be enqueued in decode-time order, but many of the Media Source Extension algorithms work in presentation-time order, SampleMap contains two binary-tree structures: a `decodeOrder()` map, and a `presentationOrder()` map.
