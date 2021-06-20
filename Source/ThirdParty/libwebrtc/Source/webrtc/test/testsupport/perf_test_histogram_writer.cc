/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/testsupport/perf_test_histogram_writer.h"

#include <stdlib.h>

#include <map>
#include <memory>

#include "absl/strings/string_view.h"
#include "api/numerics/samples_stats_counter.h"
#include "rtc_base/logging.h"
#include "rtc_base/strings/string_builder.h"
#include "rtc_base/synchronization/mutex.h"
#include "third_party/catapult/tracing/tracing/value/diagnostics/reserved_infos.h"
#include "third_party/catapult/tracing/tracing/value/histogram.h"

namespace webrtc {
namespace test {

namespace {

namespace proto = catapult::tracing::tracing::proto;

std::string AsJsonString(const std::string string) {
  return "\"" + string + "\"";
}

class PerfTestHistogramWriter : public PerfTestResultWriter {
 public:
  PerfTestHistogramWriter() : mutex_() {}
  void ClearResults() override {
    MutexLock lock(&mutex_);
    histograms_.clear();
  }

  void LogResult(absl::string_view graph_name,
                 absl::string_view trace_name,
                 const double value,
                 absl::string_view units,
                 const bool important,
                 ImproveDirection improve_direction) override {
    (void)important;
    AddSample(graph_name, trace_name, value, units, improve_direction);
  }
  void LogResultMeanAndError(absl::string_view graph_name,
                             absl::string_view trace_name,
                             const double mean,
                             const double error,
                             absl::string_view units,
                             const bool important,
                             ImproveDirection improve_direction) override {
    RTC_LOG(LS_WARNING) << "Discarding stddev, not supported by histograms";
    (void)error;
    (void)important;

    AddSample(graph_name, trace_name, mean, units, improve_direction);
  }
  void LogResultList(absl::string_view graph_name,
                     absl::string_view trace_name,
                     const rtc::ArrayView<const double> values,
                     absl::string_view units,
                     const bool important,
                     ImproveDirection improve_direction) override {
    (void)important;
    for (double value : values) {
      AddSample(graph_name, trace_name, value, units, improve_direction);
    }
  }
  std::string Serialize() const override {
    proto::HistogramSet histogram_set;

    MutexLock lock(&mutex_);
    for (const auto& histogram : histograms_) {
      std::unique_ptr<proto::Histogram> proto = histogram.second->toProto();
      histogram_set.mutable_histograms()->AddAllocated(proto.release());
    }

    std::string output;
    bool ok = histogram_set.SerializeToString(&output);
    RTC_DCHECK(ok) << "Failed to serialize histogram set to string";
    return output;
  }

 private:
  void AddSample(absl::string_view original_graph_name,
                 absl::string_view trace_name,
                 const double value,
                 absl::string_view units,
                 ImproveDirection improve_direction) {
    // WebRTC annotates the units into the metric name when they are not
    // supported by the Histogram API.
    std::string graph_name(original_graph_name);
    if (units == "dB") {
      graph_name += "_dB";
    } else if (units == "fps") {
      graph_name += "_fps";
    } else if (units == "%") {
      graph_name += "_%";
    }

    // Lookup on graph name + trace name (or measurement + story in catapult
    // parlance). There should be several histograms with the same measurement
    // if they're for different stories.
    rtc::StringBuilder measurement_and_story;
    measurement_and_story << graph_name << trace_name;
    MutexLock lock(&mutex_);
    if (histograms_.count(measurement_and_story.str()) == 0) {
      proto::UnitAndDirection unit = ParseUnit(units, improve_direction);
      std::unique_ptr<catapult::HistogramBuilder> builder =
          std::make_unique<catapult::HistogramBuilder>(graph_name, unit);

      // Set all summary options as false - we don't want to generate
      // metric_std, metric_count, and so on for all metrics.
      builder->SetSummaryOptions(proto::SummaryOptions());
      histograms_[measurement_and_story.str()] = std::move(builder);

      proto::Diagnostic stories;
      proto::GenericSet* generic_set = stories.mutable_generic_set();
      generic_set->add_values(AsJsonString(std::string(trace_name)));
      histograms_[measurement_and_story.str()]->AddDiagnostic(
          catapult::kStoriesDiagnostic, stories);
    }

    if (units == "bps") {
      // Bps has been interpreted as bits per second in WebRTC tests.
      histograms_[measurement_and_story.str()]->AddSample(value / 8);
    } else {
      histograms_[measurement_and_story.str()]->AddSample(value);
    }
  }

  proto::UnitAndDirection ParseUnit(absl::string_view units,
                                    ImproveDirection improve_direction) {
    RTC_DCHECK(units.find('_') == std::string::npos)
        << "The unit_bigger|smallerIsBetter syntax isn't supported in WebRTC, "
           "use the enum instead.";

    proto::UnitAndDirection result;
    result.set_improvement_direction(ParseDirection(improve_direction));
    if (units == "bps") {
      result.set_unit(proto::BYTES_PER_SECOND);
    } else if (units == "dB") {
      result.set_unit(proto::UNITLESS);
    } else if (units == "fps") {
      result.set_unit(proto::HERTZ);
    } else if (units == "frames") {
      result.set_unit(proto::COUNT);
    } else if (units == "ms") {
      result.set_unit(proto::MS_BEST_FIT_FORMAT);
    } else if (units == "%") {
      result.set_unit(proto::UNITLESS);
    } else {
      proto::Unit unit = catapult::UnitFromJsonUnit(std::string(units));

      // UnitFromJsonUnit returns UNITLESS if it doesn't recognize the unit.
      if (unit == proto::UNITLESS && units != "unitless") {
        RTC_LOG(LS_WARNING) << "Unit " << units << " is unsupported.";
      }

      result.set_unit(unit);
    }
    return result;
  }

  proto::ImprovementDirection ParseDirection(
      ImproveDirection improve_direction) {
    switch (improve_direction) {
      case ImproveDirection::kNone:
        return proto::NOT_SPECIFIED;
      case ImproveDirection::kSmallerIsBetter:
        return proto::SMALLER_IS_BETTER;
      case ImproveDirection::kBiggerIsBetter:
        return proto::BIGGER_IS_BETTER;
      default:
        RTC_NOTREACHED() << "Invalid enum value " << improve_direction;
    }
  }

 private:
  mutable Mutex mutex_;
  std::map<std::string, std::unique_ptr<catapult::HistogramBuilder>> histograms_
      RTC_GUARDED_BY(&mutex_);
};

}  // namespace

PerfTestResultWriter* CreateHistogramWriter() {
  return new PerfTestHistogramWriter();
}

}  // namespace test
}  // namespace webrtc
