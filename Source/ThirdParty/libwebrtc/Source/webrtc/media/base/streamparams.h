/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// This file contains structures for describing SSRCs from a media source such
// as a MediaStreamTrack when it is sent across an RTP session. Multiple media
// sources may be sent across the same RTP session, each of them will be
// described by one StreamParams object
// SsrcGroup is used to describe the relationship between the SSRCs that
// are used for this media source.
// E.x: Consider a source that is sent as 3 simulcast streams
// Let the simulcast elements have SSRC 10, 20, 30.
// Let each simulcast element use FEC and let the protection packets have
// SSRC 11,21,31.
// To describe this 4 SsrcGroups are needed,
// StreamParams would then contain ssrc = {10,11,20,21,30,31} and
// ssrc_groups = {{SIM,{10,20,30}, {FEC,{10,11}, {FEC, {20,21}, {FEC {30,31}}}
// Please see RFC 5576.

#ifndef MEDIA_BASE_STREAMPARAMS_H_
#define MEDIA_BASE_STREAMPARAMS_H_

#include <stdint.h>

#include <algorithm>
#include <set>
#include <string>
#include <vector>

#include "rtc_base/constructormagic.h"

namespace cricket {

extern const char kFecSsrcGroupSemantics[];
extern const char kFecFrSsrcGroupSemantics[];
extern const char kFidSsrcGroupSemantics[];
extern const char kSimSsrcGroupSemantics[];

struct SsrcGroup {
  SsrcGroup(const std::string& usage, const std::vector<uint32_t>& ssrcs)
      : semantics(usage), ssrcs(ssrcs) {}

  bool operator==(const SsrcGroup& other) const {
    return (semantics == other.semantics && ssrcs == other.ssrcs);
  }
  bool operator!=(const SsrcGroup &other) const {
    return !(*this == other);
  }

  bool has_semantics(const std::string& semantics) const;

  std::string ToString() const;

  std::string semantics;  // e.g FIX, FEC, SIM.
  std::vector<uint32_t> ssrcs;  // SSRCs of this type.
};

struct StreamParams {
  static StreamParams CreateLegacy(uint32_t ssrc) {
    StreamParams stream;
    stream.ssrcs.push_back(ssrc);
    return stream;
  }

  bool operator==(const StreamParams& other) const {
    return (groupid == other.groupid &&
            id == other.id &&
            ssrcs == other.ssrcs &&
            ssrc_groups == other.ssrc_groups &&
            type == other.type &&
            display == other.display &&
            cname == other.cname &&
            sync_label == other.sync_label);
  }
  bool operator!=(const StreamParams &other) const {
    return !(*this == other);
  }

  uint32_t first_ssrc() const {
    if (ssrcs.empty()) {
      return 0;
    }

    return ssrcs[0];
  }
  bool has_ssrcs() const {
    return !ssrcs.empty();
  }
  bool has_ssrc(uint32_t ssrc) const {
    return std::find(ssrcs.begin(), ssrcs.end(), ssrc) != ssrcs.end();
  }
  void add_ssrc(uint32_t ssrc) { ssrcs.push_back(ssrc); }
  bool has_ssrc_groups() const {
    return !ssrc_groups.empty();
  }
  bool has_ssrc_group(const std::string& semantics) const {
    return (get_ssrc_group(semantics) != NULL);
  }
  const SsrcGroup* get_ssrc_group(const std::string& semantics) const {
    for (std::vector<SsrcGroup>::const_iterator it = ssrc_groups.begin();
         it != ssrc_groups.end(); ++it) {
      if (it->has_semantics(semantics)) {
        return &(*it);
      }
    }
    return NULL;
  }

  // Convenience function to add an FID ssrc for a primary_ssrc
  // that's already been added.
  bool AddFidSsrc(uint32_t primary_ssrc, uint32_t fid_ssrc) {
    return AddSecondarySsrc(kFidSsrcGroupSemantics, primary_ssrc, fid_ssrc);
  }

