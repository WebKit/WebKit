/*
 * Copyright (c) 2024, Alliance for Open Media. All rights reserved.
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include "examples/multilayer_metadata.h"

#include <assert.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

#include "aom/aom_integer.h"
#include "examples/multilayer_metadata.h"

namespace libaom_examples {

namespace {

#define RETURN_IF_FALSE(A) \
  do {                     \
    if (!(A)) {            \
      return false;        \
    }                      \
  } while (0)

constexpr int kMaxNumSpatialLayers = 4;

// Removes comments and trailing spaces from the line.
void cleanup_line(std::string &line) {
  // Remove everything after the first '#'.
  std::size_t comment_pos = line.find('#');
  if (comment_pos != std::string::npos) {
    line.resize(comment_pos);
  }
  // Remove spaces at the end of the line.
  while (!line.empty() && line.back() == ' ') {
    line.resize(line.length() - 1);
  }
}

// Finds the indentation level of the line, and sets 'has_list_prefix' to true
// if the line has a '-' indicating a new item in a list.
void get_indent(const std::string &line, int *indent, bool *has_list_prefix) {
  *indent = 0;
  *has_list_prefix = false;
  while (
      *indent < static_cast<int>(line.length()) &&
      (line[*indent] == ' ' || line[*indent] == '\t' || line[*indent] == '-')) {
    if (line[*indent] == '-') {
      *has_list_prefix = true;
    }
    ++(*indent);
  }
}

class ParsedValue {
 public:
  enum class Type { kNone, kInteger, kFloatingPoint };

  void SetIntegerValue(int64_t v) {
    type_ = Type::kInteger;
    int_value_ = v;
  }

  void SetFloatingPointValue(double v) {
    type_ = Type::kFloatingPoint;
    double_value_ = v;
  }

  void Clear() { type_ = Type::kNone; }

  bool ValueAsFloatingPoint(int line_idx, double *v) {
    if (type_ == Type::kNone) {
      fprintf(
          stderr,
          "No value found where floating point value was expected at line %d\n",
          line_idx);
      return false;
    }
    *v = (type_ == Type::kFloatingPoint) ? double_value_
                                         : static_cast<double>(int_value_);
    return true;
  }

  template <typename T>
  bool IntegerValueInRange(int64_t min, int64_t max, int line_idx, T *v) {
    switch (type_) {
      case Type::kInteger:
        if (int_value_ < min || int_value_ > max) {
          fprintf(stderr,
                  "Integer value %" PRId64 " out of range [%" PRId64
                  ", %" PRId64 "] at line %d\n",
                  int_value_, min, max, line_idx);
          return false;
        }
        *v = static_cast<T>(int_value_);
        return true;
      case Type::kFloatingPoint:
        fprintf(stderr,
                "Floating point value found where integer was expected at line "
                "%d\n",
                line_idx);
        return false;
      case Type::kNone:
      default:
        fprintf(stderr,
                "No value found where integer was expected at line %d\n",
                line_idx);
        return false;
    }
  }

 private:
  Type type_ = Type::kNone;
  int64_t int_value_ = 0;
  double double_value_ = 0.0f;
};

/*
 * Parses the next line from the file, skipping empty lines.
 * Returns false if the end of the file was reached, or if the line was indented
 * less than 'min_indent', meaning that parsing should go back to the previous
 * function in the stack.
 *
 * 'min_indent' is the minimum indentation expected for the next line.
 * 'is_list' must be true if the line is allowed to contain list items ('-').
 * 'indent' MUST be initialized to -1 before the first call, and is then set to
 * the indentation of the line.
 * 'has_list_prefix' is set to true if the line starts a new list item with '-'.
 * 'line_idx' is set to the index of the last line read.
 * 'field_name' is set to the field name if the line contains a colon, or to an
 * empty string otherwise.
 * 'value' is set to the value on the line if present.
 * In case of syntax error, 'syntax_error' is set to true and the function
 * returns false.
 */
