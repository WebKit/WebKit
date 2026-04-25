##
## Copyright (c) 2017, Alliance for Open Media. All rights reserved.
##
## This source code is subject to the terms of the BSD 2 Clause License and
## the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
## was not distributed with this source code in the LICENSE file, you can
## obtain it at www.aomedia.org/license/software. If the Alliance for Open
## Media Patent License 1.0 was not distributed with this source code in the
## PATENTS file, you can obtain it at www.aomedia.org/license/patent.
##
sub aom_scale_forward_decls() {
print <<EOF
#include <stdbool.h>

struct yv12_buffer_config;
EOF
}
forward_decls qw/aom_scale_forward_decls/;

add_proto qw/int aom_yv12_realloc_with_new_border/, "struct yv12_buffer_config *ybf, int new_border, int byte_alignment, bool alloc_pyramid, int num_planes";

add_proto qw/void aom_yv12_extend_frame_borders/, "struct yv12_buffer_config *ybf, const int num_planes";

add_proto qw/void aom_yv12_copy_frame/, "const struct yv12_buffer_config *src_bc, struct yv12_buffer_config *dst_bc, const int num_planes";

add_proto qw/void aom_yv12_copy_y/, "const struct yv12_buffer_config *src_ybc, struct yv12_buffer_config *dst_ybc, int use_crop";

add_proto qw/void aom_yv12_copy_u/, "const struct yv12_buffer_config *src_bc, struct yv12_buffer_config *dst_bc, int use_crop";

add_proto qw/void aom_yv12_copy_v/, "const struct yv12_buffer_config *src_bc, struct yv12_buffer_config *dst_bc, int use_crop";

add_proto qw/void aom_yv12_partial_copy_y/, "const struct yv12_buffer_config *src_ybc, int hstart1, int hend1, int vstart1, int vend1, struct yv12_buffer_config *dst_ybc, int hstart2, int vstart2";
add_proto qw/void aom_yv12_partial_coloc_copy_y/, "const struct yv12_buffer_config *src_ybc, struct yv12_buffer_config *dst_ybc, int hstart, int hend, int vstart, int vend";
add_proto qw/void aom_yv12_partial_copy_u/, "const struct yv12_buffer_config *src_bc, int hstart1, int hend1, int vstart1, int vend1, struct yv12_buffer_config *dst_bc, int hstart2, int vstart2";
add_proto qw/void aom_yv12_partial_coloc_copy_u/, "const struct yv12_buffer_config *src_bc, struct yv12_buffer_config *dst_bc, int hstart, int hend, int vstart, int vend";
add_proto qw/void aom_yv12_partial_copy_v/, "const struct yv12_buffer_config *src_bc, int hstart1, int hend1, int vstart1, int vend1, struct yv12_buffer_config *dst_bc, int hstart2, int vstart2";
add_proto qw/void aom_yv12_partial_coloc_copy_v/, "const struct yv12_buffer_config *src_bc, struct yv12_buffer_config *dst_bc, int hstart, int hend, int vstart, int vend";

add_proto qw/void aom_extend_frame_borders_plane_row/, "const struct yv12_buffer_config *ybf, int plane, int v_start, int v_end";

add_proto qw/void aom_extend_frame_borders/, "struct yv12_buffer_config *ybf, int num_planes";
1;
