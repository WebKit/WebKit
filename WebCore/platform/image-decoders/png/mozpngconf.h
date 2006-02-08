/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla libpng configuration.
 *
 * The Initial Developer of the Original Code is
 * Tim Rowley.
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s): Tim Rowley <tor@cs.brown.edu>, Apple Computer
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef MOZPNGCONF_H
#define MOZPNGCONF_H

#define PNG_NO_GLOBAL_ARRAYS

#define PNG_NO_INFO_IMAGE
#define PNG_NO_READ_BACKGROUND
#define PNG_NO_READ_DITHER
#define PNG_NO_READ_INVERT
#define PNG_NO_READ_SHIFT
#define PNG_NO_READ_PACK
#define PNG_NO_READ_PACKSWAP
#define PNG_NO_READ_FILLER
#define PNG_NO_READ_SWAP_ALPHA
#define PNG_NO_READ_INVERT_ALPHA
#define PNG_NO_READ_RGB_TO_GRAY
#define PNG_NO_READ_USER_TRANSFORM
#define PNG_NO_READ_bKGD
#define PNG_NO_READ_cHRM
#define PNG_NO_READ_hIST
#define PNG_NO_READ_iCCP
#define PNG_NO_READ_pCAL
#define PNG_NO_READ_pHYs
#define PNG_NO_READ_sBIT
#define PNG_NO_READ_sCAL
#define PNG_NO_READ_sPLT
#define PNG_NO_READ_TEXT
#define PNG_NO_READ_tIME
#define PNG_NO_READ_UNKNOWN_CHUNKS
#define PNG_NO_READ_USER_CHUNKS
#define PNG_NO_USER_MEM
#define PNG_NO_READ_EMPTY_PLTE
#define PNG_NO_FIXED_POINT_SUPPORTED
#define PNG_NO_READ_OPT_PLTE
#define PNG_NO_MNG_FEATURES

#ifdef webkit_png_WRITE
#define PNG_NO_WRITE_BACKGROUND
#define PNG_NO_WRITE_DITHER
#define PNG_NO_WRITE_INVERT
#define PNG_NO_WRITE_SHIFT
#define PNG_NO_WRITE_PACK
#define PNG_NO_WRITE_PACKSWAP
#define PNG_NO_WRITE_FILLER
#define PNG_NO_WRITE_SWAP_ALPHA
#define PNG_NO_WRITE_INVERT_ALPHA
#define PNG_NO_WRITE_RGB_TO_GRAY
#define PNG_NO_WRITE_USER_TRANSFORM
#define PNG_NO_WRITE_bKGD
#define PNG_NO_WRITE_cHRM
#define PNG_NO_WRITE_hIST
#define PNG_NO_WRITE_iCCP
#define PNG_NO_WRITE_pCAL
#define PNG_NO_WRITE_pHYs
#define PNG_NO_WRITE_sBIT
#define PNG_NO_WRITE_sCAL
#define PNG_NO_WRITE_sPLT
#define PNG_NO_WRITE_TEXT
#define PNG_NO_WRITE_tIME
#define PNG_NO_WRITE_UNKNOWN_CHUNKS
#define PNG_NO_WRITE_USER_CHUNKS
#define PNG_NO_WRITE_EMPTY_PLTE
#define PNG_NO_WRITE_OPT_PLTE
#else
#define PNG_NO_WRITE_SUPPORTED
#endif

#define PNG_NO_READ_STRIP_ALPHA
#define PNG_NO_USER_TRANSFORM_PTR
#define PNG_NO_READ_oFFs
#define PNG_NO_HANDLE_AS_UNKNOWN
#define PNG_NO_CONSOLE_IO
#define PNG_NO_ZALLOC_ZERO
#define PNG_NO_ERROR_NUMBERS
#define PNG_NO_EASY_ACCESS

#define PNG_NO_SEQUENTIAL_READ_SUPPORTED

/* Mangle names of exported libpng functions so different libpng versions
   can coexist. It is recommended that if you do this, you give your
   library a different name such as "mozlibpng" instead of "libpng". */