bool parse_line(std::ifstream &file, int min_indent, bool is_list, int *indent,
                bool *has_list_prefix, int *line_idx, std::string *field_name,
                ParsedValue *value, bool *syntax_error) {
  *field_name = "";
  *syntax_error = false;
  value->Clear();
  std::string line;
  std::ifstream::pos_type prev_file_position;
  const int prev_indent = *indent;
  while (prev_file_position = file.tellg(), std::getline(file, line)) {
    cleanup_line(line);
    get_indent(line, indent, has_list_prefix);
    line = line.substr(*indent);  // skip indentation
    // If the line is indented less than 'min_indent', it belongs to the outer
    // object, and parsing should go back to the previous function in the stack.
    if (!line.empty() &&
        (*indent < min_indent || (prev_indent > 0 && *indent < prev_indent))) {
      // Undo reading the last line.
      if (!file.seekg(prev_file_position, std::ios::beg)) {
        fprintf(stderr, "Failed to seek to previous file position\n");
        *syntax_error = true;
        return false;
      }
      return false;
    }

    ++(*line_idx);
    if (line.empty()) continue;

    if (prev_indent >= 0 && prev_indent != *indent) {
      fprintf(stderr, "Error: Bad indentation at line %d\n", *line_idx);
      *syntax_error = true;
      return false;
    }
    if (*has_list_prefix && !is_list) {
      fprintf(stderr, "Error: Unexpected list item at line %d\n", *line_idx);
      *syntax_error = true;
      return false;
    }

    std::string value_str = line;
    size_t colon_pos = line.find(':');
    if (colon_pos != std::string::npos) {
      *field_name = line.substr(0, colon_pos);
      value_str = line.substr(colon_pos + 1);
    }
    if (!value_str.empty()) {
      char *endptr;
      if (line.find('.') != std::string::npos) {
        value->SetFloatingPointValue(strtod(value_str.c_str(), &endptr));
        if (*endptr != '\0') {
          fprintf(stderr,
                  "Error: Failed to parse floating point value from '%s' at "
                  "line %d\n",
                  value_str.c_str(), *line_idx);
          *syntax_error = true;
          return false;
        }
      } else {
        value->SetIntegerValue(strtol(value_str.c_str(), &endptr, 10));
        if (*endptr != '\0') {
          fprintf(stderr,
                  "Error: Failed to parse integer from '%s' at line %d\n",
                  value_str.c_str(), *line_idx);
          *syntax_error = true;
          return false;
        }
      }
    }
    return true;
  }
  return false;  // Reached the end of the file.
}

template <typename T>
bool parse_integer_list(std::ifstream &file, int min_indent, int *line_idx,
                        std::vector<T> *result) {
  bool has_list_prefix;
  int indent = -1;
  std::string field_name;
  ParsedValue value;
  bool syntax_error;
  while (parse_line(file, min_indent, /*is_list=*/true, &indent,
                    &has_list_prefix, line_idx, &field_name, &value,
                    &syntax_error)) {
    if (!field_name.empty()) {
      fprintf(
          stderr,
          "Error: Unexpected field name '%s' at line %d, expected a number\n",
          field_name.c_str(), *line_idx);
      return false;
    } else if (!has_list_prefix) {
      fprintf(stderr, "Error: Missing list prefix '-' at line %d\n", *line_idx);
      return false;
    } else {
      T v;
      RETURN_IF_FALSE(value.IntegerValueInRange(
          static_cast<int64_t>(std::numeric_limits<T>::min()),
          static_cast<int64_t>(std::numeric_limits<T>::max()), *line_idx, &v));
      result->push_back(v);
    }
  }
  if (syntax_error) return false;
  return true;
}

template <typename T>
std::pair<T, bool> value_present(const T &v) {
  return std::make_pair(v, true);
}

bool parse_color_properties(std::ifstream &file, int min_indent, int *line_idx,
                            ColorProperties *color) {
  bool has_list_prefix;
  int indent = -1;
  std::string field_name;
  ParsedValue value;
  bool syntax_error;
  *color = {};
  while (parse_line(file, min_indent, /*is_list=*/false, &indent,
                    &has_list_prefix, line_idx, &field_name, &value,
                    &syntax_error)) {
    if (field_name == "color_range") {
      RETURN_IF_FALSE(value.IntegerValueInRange(/*min=*/0, /*max=*/1, *line_idx,
                                                &color->color_range));
    } else if (field_name == "color_primaries") {
      if (!value.IntegerValueInRange(/*min=*/0, /*max=*/255, *line_idx,
                                     &color->color_primaries)) {
        return false;
      }
    } else if (field_name == "transfer_characteristics") {
      RETURN_IF_FALSE(value.IntegerValueInRange(
          /*min=*/0, /*max=*/255, *line_idx, &color->transfer_characteristics));
    } else if (field_name == "matrix_coefficients") {
      RETURN_IF_FALSE(value.IntegerValueInRange(
          /*min=*/0, /*max=*/255, *line_idx, &color->matrix_coefficients));
    } else {
      fprintf(stderr, "Error: Unknown field '%s' at line %d\n",
              field_name.c_str(), *line_idx);
      return false;
    }
  }
  if (syntax_error) return false;
  return true;
}

