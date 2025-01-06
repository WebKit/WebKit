#ifndef AOM_EXAMPLES_MULTILAYER_METADATA_H_
#define AOM_EXAMPLES_MULTILAYER_METADATA_H_

#include <cstdint>
#include <vector>

namespace libaom_examples {

// std::pair<T, bool> is used to indicate presence of a field,
// like an std::optional (which cannot be used because it's C++17).
// If the boolean is true, then the value is present.

struct ColorProperties {
  bool color_range;  // true for full range values
  uint8_t color_primaries;
  uint8_t transfer_characteristics;
  uint8_t matrix_coefficients;
};

enum AlphaUse {
  ALPHA_STRAIGHT = 0,
  ALPHA_PREMULTIPLIED = 1,
  ALPHA_SEGMENTATION = 2,
  ALPHA_UNSPECIFIED = 3,
};

struct AlphaInformation {
  AlphaUse alpha_use_idc;   // [0, 7]
  uint8_t alpha_bit_depth;  // [8, 15]
  uint8_t alpha_clip_idc;   // [0, 3]
  bool alpha_incr_flag;
  uint16_t alpha_transparent_value;  // [0, 1<<alpha_bit_depth]
  uint16_t alpha_opaque_value;       // [0, 1<<alpha_bit_depth]
  // Relevant for ALPHA_STRAIGHT only.
  std::pair<ColorProperties, bool> alpha_color_description;
  // Relevant for ALPHA_SEGMENTATION only.
  // Must be either empty or have the same size as the number of values between
  // alpha_transparent_value and alpha_opaque_value, inclusively.
  std::vector<uint16_t> label_type_id;
};

// TODO: maryla - parse floats directly and convert to this wire
// representation at write time.
struct DepthRepresentationElement {
  bool sign_flag;
  uint8_t exponent;  // [0, 126]
  uint32_t mantissa;
};

struct DepthInformation {
  std::pair<DepthRepresentationElement, bool> z_near;
  std::pair<DepthRepresentationElement, bool> z_far;
  std::pair<DepthRepresentationElement, bool> d_min;
  std::pair<DepthRepresentationElement, bool> d_max;
  uint8_t depth_representation_type;  // [0, 15]
  uint8_t disparity_ref_view_id;      // [0, 3]
  uint8_t depth_nonlinear_precision;  // [8, 23]
  // [0, 1<<depth_nonlinear_precision]
  std::vector<uint32_t> depth_nonlinear_representation_model;
};

enum MultilayerUseCase {
  MULTILAYER_USE_CASE_UNSPECIFIED = 0,
  MULTILAYER_USE_CASE_ALPHA = 1,
  MULTILAYER_USE_CASE_DEPTH = 2,
  MULTILAYER_USE_CASE_STEREO = 3,
  MULTILAYER_USE_CASE_STEREO_ALPHA_GLOBAL = 4,
  MULTILAYER_USE_CASE_STEREO_DEPTH_GLOBAL = 5,
  MULTILAYER_USE_CASE_STEREO_ALPHA = 6,
  MULTILAYER_USE_CASE_STEREO_DEPTH = 7,
  MULTILAYER_USE_CASE_444 = 8,
  MULTILAYER_USE_CASE_420_444 = 9,
  MULTILAYER_USE_CASE_444_ALPHA = 10,
  MULTILAYER_USE_CASE_444_DEPTH = 11,
};

enum LayerType {
  MULTIALYER_LAYER_TYPE_UNSPECIFIED = 0,
  MULTIALYER_LAYER_TYPE_TEXTURE = 1,
  MULTIALYER_LAYER_TYPE_TEXTURE_1 = 2,
  MULTIALYER_LAYER_TYPE_TEXTURE_2 = 3,
  MULTIALYER_LAYER_TYPE_TEXTURE_3 = 4,
  MULTIALYER_LAYER_TYPE_ALPHA = 5,
  MULTIALYER_LAYER_TYPE_DEPTH = 6,
};

enum MultilayerMetadataScope {
  SCOPE_UNSPECIFIED = 0,
  SCOPE_LOCAL = 1,
  SCOPE_GLOBAL = 2,
  SCOPE_MIXED = 3,
};

enum MultilayerViewType {
  VIEW_UNSPECIFIED = 0,
  VIEW_CENTER = 1,
  VIEW_LEFT = 2,
  VIEW_RIGHT = 3,
};

struct LayerMetadata {
  LayerType layer_type;  // [0, 31]
  bool luma_plane_only_flag;
  MultilayerViewType layer_view_type;            // [0, 7]
  uint8_t group_id;                              // [0, 3]
  uint8_t layer_dependency_idc;                  // [0, 7]
  MultilayerMetadataScope layer_metadata_scope;  // [0, 3]

  std::pair<ColorProperties, bool> layer_color_description;

  // Relevant for MULTIALYER_LAYER_TYPE_ALPHA with SCOPE_GLOBAL or SCOPE_MIXED.
  AlphaInformation global_alpha_info;
  // Relevant for MULTIALYER_LAYER_TYPE_DEPTH with SCOPE_GLOBAL or SCOPE_MIXED.
  DepthInformation global_depth_info;
};

struct MultilayerMetadata {
  MultilayerUseCase use_case;  // [0, 63]
  std::vector<LayerMetadata> layers;
};

// Parses a multilayer metadata file.
// Terminates the process in case of error.
// The metadata is expected to be in a subset of the YAML format supporting
// simple lists and maps with integer values, and comments.
// Does very little validation on the metadata, e.g. does not check that the
// values are in the correct range.
MultilayerMetadata parse_multilayer_file(const char *metadata_path);

// Prints the multilayer metadata to stdout for debugging.
void print_multilayer_metadata(const MultilayerMetadata &multilayer);

}  // namespace libaom_examples

#endif  // AOM_EXAMPLES_MULTILAYER_METADATA_H_
