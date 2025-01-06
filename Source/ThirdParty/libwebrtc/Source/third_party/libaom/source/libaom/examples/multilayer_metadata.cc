#include "examples/multilayer_metadata.h"

#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cstdio>
#include <fstream>
#include <iostream>
#include <limits>
#include <optional>
#include <string>
#include <vector>

#include "aom/aom_integer.h"
#include "examples/multilayer_metadata.h"

extern void usage_exit(void);

namespace libaom_examples {

namespace {

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
  *has_list_prefix = 0;
  while (
      *indent < (int)line.length() &&
      (line[*indent] == ' ' || line[*indent] == '\t' || line[*indent] == '-')) {
    if (line[*indent] == '-') {
      *has_list_prefix = true;
    }
    ++(*indent);
  }
}

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
 * 'value' is set to the integer value of the line, or to 0 if the line doesn't
 * contain a number.
 */
bool parse_line(std::fstream &file, int min_indent, bool is_list, int *indent,
                bool *has_list_prefix, int *line_idx, std::string *field_name,
                int *value) {
  *field_name = "";
  *value = 0;
  std::string line;
  std::fstream::pos_type prev_file_position;
  const int prev_indent = *indent;
  while (prev_file_position = file.tellg(), std::getline(file, line)) {
    cleanup_line(line);
    get_indent(line, indent, has_list_prefix);
    line = line.substr(*indent);  // skip indentation
    // If the line is indented less than 'min_indent', it belongs to the outer
    // object, and parsing should go back to the previous function in the stack.
    if (!line.empty() && *indent < min_indent) {
      // Undo reading the last line.
      if (!file.seekp(prev_file_position, std::ios::beg)) {
        fprintf(stderr, "Failed to seek to previous file position\n");
        exit(EXIT_FAILURE);
      }
      return false;
    }

    ++(*line_idx);
    if (line.empty()) continue;

    if (prev_indent >= 0 && prev_indent != *indent) {
      fprintf(stderr, "Error: Bad indentation at line %d\n", *line_idx);
      exit(EXIT_FAILURE);
    }
    if (*has_list_prefix && !is_list) {
      fprintf(stderr, "Error: Unexpected list item at line %d\n", *line_idx);
      exit(EXIT_FAILURE);
    }

    std::string value_str = line;
    size_t colon_pos = line.find(':');
    if (colon_pos != std::string::npos) {
      *field_name = line.substr(0, colon_pos);
      value_str = line.substr(colon_pos + 1);
    }
    char *endptr;
    *value = (int)strtol(&line[colon_pos + 1], &endptr, 10);
    if (*endptr != '\0') {
      fprintf(stderr, "Error: Failed to parse number from '%s'\n",
              value_str.c_str());
      exit(EXIT_FAILURE);
    }
    return true;
  }
  return false;  // Reached the end of the file.
}

template <typename T>
std::vector<T> parse_integer_list(std::fstream &file, int min_indent,
                                  int *line_idx) {
  bool has_list_prefix;
  int indent = -1;
  std::string field_name;
  int value;
  std::vector<T> result;
  while (parse_line(file, min_indent, /*is_list=*/true, &indent,
                    &has_list_prefix, line_idx, &field_name, &value)) {
    if (!field_name.empty()) {
      fprintf(
          stderr,
          "Error: Unexpected field name '%s' at line %d, expected a number\n",
          field_name.c_str(), *line_idx);
      exit(EXIT_FAILURE);
    } else if (!has_list_prefix) {
      fprintf(stderr, "Error: Missing list prefix '-' at line %d\n", *line_idx);
      exit(EXIT_FAILURE);
    } else if (value > (int)std::numeric_limits<T>::max() ||
               value < (int)std::numeric_limits<T>::min()) {
      fprintf(stderr, "Error: Value %d is out of range at line %d\n", value,
              *line_idx);
      exit(EXIT_FAILURE);
    } else {
      result.push_back(value);
    }
  }
  return result;
}

std::pair<ColorProperties, bool> parse_color_properties(std::fstream &file,
                                                        int min_indent,
                                                        int *line_idx) {
  bool has_list_prefix;
  int indent = -1;
  std::string field_name;
  int value;
  ColorProperties color = {};
  while (parse_line(file, min_indent, /*is_list=*/false, &indent,
                    &has_list_prefix, line_idx, &field_name, &value)) {
    if (field_name == "color_range") {
      color.color_range = value;
    } else if (field_name == "color_primaries") {
      color.color_primaries = value;
    } else if (field_name == "transfer_characteristics") {
      color.transfer_characteristics = value;
    } else if (field_name == "matrix_coefficients") {
      color.matrix_coefficients = value;
    } else {
      fprintf(stderr, "Error: Unknown field '%s' at line %d\n",
              field_name.c_str(), *line_idx);
    }
  }
  return std::make_pair(color, true);
}

AlphaInformation parse_multilayer_layer_alpha(std::fstream &file,
                                              int min_indent, int *line_idx) {
  bool has_list_prefix;
  int indent = -1;
  std::string field_name;
  int value;
  AlphaInformation alpha_info = {};
  while (parse_line(file, min_indent, /*is_list=*/false, &indent,
                    &has_list_prefix, line_idx, &field_name, &value)) {
    if (field_name == "alpha_use_idc") {
      alpha_info.alpha_use_idc = (AlphaUse)value;
    } else if (field_name == "alpha_bit_depth") {
      alpha_info.alpha_bit_depth = value;
    } else if (field_name == "alpha_clip_idc") {
      alpha_info.alpha_clip_idc = value;
    } else if (field_name == "alpha_incr_flag") {
      alpha_info.alpha_incr_flag = value;
    } else if (field_name == "alpha_transparent_value") {
      alpha_info.alpha_transparent_value = value;
    } else if (field_name == "alpha_opaque_value") {
      alpha_info.alpha_opaque_value = value;
    } else if (field_name == "alpha_color_description") {
      alpha_info.alpha_color_description =
          parse_color_properties(file, indent, line_idx);
    } else if (field_name == "label_type_id") {
      alpha_info.label_type_id = parse_integer_list<uint16_t>(
          file, /*min_indent=*/indent + 1, line_idx);
    } else {
      fprintf(stderr, "Error: Unknown field '%s' at line %d\n",
              field_name.c_str(), *line_idx);
      exit(EXIT_FAILURE);
    }
  }
  return alpha_info;
}

std::pair<DepthRepresentationElement, bool> parse_depth_representation_element(
    std::fstream &file, int min_indent, int *line_idx) {
  bool has_list_prefix;
  int indent = -1;
  std::string field_name;
  int value;
  DepthRepresentationElement element = {};
  while (parse_line(file, min_indent, /*is_list=*/false, &indent,
                    &has_list_prefix, line_idx, &field_name, &value)) {
    if (field_name == "sign_flag") {
      element.sign_flag = value;
    } else if (field_name == "exponent") {
      element.exponent = value;
    } else if (field_name == "mantissa") {
      element.mantissa = value;
    } else {
      fprintf(stderr, "Error: Unknown field '%s' at line %d\n",
              field_name.c_str(), *line_idx);
      exit(EXIT_FAILURE);
    }
  }
  return std::make_pair(element, true);
}

DepthInformation parse_multilayer_layer_depth(std::fstream &file,
                                              int min_indent, int *line_idx) {
  bool has_list_prefix;
  int indent = -1;
  std::string field_name;
  int value;
  DepthInformation depth_info = {};
  while (parse_line(file, min_indent, /*is_list=*/false, &indent,
                    &has_list_prefix, line_idx, &field_name, &value)) {
    if (field_name == "z_near") {
      depth_info.z_near =
          parse_depth_representation_element(file, indent, line_idx);
    } else if (field_name == "z_far") {
      depth_info.z_far =
          parse_depth_representation_element(file, indent, line_idx);
    } else if (field_name == "d_min") {
      depth_info.d_min =
          parse_depth_representation_element(file, indent, line_idx);
    } else if (field_name == "d_max") {
      depth_info.d_max =
          parse_depth_representation_element(file, indent, line_idx);
    } else if (field_name == "depth_representation_type") {
      depth_info.depth_representation_type = value;
    } else if (field_name == "disparity_ref_view_id") {
      depth_info.disparity_ref_view_id = value;
    } else if (field_name == "depth_nonlinear_precision") {
      depth_info.depth_nonlinear_precision = value;
    } else if (field_name == "depth_nonlinear_representation_model") {
      depth_info.depth_nonlinear_representation_model =
          parse_integer_list<uint32_t>(file, /*min_indent=*/indent + 1,
                                       line_idx);
    } else {
      fprintf(stderr, "Error: Unknown field '%s' at line %d\n",
              field_name.c_str(), *line_idx);
      exit(EXIT_FAILURE);
    }
  }
  return depth_info;
}

std::vector<LayerMetadata> parse_multilayer_layer_metadata(std::fstream &file,
                                                           int min_indent,
                                                           int *line_idx) {
  bool has_list_prefix;
  int indent = -1;
  std::string field_name;
  int value;
  std::vector<LayerMetadata> layers;
  while (parse_line(file, min_indent, /*is_list=*/true, &indent,
                    &has_list_prefix, line_idx, &field_name, &value)) {
    if (has_list_prefix) {
      if (layers.size() >= kMaxNumSpatialLayers) {
        fprintf(stderr,
                "Error: Too many layers at line %d, the maximum is %d\n",
                *line_idx, kMaxNumSpatialLayers);
        exit(EXIT_FAILURE);
      }
      layers.emplace_back();
    }
    if (layers.empty()) {
      fprintf(stderr, "Error: Missing list prefix '-' at line %d\n", *line_idx);
      exit(EXIT_FAILURE);
    }
    LayerMetadata *layer = &layers.back();
    // Check if string starts with field name.
    if ((field_name == "layer_type")) {
      layer->layer_type = (LayerType)value;
    } else if ((field_name == "luma_plane_only_flag")) {
      layer->luma_plane_only_flag = value;
    } else if ((field_name == "layer_view_type")) {
      layer->layer_view_type = (MultilayerViewType)value;
    } else if ((field_name == "group_id")) {
      layer->group_id = value;
    } else if ((field_name == "layer_dependency_idc")) {
      layer->layer_dependency_idc = value;
    } else if ((field_name == "layer_metadata_scope")) {
      layer->layer_metadata_scope = (MultilayerMetadataScope)value;
    } else if ((field_name == "layer_color_description")) {
      layer->layer_color_description =
          parse_color_properties(file, indent, line_idx);
    } else if ((field_name == "alpha")) {
      layer->global_alpha_info =
          parse_multilayer_layer_alpha(file,
                                       /*min_indent=*/indent + 1, line_idx);
    } else if (field_name == "depth") {
      layer->global_depth_info =
          parse_multilayer_layer_depth(file,
                                       /*min_indent=*/indent + 1, line_idx);
    } else {
      fprintf(stderr, "Error: Unknown field %s at line %d\n",
              field_name.c_str(), *line_idx);
      exit(EXIT_FAILURE);
    }
  }
  return layers;
}

MultilayerMetadata parse_multilayer_metadata(std::fstream &file) {
  int line_idx = 0;
  bool has_list_prefix;
  int indent = -1;
  std::string field_name;
  int value;
  MultilayerMetadata multilayer = {};
  while (parse_line(file, /*min_indent=*/0, /*is_list=*/false, &indent,
                    &has_list_prefix, &line_idx, &field_name, &value)) {
    // Check if string starts with field name.
    if ((field_name == "use_case")) {
      multilayer.use_case = (MultilayerUseCase)value;
    } else if ((field_name == "layers")) {
      multilayer.layers =
          parse_multilayer_layer_metadata(file,
                                          /*min_indent=*/indent + 1, &line_idx);
    } else {
      fprintf(stderr, "Error: Unknown field %s at line %d\n",
              field_name.c_str(), line_idx);
      exit(EXIT_FAILURE);
    }
  }
  return multilayer;
}

std::string format_depth_representation_element(
    const std::pair<DepthRepresentationElement, bool> &element) {
  if (!element.second) {
    return "absent";
  } else {
    return "sign_flag " + std::to_string(element.first.sign_flag) +
           " exponent " + std::to_string(element.first.exponent) +
           " mantissa " + std::to_string(element.first.mantissa);
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

}  // namespace

MultilayerMetadata parse_multilayer_file(const char *metadata_path) {
  std::fstream file(metadata_path);
  if (!file.is_open()) {
    fprintf(stderr, "Error: Failed to open %s\n", metadata_path);
    exit(EXIT_FAILURE);
  }

  const MultilayerMetadata multilayer = parse_multilayer_metadata(file);
  if (multilayer.layers.empty()) {
    fprintf(stderr, "Error: No layers found, there must be at least one\n");
    exit(EXIT_FAILURE);
  }
  return multilayer;
}

void print_multilayer_metadata(const MultilayerMetadata &multilayer) {
  printf("=== Multilayer metadata ===\n");
  printf("use_case: %d\n", multilayer.use_case);
  for (size_t i = 0; i < multilayer.layers.size(); ++i) {
    const LayerMetadata &layer = multilayer.layers[i];
    printf("layer %d\n", (int)i);
    printf("  layer_type: %d\n", layer.layer_type);
    printf("  luma_plane_only_flag: %d\n", layer.luma_plane_only_flag);
    printf("  layer_view_type: %d\n", layer.layer_view_type);
    printf("  group_id: %d\n", layer.group_id);
    printf("  layer_dependency_idc: %d\n", layer.layer_dependency_idc);
    printf("  layer_metadata_scope: %d\n", layer.layer_metadata_scope);
    printf("  layer_color_description: %s\n",
           format_color_properties(layer.layer_color_description).c_str());
    if (layer.layer_type == MULTIALYER_LAYER_TYPE_ALPHA) {
      printf("  alpha:\n");
      printf("    alpha_use_idc: %d\n", layer.global_alpha_info.alpha_use_idc);
      printf("    alpha_bit_depth: %d\n",
             layer.global_alpha_info.alpha_bit_depth);
      printf("    alpha_clip_idc: %d\n",
             layer.global_alpha_info.alpha_clip_idc);
      printf("    alpha_incr_flag: %d\n",
             layer.global_alpha_info.alpha_incr_flag);
      printf("    alpha_transparent_value: %hu\n",
             layer.global_alpha_info.alpha_transparent_value);
      printf("    alpha_opaque_value: %hu\n",
             layer.global_alpha_info.alpha_opaque_value);
      printf("    alpha_color_description: %s\n",
             format_color_properties(
                 layer.global_alpha_info.alpha_color_description)
                 .c_str());
      printf("    label_type_id:");
      for (uint16_t label_type_id : layer.global_alpha_info.label_type_id) {
        printf(" %d", label_type_id);
      }
      printf("\n");
    } else if (layer.layer_type == MULTIALYER_LAYER_TYPE_DEPTH) {
      printf("  depth:\n");
      printf("    z_near_flag %s\n",
             format_depth_representation_element(layer.global_depth_info.z_near)
                 .c_str());
      printf("    z_far_flag %s\n",
             format_depth_representation_element(layer.global_depth_info.z_far)
                 .c_str());
      printf("    d_min_flag %s\n",
             format_depth_representation_element(layer.global_depth_info.d_min)
                 .c_str());
      printf("    d_max_flag %s\n",
             format_depth_representation_element(layer.global_depth_info.d_max)
                 .c_str());
      printf("    depth_representation_type: %d\n",
             layer.global_depth_info.depth_representation_type);
      printf("    disparity_ref_view_id: %d\n",
             layer.global_depth_info.disparity_ref_view_id);
      printf("    depth_nonlinear_precision: %d\n",
             layer.global_depth_info.depth_nonlinear_precision);
      printf("    depth_nonlinear_representation_model:");
      for (uint32_t depth_nonlinear_representation_model :
           layer.global_depth_info.depth_nonlinear_representation_model) {
        printf(" %d", depth_nonlinear_representation_model);
      }
      printf("\n");
    }
  }
  printf("\n");
}

}  // namespace libaom_examples