bool parse_multilayer_layer_alpha(std::ifstream &file, int min_indent,
                                  int *line_idx, AlphaInformation *alpha_info) {
  bool has_list_prefix;
  int indent = -1;
  std::string field_name;
  ParsedValue value;
  bool syntax_error;
  *alpha_info = {};
  while (parse_line(file, min_indent, /*is_list=*/false, &indent,
                    &has_list_prefix, line_idx, &field_name, &value,
                    &syntax_error)) {
    if (field_name == "alpha_use_idc") {
      RETURN_IF_FALSE(value.IntegerValueInRange(
          /*min=*/0, /*max=*/7, *line_idx, &alpha_info->alpha_use_idc));
    } else if (field_name == "alpha_simple_flag") {
      RETURN_IF_FALSE(value.IntegerValueInRange(
          /*min=*/0, /*max=*/1, *line_idx, &alpha_info->alpha_simple_flag));
    } else if (field_name == "alpha_bit_depth") {
      RETURN_IF_FALSE(value.IntegerValueInRange(
          /*min=*/8, /*max=*/15, *line_idx, &alpha_info->alpha_bit_depth));
    } else if (field_name == "alpha_clip_idc") {
      RETURN_IF_FALSE(value.IntegerValueInRange(/*min=*/0, /*max=*/3, *line_idx,
                                                &alpha_info->alpha_clip_idc));
    } else if (field_name == "alpha_incr_flag") {
      RETURN_IF_FALSE(value.IntegerValueInRange(/*min=*/0, /*max=*/1, *line_idx,
                                                &alpha_info->alpha_incr_flag));
    } else if (field_name == "alpha_transparent_value") {
      // At this point we may not have parsed 'alpha_bit_depth' yet, so the
      // exact range is checked later.
      RETURN_IF_FALSE(value.IntegerValueInRange(
          std::numeric_limits<uint16_t>::min(),
          std::numeric_limits<uint16_t>::max(), *line_idx,
          &alpha_info->alpha_transparent_value));
    } else if (field_name == "alpha_opaque_value") {
      // At this point we may not have parsed 'alpha_bit_depth' yet, so the
      // exact range is checked later.
      RETURN_IF_FALSE(value.IntegerValueInRange(
          std::numeric_limits<uint16_t>::min(),
          std::numeric_limits<uint16_t>::max(), *line_idx,
          &alpha_info->alpha_opaque_value));
    } else if (field_name == "alpha_color_description") {
      ColorProperties color;
      RETURN_IF_FALSE(parse_color_properties(file, indent, line_idx, &color));
      alpha_info->alpha_color_description = value_present(color);
    } else {
      fprintf(stderr, "Error: Unknown field '%s' at line %d\n",
              field_name.c_str(), *line_idx);
      return false;
    }
  }
  if (syntax_error) return false;

  // Validation.
  if (alpha_info->alpha_bit_depth == 0) {
    fprintf(stderr,
            "Error: alpha_bit_depth must be specified (in range [8, 15]) for "
            "alpha info\n");
    return false;
  }
  const int alpha_max = (1 << (alpha_info->alpha_bit_depth + 1)) - 1;
  if (alpha_info->alpha_transparent_value > alpha_max) {
    fprintf(stderr, "Error: alpha_transparent_value %d out of range [0, %d]\n",
            alpha_info->alpha_transparent_value, alpha_max);
    return false;
  }
  if (alpha_info->alpha_opaque_value > alpha_max) {
    fprintf(stderr, "Error: alpha_opaque_value %d out of range [0, %d]\n",
            alpha_info->alpha_opaque_value, alpha_max);
    return false;
  }
  if (alpha_info->alpha_color_description.second &&
      (alpha_info->alpha_use_idc != ALPHA_STRAIGHT)) {
    fprintf(stderr,
            "Error: alpha_color_description can only be set if alpha_use_idc "
            "is %d\n",
            ALPHA_STRAIGHT);
    return false;
  }
  return true;
}

bool parse_multilayer_layer_depth(std::ifstream &file, int min_indent,
                                  int *line_idx, DepthInformation *depth_info) {
  bool has_list_prefix;
  int indent = -1;
  std::string field_name;
  ParsedValue value;
  bool syntax_error;
  *depth_info = {};
  while (parse_line(file, min_indent, /*is_list=*/false, &indent,
                    &has_list_prefix, line_idx, &field_name, &value,
                    &syntax_error)) {
    if (field_name == "z_near") {
      double tmp;
      RETURN_IF_FALSE(value.ValueAsFloatingPoint(*line_idx, &tmp));
      DepthRepresentationElement el;
      RETURN_IF_FALSE(double_to_depth_representation_element(tmp, &el));
      depth_info->z_near = value_present(el);
    } else if (field_name == "z_far") {
      double tmp;
      RETURN_IF_FALSE(value.ValueAsFloatingPoint(*line_idx, &tmp));
      DepthRepresentationElement el;
      RETURN_IF_FALSE(double_to_depth_representation_element(tmp, &el));
      depth_info->z_far = value_present(el);
    } else if (field_name == "d_min") {
      double tmp;
      RETURN_IF_FALSE(value.ValueAsFloatingPoint(*line_idx, &tmp));
      DepthRepresentationElement el;
      RETURN_IF_FALSE(double_to_depth_representation_element(tmp, &el));
      depth_info->d_min = value_present(el);
    } else if (field_name == "d_max") {
      double tmp;
      RETURN_IF_FALSE(value.ValueAsFloatingPoint(*line_idx, &tmp));
      DepthRepresentationElement el;
      RETURN_IF_FALSE(double_to_depth_representation_element(tmp, &el));
      depth_info->d_max = value_present(el);
    } else if (field_name == "depth_representation_type") {
      RETURN_IF_FALSE(
          value.IntegerValueInRange(/*min=*/0, /*max=*/15, *line_idx,
                                    &depth_info->depth_representation_type));
    } else if (field_name == "disparity_ref_view_id") {
      RETURN_IF_FALSE(value.IntegerValueInRange(
          /*min=*/0, /*max=*/3, *line_idx, &depth_info->disparity_ref_view_id));
    } else {
      fprintf(stderr, "Error: Unknown field '%s' at line %d\n",
              field_name.c_str(), *line_idx);
      return false;
    }
  }
  if (syntax_error) return false;

  return true;
}