  // Convenience function to lookup the FID ssrc for a primary_ssrc.
  // Returns false if primary_ssrc not found or FID not defined for it.
  bool GetFidSsrc(uint32_t primary_ssrc, uint32_t* fid_ssrc) const {
    return GetSecondarySsrc(kFidSsrcGroupSemantics, primary_ssrc, fid_ssrc);
  }

  // Convenience function to add an FEC-FR ssrc for a primary_ssrc
  // that's already been added.
  bool AddFecFrSsrc(uint32_t primary_ssrc, uint32_t fecfr_ssrc) {
    return AddSecondarySsrc(kFecFrSsrcGroupSemantics, primary_ssrc, fecfr_ssrc);
  }

  // Convenience function to lookup the FEC-FR ssrc for a primary_ssrc.
  // Returns false if primary_ssrc not found or FEC-FR not defined for it.
  bool GetFecFrSsrc(uint32_t primary_ssrc, uint32_t* fecfr_ssrc) const {
    return GetSecondarySsrc(kFecFrSsrcGroupSemantics, primary_ssrc, fecfr_ssrc);
  }

  // Convenience to get all the SIM SSRCs if there are SIM ssrcs, or
  // the first SSRC otherwise.
  void GetPrimarySsrcs(std::vector<uint32_t>* ssrcs) const;

  // Convenience to get all the FID SSRCs for the given primary ssrcs.
  // If a given primary SSRC does not have a FID SSRC, the list of FID
  // SSRCS will be smaller than the list of primary SSRCs.
  void GetFidSsrcs(const std::vector<uint32_t>& primary_ssrcs,
                   std::vector<uint32_t>* fid_ssrcs) const;

  std::string ToString() const;

  // Resource of the MUC jid of the participant of with this stream.
  // For 1:1 calls, should be left empty (which means remote streams
  // and local streams should not be mixed together).
  std::string groupid;
  // Unique per-groupid, not across all groupids
  std::string id;
  std::vector<uint32_t> ssrcs;         // All SSRCs for this source
  std::vector<SsrcGroup> ssrc_groups;  // e.g. FID, FEC, SIM
  // Examples: "camera", "screencast"
  std::string type;
  // Friendly name describing stream
  std::string display;
  std::string cname;  // RTCP CNAME
  std::string sync_label;  // Friendly name of cname.

 private:
  bool AddSecondarySsrc(const std::string& semantics,
                        uint32_t primary_ssrc,
                        uint32_t secondary_ssrc);
  bool GetSecondarySsrc(const std::string& semantics,
                        uint32_t primary_ssrc,
                        uint32_t* secondary_ssrc) const;
};

// A Stream can be selected by either groupid+id or ssrc.
struct StreamSelector {
  explicit StreamSelector(uint32_t ssrc) : ssrc(ssrc) {}

  StreamSelector(const std::string& groupid,
                 const std::string& streamid) :
      ssrc(0),
      groupid(groupid),
      streamid(streamid) {
  }

  bool Matches(const StreamParams& stream) const {
    if (ssrc == 0) {
      return stream.groupid == groupid && stream.id == streamid;
    } else {
      return stream.has_ssrc(ssrc);
    }
  }

  uint32_t ssrc;
  std::string groupid;
  std::string streamid;
};

typedef std::vector<StreamParams> StreamParamsVec;

// A collection of audio and video and data streams. Most of the
// methods are merely for convenience. Many of these methods are keyed
// by ssrc, which is the source identifier in the RTP spec
// (http://tools.ietf.org/html/rfc3550).
// TODO(pthatcher):  Add basic unit test for these.
// See https://code.google.com/p/webrtc/issues/detail?id=4107
struct MediaStreams {
 public:
  MediaStreams() {}
  void CopyFrom(const MediaStreams& sources);

  bool empty() const {
    return audio_.empty() && video_.empty() && data_.empty();
  }

  std::vector<StreamParams>* mutable_audio() { return &audio_; }
  std::vector<StreamParams>* mutable_video() { return &video_; }
  std::vector<StreamParams>* mutable_data() { return &data_; }
  const std::vector<StreamParams>& audio() const { return audio_; }
  const std::vector<StreamParams>& video() const { return video_; }
  const std::vector<StreamParams>& data() const { return data_; }

