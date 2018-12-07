/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_STATS_RTCSTATSREPORT_H_
#define API_STATS_RTCSTATSREPORT_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "api/stats/rtcstats.h"
#include "rtc_base/refcount.h"
#include "rtc_base/refcountedobject.h"
#include "rtc_base/scoped_ref_ptr.h"
#include "rtc_base/system/rtc_export.h"

namespace webrtc {

// A collection of stats.
// This is accessible as a map from |RTCStats::id| to |RTCStats|.
class RTC_EXPORT RTCStatsReport : public rtc::RefCountInterface {
 public:
  typedef std::map<std::string, std::unique_ptr<const RTCStats>> StatsMap;

  class ConstIterator {
   public:
    ConstIterator(const ConstIterator&& other);
    ~ConstIterator();

    ConstIterator& operator++();
    ConstIterator& operator++(int);
    const RTCStats& operator*() const;
    const RTCStats* operator->() const;
    bool operator==(const ConstIterator& other) const;
    bool operator!=(const ConstIterator& other) const;

   private:
    friend class RTCStatsReport;
    ConstIterator(const rtc::scoped_refptr<const RTCStatsReport>& report,
                  StatsMap::const_iterator it);

    // Reference report to make sure it is kept alive.
    rtc::scoped_refptr<const RTCStatsReport> report_;
    StatsMap::const_iterator it_;
  };

  // TODO(hbos): Remove "= 0" once Chromium unittest has been updated to call
  // with a parameter. crbug.com/627816
  static rtc::scoped_refptr<RTCStatsReport> Create(int64_t timestamp_us = 0);

  explicit RTCStatsReport(int64_t timestamp_us);
  RTCStatsReport(const RTCStatsReport& other) = delete;
  rtc::scoped_refptr<RTCStatsReport> Copy() const;

  int64_t timestamp_us() const { return timestamp_us_; }
  void AddStats(std::unique_ptr<const RTCStats> stats);
  const RTCStats* Get(const std::string& id) const;
  size_t size() const { return stats_.size(); }

  // Removes the stats object from the report, returning ownership of it or null
  // if there is no object with |id|.
  std::unique_ptr<const RTCStats> Take(const std::string& id);
  // Takes ownership of all the stats in |victim|, leaving it empty.
  void TakeMembersFrom(rtc::scoped_refptr<RTCStatsReport> victim);

  // Stats iterators. Stats are ordered lexicographically on |RTCStats::id|.
  ConstIterator begin() const;
  ConstIterator end() const;

  // Gets the subset of stats that are of type |T|, where |T| is any class
  // descending from |RTCStats|.
  template <typename T>
  std::vector<const T*> GetStatsOfType() const {
    std::vector<const T*> stats_of_type;
    for (const RTCStats& stats : *this) {
      if (stats.type() == T::kType)
        stats_of_type.push_back(&stats.cast_to<const T>());
    }
    return stats_of_type;
  }

  // Creates a JSON readable string representation of the report,
  // listing all of its stats objects.
  std::string ToJson() const;

  friend class rtc::RefCountedObject<RTCStatsReport>;

 private:
  ~RTCStatsReport() override;

  int64_t timestamp_us_;
  StatsMap stats_;
};

}  // namespace webrtc

#endif  // API_STATS_RTCSTATSREPORT_H_