bool parse_multilayer_layer_local_metadata(
    std::ifstream &file, int min_indent, int *line_idx,
    std::vector<FrameLocalMetadata> &frames) {
  bool has_list_prefix;
  int indent = -1;
  std::string field_name;
  ParsedValue value;
  bool syntax_error;
  while (parse_line(file, min_indent, /*is_list=*/true, &indent,
                    &has_list_prefix, line_idx, &field_name, &value,
                    &syntax_error)) {
    if (has_list_prefix) {
      frames.push_back({});
    }
    if (frames.empty()) {
      fprintf(stderr, "Error: Missing list prefix '-' at line %d\n", *line_idx);
      return false;
    }

    if (field_name == "frame_idx") {
      RETURN_IF_FALSE(
          value.IntegerValueInRange(0, std::numeric_limits<long>::max(),
                                    *line_idx, &frames.back().frame_idx));
    } else if (field_name == "alpha") {
      RETURN_IF_FALSE(parse_multilayer_layer_alpha(
          file,
          /*min_indent=*/indent + 1, line_idx, &frames.back().alpha));

    } else if (field_name == "depth") {
      RETURN_IF_FALSE(parse_multilayer_layer_depth(
          file,
          /*min_indent=*/indent + 1, line_idx, &frames.back().depth));

    } else {
      fprintf(stderr, "Error: Unknown field %s at line %d\n",
              field_name.c_str(), *line_idx);
      return false;
    }
  }
  if (syntax_error) return false;
  return true;
}

bool validate_layer(const LayerMetadata &layer, bool layer_has_alpha,
                    bool layer_has_depth) {
  if (layer_has_alpha != (layer.layer_type == MULTILAYER_LAYER_TYPE_ALPHA &&
                          layer.layer_metadata_scope >= SCOPE_GLOBAL)) {
    fprintf(stderr,
            "Error: alpha info must be set if and only if layer_type is "
            "%d and layer_metadata_scope is >= %d\n",
            MULTILAYER_LAYER_TYPE_ALPHA, SCOPE_GLOBAL);
    return false;
  }
  if (layer_has_depth != (layer.layer_type == MULTILAYER_LAYER_TYPE_DEPTH &&
                          layer.layer_metadata_scope >= SCOPE_GLOBAL)) {
    fprintf(stderr,
            "Error: depth info must be set if and only if layer_type is "
            "%d and layer_metadata_scope is >= %d\n",
            MULTILAYER_LAYER_TYPE_DEPTH, SCOPE_GLOBAL);
    return false;
  }
  return true;
}

