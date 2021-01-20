/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCStatisticsReport+Private.h"

#include "helpers/NSString+StdString.h"
#include "rtc_base/checks.h"

namespace webrtc {

/** Converts a single value to a suitable NSNumber, NSString or NSArray containing NSNumbers
    or NSStrings.*/
NSObject *ValueFromStatsMember(const RTCStatsMemberInterface *member) {
  if (member->is_defined()) {
    switch (member->type()) {
      case RTCStatsMemberInterface::kBool:
        return [NSNumber numberWithBool:*member->cast_to<RTCStatsMember<bool>>()];
      case RTCStatsMemberInterface::kInt32:
        return [NSNumber numberWithInt:*member->cast_to<RTCStatsMember<int32_t>>()];
      case RTCStatsMemberInterface::kUint32:
        return [NSNumber numberWithUnsignedInt:*member->cast_to<RTCStatsMember<uint32_t>>()];
      case RTCStatsMemberInterface::kInt64:
        return [NSNumber numberWithLong:*member->cast_to<RTCStatsMember<int64_t>>()];
      case RTCStatsMemberInterface::kUint64:
        return [NSNumber numberWithUnsignedLong:*member->cast_to<RTCStatsMember<uint64_t>>()];
      case RTCStatsMemberInterface::kDouble:
        return [NSNumber numberWithDouble:*member->cast_to<RTCStatsMember<double>>()];
      case RTCStatsMemberInterface::kString:
        return [NSString stringForStdString:*member->cast_to<RTCStatsMember<std::string>>()];
      case RTCStatsMemberInterface::kSequenceBool: {
        std::vector<bool> sequence = *member->cast_to<RTCStatsMember<std::vector<bool>>>();
        NSMutableArray *array = [NSMutableArray arrayWithCapacity:sequence.size()];
        for (const auto &item : sequence) {
          [array addObject:[NSNumber numberWithBool:item]];
        }
        return [array copy];
      }
      case RTCStatsMemberInterface::kSequenceInt32: {
        std::vector<int32_t> sequence = *member->cast_to<RTCStatsMember<std::vector<int32_t>>>();
        NSMutableArray<NSNumber *> *array = [NSMutableArray arrayWithCapacity:sequence.size()];
        for (const auto &item : sequence) {
          [array addObject:[NSNumber numberWithInt:item]];
        }
        return [array copy];
      }
      case RTCStatsMemberInterface::kSequenceUint32: {
        std::vector<uint32_t> sequence = *member->cast_to<RTCStatsMember<std::vector<uint32_t>>>();
        NSMutableArray<NSNumber *> *array = [NSMutableArray arrayWithCapacity:sequence.size()];
        for (const auto &item : sequence) {
          [array addObject:[NSNumber numberWithUnsignedInt:item]];
        }
        return [array copy];
      }
      case RTCStatsMemberInterface::kSequenceInt64: {
        std::vector<int64_t> sequence = *member->cast_to<RTCStatsMember<std::vector<int64_t>>>();
        NSMutableArray<NSNumber *> *array = [NSMutableArray arrayWithCapacity:sequence.size()];
        for (const auto &item : sequence) {
          [array addObject:[NSNumber numberWithLong:item]];
        }
        return [array copy];
      }
      case RTCStatsMemberInterface::kSequenceUint64: {
        std::vector<uint64_t> sequence = *member->cast_to<RTCStatsMember<std::vector<uint64_t>>>();
        NSMutableArray<NSNumber *> *array = [NSMutableArray arrayWithCapacity:sequence.size()];
        for (const auto &item : sequence) {
          [array addObject:[NSNumber numberWithUnsignedLong:item]];
        }
        return [array copy];
      }
      case RTCStatsMemberInterface::kSequenceDouble: {
        std::vector<double> sequence = *member->cast_to<RTCStatsMember<std::vector<double>>>();
        NSMutableArray<NSNumber *> *array = [NSMutableArray arrayWithCapacity:sequence.size()];
        for (const auto &item : sequence) {
          [array addObject:[NSNumber numberWithDouble:item]];
        }
        return [array copy];
      }
      case RTCStatsMemberInterface::kSequenceString: {
        std::vector<std::string> sequence =
            *member->cast_to<RTCStatsMember<std::vector<std::string>>>();
        NSMutableArray<NSString *> *array = [NSMutableArray arrayWithCapacity:sequence.size()];
        for (const auto &item : sequence) {
          [array addObject:[NSString stringForStdString:item]];
        }
        return [array copy];
      }
      default:
        RTC_NOTREACHED();
    }
  }

  return nil;
}
}  // namespace webrtc

@implementation RTCStatistics

@synthesize id = _id;
@synthesize timestamp_us = _timestamp_us;
@synthesize type = _type;
@synthesize values = _values;

- (instancetype)initWithStatistics:(const webrtc::RTCStats &)statistics {
  if (self = [super init]) {
    _id = [NSString stringForStdString:statistics.id()];
    _timestamp_us = statistics.timestamp_us();
    _type = [NSString stringWithCString:statistics.type() encoding:NSUTF8StringEncoding];

    NSMutableDictionary<NSString *, NSObject *> *values = [NSMutableDictionary dictionary];
    for (const webrtc::RTCStatsMemberInterface *member : statistics.Members()) {
      NSObject *value = ValueFromStatsMember(member);
      if (value) {
        NSString *name = [NSString stringWithCString:member->name() encoding:NSUTF8StringEncoding];
        RTC_DCHECK(name.length > 0);
        RTC_DCHECK(!values[name]);
        values[name] = value;
      }
    }
    _values = [values copy];
  }

  return self;
}

- (NSString *)description {
  return [NSString stringWithFormat:@"id = %@, type = %@, timestamp = %.0f, values = %@",
                                    self.id,
                                    self.type,
                                    self.timestamp_us,
                                    self.values];
}

@end

@implementation RTCStatisticsReport

@synthesize timestamp_us = _timestamp_us;
@synthesize statistics = _statistics;

- (NSString *)description {
  return [NSString
      stringWithFormat:@"timestamp = %.0f, statistics = %@", self.timestamp_us, self.statistics];
}

@end

@implementation RTCStatisticsReport (Private)

- (instancetype)initWithReport:(const webrtc::RTCStatsReport &)report {
  if (self = [super init]) {
    _timestamp_us = report.timestamp_us();

    NSMutableDictionary *statisticsById =
        [NSMutableDictionary dictionaryWithCapacity:report.size()];
    for (const auto &stat : report) {
      RTCStatistics *statistics = [[RTCStatistics alloc] initWithStatistics:stat];
      statisticsById[statistics.id] = statistics;
    }
    _statistics = [statisticsById copy];
  }

  return self;
}

@end