  // Gets a stream, returning true if found.
  bool GetAudioStream(
      const StreamSelector& selector, StreamParams* stream);
  bool GetVideoStream(
      const StreamSelector& selector, StreamParams* stream);
  bool GetDataStream(
      const StreamSelector& selector, StreamParams* stream);
  // Adds a stream.
  void AddAudioStream(const StreamParams& stream);
  void AddVideoStream(const StreamParams& stream);
  void AddDataStream(const StreamParams& stream);
  // Removes a stream, returning true if found and removed.
  bool RemoveAudioStream(const StreamSelector& selector);
  bool RemoveVideoStream(const StreamSelector& selector);
  bool RemoveDataStream(const StreamSelector& selector);

 private:
  std::vector<StreamParams> audio_;
  std::vector<StreamParams> video_;
  std::vector<StreamParams> data_;

  RTC_DISALLOW_COPY_AND_ASSIGN(MediaStreams);
};

template <class Condition>
const StreamParams* GetStream(const StreamParamsVec& streams,
                              Condition condition) {
  StreamParamsVec::const_iterator found =
      std::find_if(streams.begin(), streams.end(), condition);
  return found == streams.end() ? nullptr : &(*found);
}

template <class Condition>
StreamParams* GetStream(StreamParamsVec& streams, Condition condition) {
  StreamParamsVec::iterator found =
      std::find_if(streams.begin(), streams.end(), condition);
  return found == streams.end() ? nullptr : &(*found);
}

inline const StreamParams* GetStreamBySsrc(const StreamParamsVec& streams,
                                           uint32_t ssrc) {
  return GetStream(streams,
      [&ssrc](const StreamParams& sp) { return sp.has_ssrc(ssrc); });
}

inline const StreamParams* GetStreamByIds(const StreamParamsVec& streams,
                                          const std::string& groupid,
                                          const std::string& id) {
  return GetStream(streams, [&groupid, &id](const StreamParams& sp) {
    return sp.groupid == groupid && sp.id == id;
  });
}

inline StreamParams* GetStreamByIds(StreamParamsVec& streams,
                                    const std::string& groupid,
                                    const std::string& id) {
  return GetStream(streams,
      [&groupid, &id](const StreamParams& sp) {
        return sp.groupid == groupid && sp.id == id;
      });
}

inline const StreamParams* GetStream(const StreamParamsVec& streams,
                                     const StreamSelector& selector) {
  return GetStream(streams,
      [&selector](const StreamParams& sp) { return selector.Matches(sp); });
}

template <class Condition>
bool RemoveStream(StreamParamsVec* streams, Condition condition) {
  auto iter(std::remove_if(streams->begin(), streams->end(), condition));
  if (iter == streams->end())
    return false;
  streams->erase(iter, streams->end());
  return true;
}

// Removes the stream from streams. Returns true if a stream is
// found and removed.
inline bool RemoveStream(StreamParamsVec* streams,
                  const StreamSelector& selector) {
  return RemoveStream(streams,
      [&selector](const StreamParams& sp) { return selector.Matches(sp); });
}
inline bool RemoveStreamBySsrc(StreamParamsVec* streams, uint32_t ssrc) {
  return RemoveStream(streams,
      [&ssrc](const StreamParams& sp) { return sp.has_ssrc(ssrc); });
}
inline bool RemoveStreamByIds(StreamParamsVec* streams,
                              const std::string& groupid,
                              const std::string& id) {
  return RemoveStream(streams,
      [&groupid, &id](const StreamParams& sp) {
        return sp.groupid == groupid && sp.id == id;
      });
}

// Checks if |sp| defines parameters for a single primary stream. There may
// be an RTX stream or a FlexFEC stream (or both) associated with the primary
// stream. Leaving as non-static so we can test this function.
bool IsOneSsrcStream(const StreamParams& sp);

// Checks if |sp| defines parameters for one Simulcast stream. There may be RTX
// streams associated with the simulcast streams. Leaving as non-static so we
// can test this function.
bool IsSimulcastStream(const StreamParams& sp);

}  // namespace cricket

#endif  // MEDIA_BASE_STREAMPARAMS_H_