bool parse_multilayer_layer_metadata(std::ifstream &file, int min_indent,
                                     int *line_idx,
                                     std::vector<LayerMetadata> &layers) {
  bool has_list_prefix;
  int indent = -1;
  std::string field_name;
  ParsedValue value;
  bool syntax_error;
  bool layer_has_alpha = false;
  bool layer_has_depth = false;
  while (parse_line(file, min_indent, /*is_list=*/true, &indent,
                    &has_list_prefix, line_idx, &field_name, &value,
                    &syntax_error)) {
    if (has_list_prefix) {
      // Start of a new layer.
      if (layers.size() >= kMaxNumSpatialLayers) {
        fprintf(stderr,
                "Error: Too many layers at line %d, the maximum is %d\n",
                *line_idx, kMaxNumSpatialLayers);
        return false;
      }

      // Validate the previous layer.
      if (!layers.empty()) {
        RETURN_IF_FALSE(
            validate_layer(layers.back(), layer_has_alpha, layer_has_depth));
      }
      if (layers.size() == 1 && layers.back().layer_color_description.second) {
        fprintf(stderr,
                "Error: layer_color_description cannot be specified for the "
                "first layer\n");
        return false;
      }

      layers.push_back({});
      layer_has_alpha = false;
      layer_has_depth = false;
    }
    if (layers.empty()) {
      fprintf(stderr, "Error: Missing list prefix '-' at line %d\n", *line_idx);
      return false;
    }

    LayerMetadata *layer = &layers.back();
    // Check if string starts with field name.
    if ((field_name == "layer_type")) {
      RETURN_IF_FALSE(value.IntegerValueInRange(
          /*min=*/0, /*max=*/31, *line_idx, &layer->layer_type));
    } else if ((field_name == "luma_plane_only_flag")) {
      RETURN_IF_FALSE(value.IntegerValueInRange(/*min=*/0, /*max=*/1, *line_idx,
                                                &layer->luma_plane_only_flag));
    } else if ((field_name == "layer_view_type")) {
      RETURN_IF_FALSE(value.IntegerValueInRange(
          /*min=*/0, /*max=*/7, *line_idx, &layer->layer_view_type));
    } else if ((field_name == "group_id")) {
      RETURN_IF_FALSE(value.IntegerValueInRange(/*min=*/0, /*max=*/3, *line_idx,
                                                &layer->group_id));
    } else if ((field_name == "layer_dependency_idc")) {
      RETURN_IF_FALSE(value.IntegerValueInRange(/*min=*/0, /*max=*/7, *line_idx,
                                                &layer->layer_dependency_idc));
    } else if ((field_name == "layer_metadata_scope")) {
      RETURN_IF_FALSE(value.IntegerValueInRange(
          /*min=*/0, /*max=*/3, *line_idx, &layer->layer_metadata_scope));
    } else if ((field_name == "layer_color_description")) {
      ColorProperties color_properties;
      RETURN_IF_FALSE(
          parse_color_properties(file, indent, line_idx, &color_properties));
      layer->layer_color_description = value_present(color_properties);
    } else if ((field_name == "alpha")) {
      layer_has_alpha = true;
      RETURN_IF_FALSE(parse_multilayer_layer_alpha(file,
                                                   /*min_indent=*/indent + 1,
                                                   line_idx, &layer->alpha));
    } else if (field_name == "depth") {
      layer_has_depth = true;
      RETURN_IF_FALSE(parse_multilayer_layer_depth(file,
                                                   /*min_indent=*/indent + 1,
                                                   line_idx, &layer->depth));
      if ((layer->depth.d_min.second || layer->depth.d_max.second) &&
          layer->depth.disparity_ref_view_id == (layers.size() - 1)) {
        fprintf(stderr,
                "disparity_ref_view_id must be different from the layer's id "
                "for layer %d (zero-based index)\n",
                static_cast<int>(layers.size()) - 1);
        return false;
      }
    } else if (field_name == "local_metadata") {
      RETURN_IF_FALSE(parse_multilayer_layer_local_metadata(
          file, /*min_indent=*/indent + 1, line_idx, layer->local_metadata));
    } else {
      fprintf(stderr, "Error: Unknown field %s at line %d\n",
              field_name.c_str(), *line_idx);
      return false;
    }
  }
  if (syntax_error) return false;
  RETURN_IF_FALSE(
      validate_layer(layers.back(), layer_has_alpha, layer_has_depth));
  return true;
}

bool parse_multilayer_metadata(std::ifstream &file,
                               MultilayerMetadata *multilayer) {
  int line_idx = 0;
  bool has_list_prefix;
  int indent = -1;
  std::string field_name;
  ParsedValue value;
  bool syntax_error;
  *multilayer = {};
  while (parse_line(file, /*min_indent=*/0, /*is_list=*/false, &indent,
                    &has_list_prefix, &line_idx, &field_name, &value,
                    &syntax_error)) {
    // Check if string starts with field name.
    if ((field_name == "use_case")) {
      RETURN_IF_FALSE(value.IntegerValueInRange(
          /*min=*/0, /*max=*/63, line_idx, &multilayer->use_case));
    } else if ((field_name == "layers")) {
      RETURN_IF_FALSE(parse_multilayer_layer_metadata(
          file,
          /*min_indent=*/indent + 1, &line_idx, multilayer->layers));
    } else {
      fprintf(stderr, "Error: Unknown field %s at line %d\n",
              field_name.c_str(), line_idx);
      return false;
    }
  }
  if (syntax_error) return false;
  return true;
}

std::string format_depth_representation_element(
    const std::pair<DepthRepresentationElement, bool> &element) {
  if (!element.second) {
    return "absent";
  } else {
    return std::to_string(
               depth_representation_element_to_double(element.first)) +
           " (sign " + std::to_string(element.first.sign_flag) + " exponent " +
           std::to_string(element.first.exponent) + " mantissa " +
           std::to_string(element.first.mantissa) + " mantissa_len " +
           std::to_string(element.first.mantissa_len) + ")";
  }
}

std::string format_color_properties(
    const std::pair<ColorProperties, bool> &color_properties) {
  if (!color_properties.second) {
    return "absent";
  } else {
    return std::to_string(color_properties.first.color_primaries) + "/" +
           std::to_string(color_properties.first.transfer_characteristics) +
           "/" + std::to_string(color_properties.first.matrix_coefficients) +
           (color_properties.first.color_range ? "F" : "L");
  }
}