/* The following has been present since libpng-0.88, has never changed, and
   is unaffected by conditional compilation macros.  It will not be mangled
   and it is the only choice for use in configure scripts for detecting the
   presence of any libpng version since 0.88.

   png_get_io_ptr
*/

/* Like Mozilla, we mangle it anyway. */
#define png_get_io_ptr                  webkit_png_get_io_ptr

/* The following weren't present in libpng-0.88 but have never changed since
   they were first introduced and are not affected by any conditional compile
   choices and therefore don't need to be mangled.  We'll mangle them anyway. */
#define png_sig_cmp                     webkit_png_sig_cmp               /* 0.90 */
#define png_memcpy_check                webkit_png_memcpy_ck             /* 1.0.0 */
#define png_memset_check                webkit_png_memset_ck             /* 1.0.0 */
#define png_access_version_number       webkit_png_access_vn             /* 1.0.7 */

/* These have never changed since they were first introduced but they
   make direct reference to members of png_ptr that might have been moved,
   so they will be mangled. */

#define png_set_sig_bytes               webkit_png_set_sig_b             /* 0.90 */
#define png_reset_zstream               webkit_png_reset_zs              /* 1.0.7 */

/* The following may have changed, or can be affected by conditional compilation
   choices, and will be mangled. */
#define png_build_gamma_table           webkit_png_build_gamma_tab
#define png_build_grayscale_palette     webkit_png_build_g_p
#define png_calculate_crc               webkit_png_calc_crc
#define png_check_chunk_name            webkit_png_ck_chunk_name
#define png_check_sig                   webkit_png_ck_sig
#define png_chunk_error                 webkit_png_chunk_err
#define png_chunk_warning               webkit_png_chunk_warn
#define png_combine_row                 webkit_png_combine_row
#define png_convert_from_struct_tm      webkit_png_cv_from_struct_tm
#define png_convert_from_time_t         webkit_png_cv_from_time_t
#define png_convert_to_rfc1123          webkit_png_cv_to_rfc1123
#define png_crc_error                   webkit_png_crc_error
#define png_crc_finish                  webkit_png_crc_finish
#define png_crc_read                    webkit_png_crc_read
#define png_create_info_struct          webkit_png_cr_info_str
#define png_create_read_struct          webkit_png_cr_read_str
#define png_create_read_struct_2        webkit_png_cr_read_str_2
#define png_create_struct               webkit_png_create_st
#define png_create_struct_2             webkit_png_create_s2
#define png_create_write_struct         webkit_png_cr_write_str
#define png_create_write_struct_2       webkit_png_cr_write_str_2
#define png_data_freer                  webkit_png_data_freer
#define png_decompress_chunk            webkit_png_decomp_chunk
#define png_default_error               webkit_png_def_error
#define png_default_flush               webkit_png_def_flush
#define png_default_read                webkit_png_def_read
#define png_default_read_data           webkit_png_def_read_data
#define png_default_warning             webkit_png_def_warning
#define png_default_write               webkit_png_def_write
#define png_destroy_info_struct         webkit_png_dest_info_str
#define png_destroy_read_struct         webkit_png_dest_read_str
#define png_destroy_struct              webkit_png_dest_str
#define png_destroy_struct_2            webkit_png_dest_str_2
#define png_destroy_write_struct        webkit_png_dest_write_str
#define png_digit                       webkit_png_digit
#define png_do_background               webkit_png_do_back
#define png_do_bgr                      webkit_png_do_bgr
#define png_do_chop                     webkit_png_do_chop
#define png_do_dither                   webkit_png_do_dith
#define png_do_expand                   webkit_png_do_expand
#define png_do_expand_palette           webkit_png_do_expand_plte
#define png_do_gamma                    webkit_png_do_gamma
#define png_do_gray_to_rgb              webkit_png_do_g_to_rgb
#define png_do_invert                   webkit_png_do_invert
#define png_do_packswap                 webkit_png_do_packswap
#define png_do_read_filler              webkit_png_do_read_fill
#define png_do_read_interlace           webkit_png_do_read_int
#define png_do_read_intrapixel          webkit_png_do_read_intra
#define png_do_read_invert_alpha        webkit_png_do_read_inv_alph
#define png_do_read_swap_alpha          webkit_png_do_read_swap_alph
#define png_do_read_transformations     webkit_png_do_read_trans
#define png_do_rgb_to_gray              webkit_png_do_rgb_to_g
#define png_do_strip_filler             webkit_png_do_strip_fill
#define png_do_swap                     webkit_png_do_swap
#define png_do_unpack                   webkit_png_do_unpack
#define png_do_unshift                  webkit_png_do_unshift
#define png_error                       webkit_png_error
#define png_format_buffer               webkit_png_format_buf
#define png_free                        webkit_png_free
#define png_free_data                   webkit_png_free_data
#define png_free_default                webkit_png_free_def
#define png_gamma_shift                 webkit_png_gamma_shift
#define png_get_IHDR                    webkit_png_get_IHDR
#define png_get_PLTE                    webkit_png_get_PLTE
#define png_get_asm_flagmask            webkit_png_get_asm_mask
#define png_get_asm_flags               webkit_png_get_asm_flags
#define png_get_bKGD                    webkit_png_get_bKGD
#define png_get_bit_depth               webkit_png_get_bit_depth
#define png_get_cHRM                    webkit_png_get_cHRM
#define png_get_cHRM_fixed              webkit_png_get_cHRM_fixed
#define png_get_channels                webkit_png_get_channels
#define png_get_color_type              webkit_png_get_color_type
#define png_get_compression_buffer_size webkit_png_get_comp_buf_siz
#define png_get_compression_type        webkit_png_get_comp_type
#define png_get_copyright               webkit_png_get_copyright
#define png_get_error_ptr               webkit_png_get_error_ptr
#define png_get_filter_type             webkit_png_get_filter_type
#define png_get_gAMA                    webkit_png_get_gAMA
#define png_get_gAMA_fixed              webkit_png_get_gAMA_fixed
#define png_get_hIST                    webkit_png_get_hIST
#define png_get_header_ver              webkit_png_get_hdr_ver
#define png_get_header_version          webkit_png_get_hdr_vn
#define png_get_iCCP                    webkit_png_get_iCCP
#define png_get_image_height            webkit_png_get_image_h
#define png_get_image_width             webkit_png_get_image_w
#define png_get_int_32                  webkit_png_get_int_32
#define png_get_interlace_type          webkit_png_get_interlace_type
#define png_get_libpng_ver              webkit_png_get_libpng_ver
#define png_get_mem_ptr                 webkit_png_get_mem_ptr
#define png_get_mmx_bitdepth_threshold  webkit_png_get_mmx_bitdepth_thr
#define png_get_mmx_flagmask            webkit_png_get_mmx_flagmask
#define png_get_mmx_rowbytes_threshold  webkit_png_get_mmx_rowbytes_thr
#define png_get_oFFs                    webkit_png_get_oFFs
#define png_get_pCAL                    webkit_png_get_pCAL
#define png_get_pHYs                    webkit_png_get_pHYs
#define png_get_pixel_aspect_ratio      webkit_png_get_pixel_aspect_ratio
#define png_get_pixels_per_meter        webkit_png_get_pixels_p_m
#define png_get_progressive_ptr         webkit_png_get_progressive_ptr
#define png_get_rgb_to_gray_status      webkit_png_get_rgb_to_gray_status
#define png_get_rowbytes                webkit_png_get_rowbytes
#define png_get_rows                    webkit_png_get_rows
#define png_get_sBIT                    webkit_png_get_sBIT
#define png_get_sCAL                    webkit_png_get_sCAL
#define png_get_sCAL_s                  webkit_png_get_sCAL_s
#define png_get_sPLT                    webkit_png_get_sPLT
#define png_get_sRGB                    webkit_png_get_sRGB
#define png_get_signature               webkit_png_get_signature
#define png_get_tIME                    webkit_png_get_tIME
#define png_get_tRNS                    webkit_png_get_tRNS
#define png_get_text                    webkit_png_get_text
#define png_get_uint_16                 webkit_png_get_uint_16
#define png_get_uint_32                 webkit_png_get_uint_32
#define png_get_unknown_chunks          webkit_png_get_unk_chunks
#define png_get_user_chunk_ptr          webkit_png_get_user_chunk_ptr
#define png_get_user_transform_ptr      webkit_png_get_user_transform_ptr
#define png_get_valid                   webkit_png_get_valid
#define png_get_x_offset_microns        webkit_png_get_x_offs_microns
#define png_get_x_offset_pixels         webkit_png_get_x_offs_pixels
#define png_get_x_pixels_per_meter      webkit_png_get_x_pix_per_meter
#define png_get_y_offset_microns        webkit_png_get_y_offs_microns
#define png_get_y_offset_pixels         webkit_png_get_y_offs_pixels
#define png_get_y_pixels_per_meter      webkit_png_get_y_pix_per_meter
#define png_handle_IEND                 webkit_png_handle_IEND
#define png_handle_IHDR                 webkit_png_handle_IHDR
#define png_handle_PLTE                 webkit_png_handle_PLTE
#define png_handle_as_unknown           webkit_png_handle_as_unknown
#define png_handle_bKGD                 webkit_png_handle_bKGD
#define png_handle_cHRM                 webkit_png_handle_cHRM
#define png_handle_gAMA                 webkit_png_handle_gAMA
#define png_handle_hIST                 webkit_png_handle_hIST
#define png_handle_iCCP                 webkit_png_handle_iCCP
#define png_handle_oFFs                 webkit_png_handle_oFFs
#define png_handle_pCAL                 webkit_png_handle_pCAL
#define png_handle_pHYs                 webkit_png_handle_pHYs
#define png_handle_sBIT                 webkit_png_handle_sBIT
#define png_handle_sCAL                 webkit_png_handle_sCAL
#define png_handle_sPLT                 webkit_png_handle_sPLT
#define png_handle_sRGB                 webkit_png_handle_sRGB
#define png_handle_tEXt                 webkit_png_handle_tEXt
#define png_handle_tIME                 webkit_png_handle_tIME
#define png_handle_tRNS                 webkit_png_handle_tRNS
#define png_handle_unknown              webkit_png_handle_unknown
#define png_handle_zTXt                 webkit_png_handle_zTXt
#define png_info_destroy                webkit_png_info_dest
#define png_info_init_3                 webkit_png_info_init_3
#define png_init_io                     webkit_png_init_io
#define png_init_mmx_flags              webkit_png_init_mmx_flags
#define png_init_read_transformations   webkit_png_init_read_transf
#define png_malloc                      webkit_png_malloc
#define png_malloc_default              webkit_png_malloc_default
#define png_malloc_warn                 webkit_png_malloc_warn
#define png_mmx_support                 webkit_png_mmx_support
#define png_permit_empty_plte           webkit_png_permit_mng_empty_plte
#define png_permit_mng_features         webkit_png_permit_mng_features
#define png_process_IDAT_data           webkit_png_proc_IDAT_data
#define png_process_data                webkit_png_process_data
#define png_process_some_data           webkit_png_proc_some_data
#define png_progressive_combine_row     webkit_png_progressive_combine_row
#define png_push_crc_finish             webkit_png_push_crc_finish
#define png_push_crc_skip               webkit_png_push_crc_skip
#define png_push_fill_buffer            webkit_png_push_fill_buffer
#define png_push_handle_tEXt            webkit_png_push_handle_tEXt
#define png_push_handle_unknown         webkit_png_push_handle_unk
#define png_push_handle_zTXt            webkit_png_push_handle_ztXt
#define png_push_have_end               webkit_png_push_have_end
#define png_push_have_info              webkit_png_push_have_info
#define png_push_have_row               webkit_png_push_have_row
#define png_push_process_row            webkit_png_push_proc_row
#define png_push_read_IDAT              webkit_png_push_read_IDAT
#define png_push_read_chunk             webkit_png_push_read_chunk
#define png_push_read_sig               webkit_png_push_read_sig
#define png_push_read_tEXt              webkit_png_push_read_tEXt
#define png_push_read_zTXt              webkit_png_push_read_zTXt
#define png_push_restore_buffer         webkit_png_push_rest_buf
#define png_push_save_buffer            webkit_png_push_save_buf
#define png_read_data                   webkit_png_read_data
#define png_read_destroy                webkit_png_read_dest
#define png_read_end                    webkit_png_read_end
#define png_read_filter_row             webkit_png_read_filt_row
#define png_read_finish_row             webkit_png_read_finish_row
#define png_read_image                  webkit_png_read_image
#define png_read_info                   webkit_png_read_info
#define png_read_init_2                 webkit_png_read_init_2
#define png_read_init_3                 webkit_png_read_init_3
#define png_read_png                    webkit_png_read_png
#define png_read_push_finish_row        webkit_png_read_push_finish_row
#define png_read_row                    webkit_png_read_row
#define png_read_rows                   webkit_png_read_rows
#define png_read_start_row              webkit_png_read_start_row
#define png_read_transform_info         webkit_png_read_transform_info
#define png_read_update_info            webkit_png_read_update_info
#define png_reset_crc                   webkit_png_reset_crc
#define png_set_IHDR                    webkit_png_set_IHDR
#define png_set_PLTE                    webkit_png_set_PLTE
#define png_set_asm_flags               webkit_png_set_asm_flags
#define png_set_bKGD                    webkit_png_set_bKGD
#define png_set_background              webkit_png_set_background
#define png_set_bgr                     webkit_png_set_bgr
#define png_set_cHRM                    webkit_png_set_cHRM
#define png_set_cHRM_fixed              webkit_png_set_cHRM_fixed
#define png_set_compression_buffer_size webkit_png_set_comp_buf_siz
#define png_set_compression_level       webkit_png_set_comp_level
#define png_set_compression_mem_level   webkit_png_set_comp_mem_lev
#define png_set_compression_method      webkit_png_set_comp_method
#define png_set_compression_strategy    webkit_png_set_comp_strategy
#define png_set_compression_window_bits webkit_png_set_comp_win_bits
#define png_set_crc_action              webkit_png_set_crc_action
#define png_set_dither                  webkit_png_set_dither
#define png_set_error_fn                webkit_png_set_error_fn
#define png_set_expand                  webkit_png_set_expand
#define png_set_filler                  webkit_png_set_filler
#define png_set_filter                  webkit_png_set_filter
#define png_set_filter_heuristics       webkit_png_set_filter_heur
#define png_set_flush                   webkit_png_set_flush
#define png_set_gAMA                    webkit_png_set_gAMA
#define png_set_gAMA_fixed              webkit_png_set_gAMA_fixed
#define png_set_gamma                   webkit_png_set_gamma
#define png_set_gray_1_2_4_to_8         webkit_png_set_gray_1_2_4_to_8
#define png_set_gray_to_rgb             webkit_png_set_gray_to_rgb
#define png_set_hIST                    webkit_png_set_hIST
#define png_set_iCCP                    webkit_png_set_iCCP
#define png_set_interlace_handling      webkit_png_set_interlace_handling
#define png_set_invalid                 webkit_png_set_invalid
#define png_set_invert_alpha            webkit_png_set_invert_alpha
#define png_set_invert_mono             webkit_png_set_invert_mono
#define png_set_keep_unknown_chunks     webkit_png_set_keep_unknown_chunks
#define png_set_mem_fn                  webkit_png_set_mem_fn
#define png_set_mmx_thresholds          webkit_png_set_mmx_thr
#define png_set_oFFs                    webkit_png_set_oFFs
#define png_set_pCAL                    webkit_png_set_pCAL
#define png_set_pHYs                    webkit_png_set_pHYs
#define png_set_packing                 webkit_png_set_packing
#define png_set_packswap                webkit_png_set_packswap
#define png_set_palette_to_rgb          webkit_png_set_palette_to_rgb
#define png_set_progressive_read_fn     webkit_png_set_progressive_read_fn
#define png_set_read_fn                 webkit_png_set_read_fn
#define png_set_read_status_fn          webkit_png_set_read_status_fn
#define png_set_read_user_chunk_fn      webkit_png_set_read_user_chunk_fn
#define png_set_read_user_transform_fn  webkit_png_set_read_user_trans_fn
#define png_set_rgb_to_gray             webkit_png_set_rgb_to_gray
#define png_set_rgb_to_gray_fixed       webkit_png_set_rgb_to_gray_fixed
#define png_set_rows                    webkit_png_set_rows
#define png_set_sBIT                    webkit_png_set_sBIT
#define png_set_sCAL                    webkit_png_set_sCAL
#define png_set_sCAL_s                  webkit_png_set_sCAL_s
#define png_set_sPLT                    webkit_png_set_sPLT
#define png_set_sRGB                    webkit_png_set_sRGB
#define png_set_sRGB_gAMA_and_cHRM      webkit_png_set_sRGB_gAMA_and_cHRM
#define png_set_shift                   webkit_png_set_shift
#define png_set_strip_16                webkit_png_set_strip_16
#define png_set_strip_alpha             webkit_png_set_strip_alpha
#define png_set_strip_error_numbers     webkit_png_set_strip_err_nums
#define png_set_swap                    webkit_png_set_swap
#define png_set_swap_alpha              webkit_png_set_swap_alpha
#define png_set_tIME                    webkit_png_set_tIME
#define png_set_tRNS                    webkit_png_set_tRNS
#define png_set_tRNS_to_alpha           webkit_png_set_tRNS_to_alpha
#define png_set_text                    webkit_png_set_text
#define png_set_text_2                  webkit_png_set_text-2
#define png_set_unknown_chunk_location  webkit_png_set_unknown_chunk_loc
#define png_set_unknown_chunks          webkit_png_set_unknown_chunks
#define png_set_user_transform_info     webkit_png_set_user_transform_info
#define png_set_write_fn                webkit_png_set_write_fn
#define png_set_write_status_fn         webkit_png_set_write_status_fn
#define png_set_write_user_transform_fn webkit_png_set_write_user_trans_fn
#define png_sig_bytes                   webkit_png_sig_bytes
#define png_start_read_image            webkit_png_start_read_image
#define png_warning                     webkit_png_warning
#define png_write_chunk                 webkit_png_write_chunk
#define png_write_chunk_data            webkit_png_write_chunk_data
#define png_write_chunk_end             webkit_png_write_chunk_end
#define png_write_chunk_start           webkit_png_write_chunk_start
#define png_write_end                   webkit_png_write_end
#define png_write_flush                 webkit_png_write_flush
#define png_write_image                 webkit_png_write_image
#define png_write_info                  webkit_png_write_info
#define png_write_info_before_PLTE      webkit_png_write_info_before_PLTE
#define png_write_init                  webkit_png_write_init
#define png_write_init_2                webkit_png_write_init_2
#define png_write_init_3                webkit_png_write_init_3
#define png_write_png                   webkit_png_write_png
#define png_write_row                   webkit_png_write_row
#define png_write_rows                  webkit_png_write_rows
#define png_zalloc                      webkit_png_zalloc
#define png_zfree                       webkit_png_zfree

/* libpng-1.2.6 additions */
#define png_convert_size                webkit_png_convert_size
#define png_get_uint_31                 webkit_png_get_uint_31
#define png_get_user_height_max         webkit_png_get_user_height_max
#define png_get_user_width_max          webkit_png_get_user_width_max
#define png_set_user_limits             webkit_png_set_user_limits

/* libpng-1.2.7 addition */
#define png_set_add_alpha               webkit_png_set_add_alpha

#endif