bool validate_multilayer_metadata(const MultilayerMetadata &multilayer) {
  if (multilayer.layers.empty()) {
    fprintf(stderr, "Error: No layers found, there must be at least one\n");
    return false;
  }
  if (multilayer.layers.size() > 4) {
    fprintf(stderr, "Error: Too many layers, found %d, max 4\n",
            static_cast<int>(multilayer.layers.size()));
    return false;
  }

  bool same_view_type = true;
  MultilayerViewType view_type = multilayer.layers[0].layer_view_type;
  for (const LayerMetadata &layer : multilayer.layers) {
    if (layer.layer_view_type != view_type) {
      same_view_type = false;
      break;
    }
  }

  for (int i = 0; i < static_cast<int>(multilayer.layers.size()); ++i) {
    const LayerMetadata &layer = multilayer.layers[i];
    switch (multilayer.use_case) {
      case MULTILAYER_USE_CASE_GLOBAL_ALPHA:
      case MULTILAYER_USE_CASE_GLOBAL_DEPTH:
      case MULTILAYER_USE_CASE_STEREO:
      case MULTILAYER_USE_CASE_STEREO_GLOBAL_ALPHA:
      case MULTILAYER_USE_CASE_STEREO_GLOBAL_DEPTH:
      case MULTILAYER_USE_CASE_444_GLOBAL_ALPHA:
      case MULTILAYER_USE_CASE_444_GLOBAL_DEPTH:
        if (layer.layer_metadata_scope != SCOPE_GLOBAL) {
          fprintf(
              stderr,
              "Error: for use_case %d, all layers must have scope %d, found %d "
              "instead for layer %d (zero-based index)\n",
              multilayer.use_case, SCOPE_GLOBAL, layer.layer_metadata_scope, i);
          return false;
        }
        break;
      default: break;
    }
    switch (multilayer.use_case) {
      case MULTILAYER_USE_CASE_GLOBAL_ALPHA:
      case MULTILAYER_USE_CASE_GLOBAL_DEPTH:
      case MULTILAYER_USE_CASE_ALPHA:
      case MULTILAYER_USE_CASE_DEPTH:
      case MULTILAYER_USE_CASE_444_GLOBAL_ALPHA:
      case MULTILAYER_USE_CASE_444_GLOBAL_DEPTH:
      case MULTILAYER_USE_CASE_444:
      case MULTILAYER_USE_CASE_420_444:
        if (!same_view_type) {
          fprintf(stderr,
                  "Error: for use_case %d, all layers must have the same view "
                  "type, found different view_type for layer %d (zero-based "
                  "index)\n",
                  multilayer.use_case, i);
          return false;
        }
      default: break;
    }
    if (layer.layer_type != MULTILAYER_LAYER_TYPE_UNSPECIFIED)
      switch (multilayer.use_case) {
        case MULTILAYER_USE_CASE_GLOBAL_ALPHA:
        case MULTILAYER_USE_CASE_ALPHA:
        case MULTILAYER_USE_CASE_STEREO_GLOBAL_ALPHA:
        case MULTILAYER_USE_CASE_STEREO_ALPHA:
          if (layer.layer_type != MULTILAYER_LAYER_TYPE_TEXTURE &&
              layer.layer_type != MULTILAYER_LAYER_TYPE_ALPHA) {
            fprintf(stderr,
                    "Error: for use_case %d, all layers must be of type %d or "
                    "%d, found %d for layer %d (zero-based index)\n",
                    multilayer.use_case, MULTILAYER_LAYER_TYPE_TEXTURE,
                    MULTILAYER_LAYER_TYPE_ALPHA, layer.layer_type, i);
            return false;
          }
          break;
        case MULTILAYER_USE_CASE_GLOBAL_DEPTH:
        case MULTILAYER_USE_CASE_DEPTH:
        case MULTILAYER_USE_CASE_STEREO_GLOBAL_DEPTH:
        case MULTILAYER_USE_CASE_STEREO_DEPTH:
          if (layer.layer_type != MULTILAYER_LAYER_TYPE_TEXTURE &&
              layer.layer_type != MULTILAYER_LAYER_TYPE_DEPTH) {
            fprintf(stderr,
                    "Error: for use_case %d, all layers must be of type %d or "
                    "%d, found %d for layer %d (zero-based index)\n",
                    multilayer.use_case, MULTILAYER_LAYER_TYPE_TEXTURE,
                    MULTILAYER_LAYER_TYPE_DEPTH, layer.layer_type, i);
            return false;
          }
          break;
        case MULTILAYER_USE_CASE_STEREO:
          if (layer.layer_type != MULTILAYER_LAYER_TYPE_TEXTURE) {
            fprintf(stderr,
                    "Error: for use_case %d, all layers must be of type %d, "
                    "found %d for layer %d (zero-based index)\n",
                    multilayer.use_case, MULTILAYER_LAYER_TYPE_TEXTURE,
                    layer.layer_type, i);
            return false;
          }
          break;
        case MULTILAYER_USE_CASE_444_GLOBAL_ALPHA:
          if (layer.layer_type != MULTILAYER_LAYER_TYPE_TEXTURE_1 &&
              layer.layer_type != MULTILAYER_LAYER_TYPE_TEXTURE_2 &&
              layer.layer_type != MULTILAYER_LAYER_TYPE_TEXTURE_3 &&
              layer.layer_type != MULTILAYER_LAYER_TYPE_ALPHA) {
            fprintf(stderr,
                    "Error: for use_case %d, all layers must be of type %d, "
                    "%d, %d, or %d, found %d for layer %d (zero-based index)\n",
                    multilayer.use_case, MULTILAYER_LAYER_TYPE_TEXTURE_1,
                    MULTILAYER_LAYER_TYPE_TEXTURE_2,
                    MULTILAYER_LAYER_TYPE_TEXTURE_3,
                    MULTILAYER_LAYER_TYPE_ALPHA, layer.layer_type, i);
            return false;
          }
          break;
        case MULTILAYER_USE_CASE_444_GLOBAL_DEPTH:
          if (layer.layer_type != MULTILAYER_LAYER_TYPE_TEXTURE_1 &&
              layer.layer_type != MULTILAYER_LAYER_TYPE_TEXTURE_2 &&
              layer.layer_type != MULTILAYER_LAYER_TYPE_TEXTURE_3 &&
              layer.layer_type != MULTILAYER_LAYER_TYPE_DEPTH) {
            fprintf(stderr,
                    "Error: for use_case %d, all layers must be of type %d, "
                    "%d, %d, or %d, found %d for layer %d (zero-based index)\n",
                    multilayer.use_case, MULTILAYER_LAYER_TYPE_TEXTURE_1,
                    MULTILAYER_LAYER_TYPE_TEXTURE_2,
                    MULTILAYER_LAYER_TYPE_TEXTURE_3,
                    MULTILAYER_LAYER_TYPE_DEPTH, layer.layer_type, i);
            return false;
          }
          break;
        case MULTILAYER_USE_CASE_444:
          if (layer.layer_type != MULTILAYER_LAYER_TYPE_TEXTURE_1 &&
              layer.layer_type != MULTILAYER_LAYER_TYPE_TEXTURE_2 &&
              layer.layer_type != MULTILAYER_LAYER_TYPE_TEXTURE_3) {
            fprintf(
                stderr,
                "Error: for use_case %d, all layers must be of type %d, %d, or "
                "%d, found %d for layer %d (zero-based index)\n",
                multilayer.use_case, MULTILAYER_LAYER_TYPE_TEXTURE_1,
                MULTILAYER_LAYER_TYPE_TEXTURE_2,
                MULTILAYER_LAYER_TYPE_TEXTURE_3, layer.layer_type, i);
            return false;
          }
          break;
        case MULTILAYER_USE_CASE_420_444:
          if (layer.layer_type != MULTILAYER_LAYER_TYPE_TEXTURE &&
              layer.layer_type != MULTILAYER_LAYER_TYPE_TEXTURE_1 &&
              layer.layer_type != MULTILAYER_LAYER_TYPE_TEXTURE_2 &&
              layer.layer_type != MULTILAYER_LAYER_TYPE_TEXTURE_3) {
            fprintf(stderr,
                    "Error: for use_case %d, all layers must be of type %d, "
                    "%d, %d, or %d, found %d for layer %d (zero-based index)\n",
                    multilayer.use_case, MULTILAYER_LAYER_TYPE_TEXTURE,
                    MULTILAYER_LAYER_TYPE_TEXTURE_1,
                    MULTILAYER_LAYER_TYPE_TEXTURE_2,
                    MULTILAYER_LAYER_TYPE_TEXTURE_3, layer.layer_type, i);
            return false;
          }
          break;
        default: break;
      }
    if (layer.layer_dependency_idc >= (1 << i)) {
      fprintf(stderr,
              "Error: layer_dependency_idc of layer %d (zero-based index) must "
              "be in [0, %d], found %d for layer %d (zero-based index)\n",
              i, (1 << i) - 1, layer.layer_dependency_idc, i);
      return false;
    }
    if ((layer.layer_type == MULTILAYER_LAYER_TYPE_ALPHA ||
         layer.layer_type == MULTILAYER_LAYER_TYPE_DEPTH) &&
        layer.layer_color_description.second) {
      fprintf(stderr,
              "Error: alpha or depth layers cannot have "
              "layer_color_description for layer %d (zero-based index)\n",
              i);
      return false;
    }
  }
  return true;
}

void print_alpha_information(const AlphaInformation &alpha) {
  printf("    alpha_simple_flag: %d\n", alpha.alpha_simple_flag);
  if (!alpha.alpha_simple_flag) {
    printf("    alpha_bit_depth: %d\n", alpha.alpha_bit_depth);
    printf("    alpha_clip_idc: %d\n", alpha.alpha_clip_idc);
    printf("    alpha_incr_flag: %d\n", alpha.alpha_incr_flag);
    printf("    alpha_transparent_value: %hu\n", alpha.alpha_transparent_value);
    printf("    alpha_opaque_value: %hu\n", alpha.alpha_opaque_value);
    printf("    alpha_color_description: %s\n",
           format_color_properties(alpha.alpha_color_description).c_str());
  }
}

void print_depth_information(const DepthInformation &depth) {
  printf("    z_near: %s\n",
         format_depth_representation_element(depth.z_near).c_str());
  printf("    z_far: %s\n",
         format_depth_representation_element(depth.z_far).c_str());
  printf("    d_min: %s\n",
         format_depth_representation_element(depth.d_min).c_str());
  printf("    d_max: %s\n",
         format_depth_representation_element(depth.d_max).c_str());
  printf("    depth_representation_type: %d\n",
         depth.depth_representation_type);
  printf("    disparity_ref_view_id: %d\n", depth.disparity_ref_view_id);
}

}  // namespace

double depth_representation_element_to_double(
    const DepthRepresentationElement &e) {
  // Let x be a variable that is computed using four variables s, e, m, and n,
  // as follows: If e is greater than 0 and less than 127, x is set equal to
  // (−1)^s*2^(e−31) * (1+m÷2^n).
  // Otherwise (e is equal to 0), x is set equal to (−1)^s*2^−(30+n)*m.
  if (e.exponent > 0) {
    return (e.sign_flag ? -1 : 1) * std::pow(2.0, e.exponent - 31) *
           (1 + static_cast<double>(e.mantissa) /
                    (static_cast<int64_t>(1) << e.mantissa_len));
  } else {
    return (e.sign_flag ? -1 : 1) * e.mantissa *
           std::pow(2.0, -30 + e.mantissa_len);
  }
}

bool double_to_depth_representation_element(
    double v, DepthRepresentationElement *element) {
  const double orig = v;
  if (v == 0.0) {
    *element = { 0, 0, 0, 1 };
    return true;
  }
  const bool sign = v < 0.0;
  if (sign) {
    v = -v;
  }
  int exp = 0;
  if (v >= 1.0) {
    while (v >= 2.0) {
      ++exp;
      v /= 2;
    }
  } else {
    while (v < 1.0) {
      ++exp;
      v *= 2.0;
    }
    exp = -exp;
  }
  if ((exp + 31) <= 0 || (exp + 31) > 126) {
    fprintf(stderr,
            "Error: Floating point value %f out of range (too large or too "
            "small)\n",
            orig);
    return false;
  }
  assert(v >= 1.0 && v < 2.0);
  v -= 1.0;
  uint32_t mantissa = 0;
  uint8_t mantissa_len = 0;
  constexpr uint8_t kMaxMantissaLen = 32;
  do {
    const int bit = (v >= 0.5);
    mantissa = (mantissa << 1) + bit;
    v -= bit * 0.5;
    ++mantissa_len;
    v *= 2.0;
  } while (mantissa_len < kMaxMantissaLen && v > 0.0);
  *element = { sign, static_cast<uint8_t>(exp + 31), mantissa_len, mantissa };
  return true;
}

bool parse_multilayer_file(const char *metadata_path,
                           MultilayerMetadata *multilayer) {
  std::ifstream file(metadata_path);
  if (!file.is_open()) {
    fprintf(stderr, "Error: Failed to open %s\n", metadata_path);
    return false;
  }

  if (!parse_multilayer_metadata(file, multilayer) ||
      !validate_multilayer_metadata(*multilayer)) {
    return false;
  }
  return multilayer;
}

void print_multilayer_metadata(const MultilayerMetadata &multilayer) {
  printf("=== Multilayer metadata ===\n");
  printf("use_case: %d\n", multilayer.use_case);
  for (size_t i = 0; i < multilayer.layers.size(); ++i) {
    const LayerMetadata &layer = multilayer.layers[i];
    printf("layer %zu\n", i);
    printf("  layer_type: %d\n", layer.layer_type);
    printf("  luma_plane_only_flag: %d\n", layer.luma_plane_only_flag);
    printf("  layer_view_type: %d\n", layer.layer_view_type);
    printf("  group_id: %d\n", layer.group_id);
    printf("  layer_dependency_idc: %d\n", layer.layer_dependency_idc);
    printf("  layer_metadata_scope: %d\n", layer.layer_metadata_scope);
    printf("  layer_color_description: %s\n",
           format_color_properties(layer.layer_color_description).c_str());
    if (layer.layer_type == MULTILAYER_LAYER_TYPE_ALPHA) {
      if (layer.layer_metadata_scope >= SCOPE_GLOBAL) {
        printf("  global alpha:\n");
        print_alpha_information(layer.alpha);
      }
      for (const FrameLocalMetadata &local_metadata : layer.local_metadata) {
        printf("  local alpha for frame %ld:\n", local_metadata.frame_idx);
        print_alpha_information(local_metadata.alpha);
      }
    } else if (layer.layer_type == MULTILAYER_LAYER_TYPE_DEPTH) {
      printf("  global depth:\n");
      print_depth_information(layer.depth);
      for (const FrameLocalMetadata &local_metadata : layer.local_metadata) {
        printf("  local depth for frame %ld:\n", local_metadata.frame_idx);
        print_depth_information(local_metadata.depth);
      }
    }
  }
  printf("\n");
}

}  // namespace libaom_examples
