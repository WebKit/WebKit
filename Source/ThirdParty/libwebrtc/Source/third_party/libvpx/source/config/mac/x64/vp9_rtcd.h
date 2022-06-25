// This file is generated. Do not edit.
#ifndef VP9_RTCD_H_
#define VP9_RTCD_H_

#ifdef RTCD_C
#define RTCD_EXTERN
#else
#define RTCD_EXTERN extern
#endif

/*
 * VP9
 */

#include "vp9/common/vp9_common.h"
#include "vp9/common/vp9_enums.h"
#include "vp9/common/vp9_filter.h"
#include "vpx/vpx_integer.h"

struct macroblockd;

/* Encoder forward decls */
struct macroblock;
struct vp9_variance_vtable;
struct search_site_config;
struct mv;
union int_mv;
struct yv12_buffer_config;

#ifdef __cplusplus
extern "C" {
#endif

int64_t vp9_block_error_c(const tran_low_t* coeff,
                          const tran_low_t* dqcoeff,
                          intptr_t block_size,
                          int64_t* ssz);
int64_t vp9_block_error_sse2(const tran_low_t* coeff,
                             const tran_low_t* dqcoeff,
                             intptr_t block_size,
                             int64_t* ssz);
int64_t vp9_block_error_avx2(const tran_low_t* coeff,
                             const tran_low_t* dqcoeff,
                             intptr_t block_size,
                             int64_t* ssz);
RTCD_EXTERN int64_t (*vp9_block_error)(const tran_low_t* coeff,
                                       const tran_low_t* dqcoeff,
                                       intptr_t block_size,
                                       int64_t* ssz);

int64_t vp9_block_error_fp_c(const tran_low_t* coeff,
                             const tran_low_t* dqcoeff,
                             int block_size);
int64_t vp9_block_error_fp_sse2(const tran_low_t* coeff,
                                const tran_low_t* dqcoeff,
                                int block_size);
int64_t vp9_block_error_fp_avx2(const tran_low_t* coeff,
                                const tran_low_t* dqcoeff,
                                int block_size);
RTCD_EXTERN int64_t (*vp9_block_error_fp)(const tran_low_t* coeff,
                                          const tran_low_t* dqcoeff,
                                          int block_size);

int vp9_denoiser_filter_c(const uint8_t* sig,
                          int sig_stride,
                          const uint8_t* mc_avg,
                          int mc_avg_stride,
                          uint8_t* avg,
                          int avg_stride,
                          int increase_denoising,
                          BLOCK_SIZE bs,
                          int motion_magnitude);
int vp9_denoiser_filter_sse2(const uint8_t* sig,
                             int sig_stride,
                             const uint8_t* mc_avg,
                             int mc_avg_stride,
                             uint8_t* avg,
                             int avg_stride,
                             int increase_denoising,
                             BLOCK_SIZE bs,
                             int motion_magnitude);
#ifdef WEBRTC_WEBKIT_DISABLE_HARDWARE_ACCELERATION
#define vp9_denoiser_filter vp9_denoiser_filter_c
#else
#define vp9_denoiser_filter vp9_denoiser_filter_sse2
#endif

int vp9_diamond_search_sad_c(const struct macroblock* x,
                             const struct search_site_config* cfg,
                             struct mv* ref_mv,
                             struct mv* best_mv,
                             int search_param,
                             int sad_per_bit,
                             int* num00,
                             const struct vp9_variance_vtable* fn_ptr,
                             const struct mv* center_mv);
int vp9_diamond_search_sad_avx(const struct macroblock* x,
                               const struct search_site_config* cfg,
                               struct mv* ref_mv,
                               struct mv* best_mv,
                               int search_param,
                               int sad_per_bit,
                               int* num00,
                               const struct vp9_variance_vtable* fn_ptr,
                               const struct mv* center_mv);
RTCD_EXTERN int (*vp9_diamond_search_sad)(
    const struct macroblock* x,
    const struct search_site_config* cfg,
    struct mv* ref_mv,
    struct mv* best_mv,
    int search_param,
    int sad_per_bit,
    int* num00,
    const struct vp9_variance_vtable* fn_ptr,
    const struct mv* center_mv);

void vp9_fht16x16_c(const int16_t* input,
                    tran_low_t* output,
                    int stride,
                    int tx_type);
void vp9_fht16x16_sse2(const int16_t* input,
                       tran_low_t* output,
                       int stride,
                       int tx_type);
#ifdef WEBRTC_WEBKIT_DISABLE_HARDWARE_ACCELERATION
#define vp9_fht16x16 vp9_fht16x16_c
#else
#define vp9_fht16x16 vp9_fht16x16_sse2
#endif

void vp9_fht4x4_c(const int16_t* input,
                  tran_low_t* output,
                  int stride,
                  int tx_type);
void vp9_fht4x4_sse2(const int16_t* input,
                     tran_low_t* output,
                     int stride,
                     int tx_type);
#ifdef WEBRTC_WEBKIT_DISABLE_HARDWARE_ACCELERATION
#define vp9_fht4x4 vp9_fht4x4_c
#else
#define vp9_fht4x4 vp9_fht4x4_sse2
#endif

void vp9_fht8x8_c(const int16_t* input,
                  tran_low_t* output,
                  int stride,
                  int tx_type);
void vp9_fht8x8_sse2(const int16_t* input,
                     tran_low_t* output,
                     int stride,
                     int tx_type);
#ifdef WEBRTC_WEBKIT_DISABLE_HARDWARE_ACCELERATION
#define vp9_fht8x8 vp9_fht8x8_c
#else
#define vp9_fht8x8 vp9_fht8x8_sse2
#endif

void vp9_filter_by_weight16x16_c(const uint8_t* src,
                                 int src_stride,
                                 uint8_t* dst,
                                 int dst_stride,
                                 int src_weight);
void vp9_filter_by_weight16x16_sse2(const uint8_t* src,
                                    int src_stride,
                                    uint8_t* dst,
                                    int dst_stride,
                                    int src_weight);
#ifdef WEBRTC_WEBKIT_DISABLE_HARDWARE_ACCELERATION
#define vp9_filter_by_weight16x16 vp9_filter_by_weight16x16_c
#else
#define vp9_filter_by_weight16x16 vp9_filter_by_weight16x16_sse2
#endif

void vp9_filter_by_weight8x8_c(const uint8_t* src,
                               int src_stride,
                               uint8_t* dst,
                               int dst_stride,
                               int src_weight);
void vp9_filter_by_weight8x8_sse2(const uint8_t* src,
                                  int src_stride,
                                  uint8_t* dst,
                                  int dst_stride,
                                  int src_weight);
#ifdef WEBRTC_WEBKIT_DISABLE_HARDWARE_ACCELERATION
#define vp9_filter_by_weight8x8 vp9_filter_by_weight8x8_c
#else
#define vp9_filter_by_weight8x8 vp9_filter_by_weight8x8_sse2
#endif

void vp9_fwht4x4_c(const int16_t* input, tran_low_t* output, int stride);
void vp9_fwht4x4_sse2(const int16_t* input, tran_low_t* output, int stride);
#ifdef WEBRTC_WEBKIT_DISABLE_HARDWARE_ACCELERATION
#define vp9_fwht4x4 vp9_fwht4x4_c
#else
#define vp9_fwht4x4 vp9_fwht4x4_sse2
#endif

int64_t vp9_highbd_block_error_c(const tran_low_t* coeff,
                                 const tran_low_t* dqcoeff,
                                 intptr_t block_size,
                                 int64_t* ssz,
                                 int bd);
int64_t vp9_highbd_block_error_sse2(const tran_low_t* coeff,
                                    const tran_low_t* dqcoeff,
                                    intptr_t block_size,
                                    int64_t* ssz,
                                    int bd);
#ifdef WEBRTC_WEBKIT_DISABLE_HARDWARE_ACCELERATION
#define vp9_highbd_block_error vp9_highbd_block_error_c
#else
#define vp9_highbd_block_error vp9_highbd_block_error_sse2
#endif

void vp9_highbd_fht16x16_c(const int16_t* input,
                           tran_low_t* output,
                           int stride,
                           int tx_type);
#define vp9_highbd_fht16x16 vp9_highbd_fht16x16_c

void vp9_highbd_fht4x4_c(const int16_t* input,
                         tran_low_t* output,
                         int stride,
                         int tx_type);
#define vp9_highbd_fht4x4 vp9_highbd_fht4x4_c

void vp9_highbd_fht8x8_c(const int16_t* input,
                         tran_low_t* output,
                         int stride,
                         int tx_type);
#define vp9_highbd_fht8x8 vp9_highbd_fht8x8_c

void vp9_highbd_fwht4x4_c(const int16_t* input, tran_low_t* output, int stride);
#define vp9_highbd_fwht4x4 vp9_highbd_fwht4x4_c

void vp9_highbd_iht16x16_256_add_c(const tran_low_t* input,
                                   uint16_t* dest,
                                   int stride,
                                   int tx_type,
                                   int bd);
void vp9_highbd_iht16x16_256_add_sse4_1(const tran_low_t* input,
                                        uint16_t* dest,
                                        int stride,
                                        int tx_type,
                                        int bd);
RTCD_EXTERN void (*vp9_highbd_iht16x16_256_add)(const tran_low_t* input,
                                                uint16_t* dest,
                                                int stride,
                                                int tx_type,
                                                int bd);

void vp9_highbd_iht4x4_16_add_c(const tran_low_t* input,
                                uint16_t* dest,
                                int stride,
                                int tx_type,
                                int bd);
void vp9_highbd_iht4x4_16_add_sse4_1(const tran_low_t* input,
                                     uint16_t* dest,
                                     int stride,
                                     int tx_type,
                                     int bd);
RTCD_EXTERN void (*vp9_highbd_iht4x4_16_add)(const tran_low_t* input,
                                             uint16_t* dest,
                                             int stride,
                                             int tx_type,
                                             int bd);

void vp9_highbd_iht8x8_64_add_c(const tran_low_t* input,
                                uint16_t* dest,
                                int stride,
                                int tx_type,
                                int bd);
void vp9_highbd_iht8x8_64_add_sse4_1(const tran_low_t* input,
                                     uint16_t* dest,
                                     int stride,
                                     int tx_type,
                                     int bd);
RTCD_EXTERN void (*vp9_highbd_iht8x8_64_add)(const tran_low_t* input,
                                             uint16_t* dest,
                                             int stride,
                                             int tx_type,
                                             int bd);

void vp9_highbd_mbpost_proc_across_ip_c(uint16_t* src,
                                        int pitch,
                                        int rows,
                                        int cols,
                                        int flimit);
#define vp9_highbd_mbpost_proc_across_ip vp9_highbd_mbpost_proc_across_ip_c

void vp9_highbd_mbpost_proc_down_c(uint16_t* dst,
                                   int pitch,
                                   int rows,
                                   int cols,
                                   int flimit);
#define vp9_highbd_mbpost_proc_down vp9_highbd_mbpost_proc_down_c

void vp9_highbd_post_proc_down_and_across_c(const uint16_t* src_ptr,
                                            uint16_t* dst_ptr,
                                            int src_pixels_per_line,
                                            int dst_pixels_per_line,
                                            int rows,
                                            int cols,
                                            int flimit);
#define vp9_highbd_post_proc_down_and_across \
  vp9_highbd_post_proc_down_and_across_c

void vp9_highbd_quantize_fp_c(const tran_low_t* coeff_ptr,
                              intptr_t n_coeffs,
                              int skip_block,
                              const int16_t* round_ptr,
                              const int16_t* quant_ptr,
                              tran_low_t* qcoeff_ptr,
                              tran_low_t* dqcoeff_ptr,
                              const int16_t* dequant_ptr,
                              uint16_t* eob_ptr,
                              const int16_t* scan,
                              const int16_t* iscan);
#define vp9_highbd_quantize_fp vp9_highbd_quantize_fp_c

void vp9_highbd_quantize_fp_32x32_c(const tran_low_t* coeff_ptr,
                                    intptr_t n_coeffs,
                                    int skip_block,
                                    const int16_t* round_ptr,
                                    const int16_t* quant_ptr,
                                    tran_low_t* qcoeff_ptr,
                                    tran_low_t* dqcoeff_ptr,
                                    const int16_t* dequant_ptr,
                                    uint16_t* eob_ptr,
                                    const int16_t* scan,
                                    const int16_t* iscan);
#define vp9_highbd_quantize_fp_32x32 vp9_highbd_quantize_fp_32x32_c

void vp9_highbd_temporal_filter_apply_c(const uint8_t* frame1,
                                        unsigned int stride,
                                        const uint8_t* frame2,
                                        unsigned int block_width,
                                        unsigned int block_height,
                                        int strength,
                                        int* blk_fw,
                                        int use_32x32,
                                        uint32_t* accumulator,
                                        uint16_t* count);
#define vp9_highbd_temporal_filter_apply vp9_highbd_temporal_filter_apply_c

void vp9_iht16x16_256_add_c(const tran_low_t* input,
                            uint8_t* dest,
                            int stride,
                            int tx_type);
void vp9_iht16x16_256_add_sse2(const tran_low_t* input,
                               uint8_t* dest,
                               int stride,
                               int tx_type);
#ifdef WEBRTC_WEBKIT_DISABLE_HARDWARE_ACCELERATION
#define vp9_iht16x16_256_add vp9_iht16x16_256_add_c
#else
#define vp9_iht16x16_256_add vp9_iht16x16_256_add_sse2
#endif

void vp9_iht4x4_16_add_c(const tran_low_t* input,
                         uint8_t* dest,
                         int stride,
                         int tx_type);
void vp9_iht4x4_16_add_sse2(const tran_low_t* input,
                            uint8_t* dest,
                            int stride,
                            int tx_type);
#ifdef WEBRTC_WEBKIT_DISABLE_HARDWARE_ACCELERATION
#define vp9_iht4x4_16_add vp9_iht4x4_16_add_c
#else
#define vp9_iht4x4_16_add vp9_iht4x4_16_add_sse2
#endif

void vp9_iht8x8_64_add_c(const tran_low_t* input,
                         uint8_t* dest,
                         int stride,
                         int tx_type);
void vp9_iht8x8_64_add_sse2(const tran_low_t* input,
                            uint8_t* dest,
                            int stride,
                            int tx_type);
#ifdef WEBRTC_WEBKIT_DISABLE_HARDWARE_ACCELERATION
#define vp9_iht8x8_64_add vp9_iht8x8_64_add_c
#else
#define vp9_iht8x8_64_add vp9_iht8x8_64_add_sse2
#endif

void vp9_quantize_fp_c(const tran_low_t* coeff_ptr,
                       intptr_t n_coeffs,
                       int skip_block,
                       const int16_t* round_ptr,
                       const int16_t* quant_ptr,
                       tran_low_t* qcoeff_ptr,
                       tran_low_t* dqcoeff_ptr,
                       const int16_t* dequant_ptr,
                       uint16_t* eob_ptr,
                       const int16_t* scan,
                       const int16_t* iscan);
void vp9_quantize_fp_sse2(const tran_low_t* coeff_ptr,
                          intptr_t n_coeffs,
                          int skip_block,
                          const int16_t* round_ptr,
                          const int16_t* quant_ptr,
                          tran_low_t* qcoeff_ptr,
                          tran_low_t* dqcoeff_ptr,
                          const int16_t* dequant_ptr,
                          uint16_t* eob_ptr,
                          const int16_t* scan,
                          const int16_t* iscan);
void vp9_quantize_fp_ssse3(const tran_low_t* coeff_ptr,
                           intptr_t n_coeffs,
                           int skip_block,
                           const int16_t* round_ptr,
                           const int16_t* quant_ptr,
                           tran_low_t* qcoeff_ptr,
                           tran_low_t* dqcoeff_ptr,
                           const int16_t* dequant_ptr,
                           uint16_t* eob_ptr,
                           const int16_t* scan,
                           const int16_t* iscan);
void vp9_quantize_fp_avx2(const tran_low_t* coeff_ptr,
                          intptr_t n_coeffs,
                          int skip_block,
                          const int16_t* round_ptr,
                          const int16_t* quant_ptr,
                          tran_low_t* qcoeff_ptr,
                          tran_low_t* dqcoeff_ptr,
                          const int16_t* dequant_ptr,
                          uint16_t* eob_ptr,
                          const int16_t* scan,
                          const int16_t* iscan);
RTCD_EXTERN void (*vp9_quantize_fp)(const tran_low_t* coeff_ptr,
                                    intptr_t n_coeffs,
                                    int skip_block,
                                    const int16_t* round_ptr,
                                    const int16_t* quant_ptr,
                                    tran_low_t* qcoeff_ptr,
                                    tran_low_t* dqcoeff_ptr,
                                    const int16_t* dequant_ptr,
                                    uint16_t* eob_ptr,
                                    const int16_t* scan,
                                    const int16_t* iscan);

void vp9_quantize_fp_32x32_c(const tran_low_t* coeff_ptr,
                             intptr_t n_coeffs,
                             int skip_block,
                             const int16_t* round_ptr,
                             const int16_t* quant_ptr,
                             tran_low_t* qcoeff_ptr,
                             tran_low_t* dqcoeff_ptr,
                             const int16_t* dequant_ptr,
                             uint16_t* eob_ptr,
                             const int16_t* scan,
                             const int16_t* iscan);
void vp9_quantize_fp_32x32_ssse3(const tran_low_t* coeff_ptr,
                                 intptr_t n_coeffs,
                                 int skip_block,
                                 const int16_t* round_ptr,
                                 const int16_t* quant_ptr,
                                 tran_low_t* qcoeff_ptr,
                                 tran_low_t* dqcoeff_ptr,
                                 const int16_t* dequant_ptr,
                                 uint16_t* eob_ptr,
                                 const int16_t* scan,
                                 const int16_t* iscan);
RTCD_EXTERN void (*vp9_quantize_fp_32x32)(const tran_low_t* coeff_ptr,
                                          intptr_t n_coeffs,
                                          int skip_block,
                                          const int16_t* round_ptr,
                                          const int16_t* quant_ptr,
                                          tran_low_t* qcoeff_ptr,
                                          tran_low_t* dqcoeff_ptr,
                                          const int16_t* dequant_ptr,
                                          uint16_t* eob_ptr,
                                          const int16_t* scan,
                                          const int16_t* iscan);

void vp9_scale_and_extend_frame_c(const struct yv12_buffer_config* src,
                                  struct yv12_buffer_config* dst,
                                  INTERP_FILTER filter_type,
                                  int phase_scaler);
void vp9_scale_and_extend_frame_ssse3(const struct yv12_buffer_config* src,
                                      struct yv12_buffer_config* dst,
                                      INTERP_FILTER filter_type,
                                      int phase_scaler);
RTCD_EXTERN void (*vp9_scale_and_extend_frame)(
    const struct yv12_buffer_config* src,
    struct yv12_buffer_config* dst,
    INTERP_FILTER filter_type,
    int phase_scaler);

void vp9_apply_temporal_filter_c(const uint8_t *y_src, int y_src_stride, const uint8_t *y_pre, int y_pre_stride, const uint8_t *u_src, const uint8_t *v_src, int uv_src_stride, const uint8_t *u_pre, const uint8_t *v_pre, int uv_pre_stride, unsigned int block_width, unsigned int block_height, int ss_x, int ss_y, int strength, const int *const blk_fw, int use_32x32, uint32_t *y_accumulator, uint16_t *y_count, uint32_t *u_accumulator, uint16_t *u_count, uint32_t *v_accumulator, uint16_t *v_count);
void vp9_apply_temporal_filter_sse4_1(const uint8_t *y_src, int y_src_stride, const uint8_t *y_pre, int y_pre_stride, const uint8_t *u_src, const uint8_t *v_src, int uv_src_stride, const uint8_t *u_pre, const uint8_t *v_pre, int uv_pre_stride, unsigned int block_width, unsigned int block_height, int ss_x, int ss_y, int strength, const int *const blk_fw, int use_32x32, uint32_t *y_accumulator, uint16_t *y_count, uint32_t *u_accumulator, uint16_t *u_count, uint32_t *v_accumulator, uint16_t *v_count);
RTCD_EXTERN void (*vp9_apply_temporal_filter)(const uint8_t *y_src, int y_src_stride, const uint8_t *y_pre, int y_pre_stride, const uint8_t *u_src, const uint8_t *v_src, int uv_src_stride, const uint8_t *u_pre, const uint8_t *v_pre, int uv_pre_stride, unsigned int block_width, unsigned int block_height, int ss_x, int ss_y, int strength, const int *const blk_fw, int use_32x32, uint32_t *y_accumulator, uint16_t *y_count, uint32_t *u_accumulator, uint16_t *u_count, uint32_t *v_accumulator, uint16_t *v_count);

void vp9_highbd_apply_temporal_filter_c(const uint16_t *y_src, int y_src_stride, const uint16_t *y_pre, int y_pre_stride, const uint16_t *u_src, const uint16_t *v_src, int uv_src_stride, const uint16_t *u_pre, const uint16_t *v_pre, int uv_pre_stride, unsigned int block_width, unsigned int block_height, int ss_x, int ss_y, int strength, const int *const blk_fw, int use_32x32, uint32_t *y_accum, uint16_t *y_count, uint32_t *u_accum, uint16_t *u_count, uint32_t *v_accum, uint16_t *v_count);
void vp9_highbd_apply_temporal_filter_sse4_1(const uint16_t *y_src, int y_src_stride, const uint16_t *y_pre, int y_pre_stride, const uint16_t *u_src, const uint16_t *v_src, int uv_src_stride, const uint16_t *u_pre, const uint16_t *v_pre, int uv_pre_stride, unsigned int block_width, unsigned int block_height, int ss_x, int ss_y, int strength, const int *const blk_fw, int use_32x32, uint32_t *y_accum, uint16_t *y_count, uint32_t *u_accum, uint16_t *u_count, uint32_t *v_accum, uint16_t *v_count);
RTCD_EXTERN void (*vp9_highbd_apply_temporal_filter)(const uint16_t *y_src, int y_src_stride, const uint16_t *y_pre, int y_pre_stride, const uint16_t *u_src, const uint16_t *v_src, int uv_src_stride, const uint16_t *u_pre, const uint16_t *v_pre, int uv_pre_stride, unsigned int block_width, unsigned int block_height, int ss_x, int ss_y, int strength, const int *const blk_fw, int use_32x32, uint32_t *y_accum, uint16_t *y_count, uint32_t *u_accum, uint16_t *u_count, uint32_t *v_accum, uint16_t *v_count);

void vp9_rtcd(void);

#ifdef RTCD_C
#include "vpx_ports/x86.h"

static void setup_rtcd_internal(void) {
  int flags = x86_simd_caps();

  (void)flags;

#ifdef WEBRTC_WEBKIT_DISABLE_HARDWARE_ACCELERATION
  vp9_block_error = vp9_block_error_c;
  vp9_block_error_fp = vp9_block_error_fp_c;
#else
  vp9_block_error = vp9_block_error_sse2;
  vp9_block_error_fp = vp9_block_error_fp_sse2;
#endif
  vp9_diamond_search_sad = vp9_diamond_search_sad_c;
#if !defined(WEBRTC_WEBKIT_DISABLE_HARDWARE_ACCELERATION) && !defined(WEBRTC_WEBKIT_MAC_CATALIST)
  if (flags & HAS_AVX)
    vp9_diamond_search_sad = vp9_diamond_search_sad_avx;
#endif
  vp9_highbd_iht16x16_256_add = vp9_highbd_iht16x16_256_add_c;
#if !defined(WEBRTC_WEBKIT_DISABLE_HARDWARE_ACCELERATION) && !defined(WEBRTC_WEBKIT_MAC_CATALIST)
  if (flags & HAS_SSE4_1)
    vp9_highbd_iht16x16_256_add = vp9_highbd_iht16x16_256_add_sse4_1;
#endif
  vp9_highbd_iht4x4_16_add = vp9_highbd_iht4x4_16_add_c;
#if !defined(WEBRTC_WEBKIT_DISABLE_HARDWARE_ACCELERATION) && !defined(WEBRTC_WEBKIT_MAC_CATALIST)
  if (flags & HAS_SSE4_1)
    vp9_highbd_iht4x4_16_add = vp9_highbd_iht4x4_16_add_sse4_1;
#endif
  vp9_highbd_iht8x8_64_add = vp9_highbd_iht8x8_64_add_c;
#if !defined(WEBRTC_WEBKIT_DISABLE_HARDWARE_ACCELERATION) && !defined(WEBRTC_WEBKIT_MAC_CATALIST)
  if (flags & HAS_SSE4_1)
    vp9_highbd_iht8x8_64_add = vp9_highbd_iht8x8_64_add_sse4_1;
#endif
#ifdef WEBRTC_WEBKIT_DISABLE_HARDWARE_ACCELERATION
  vp9_quantize_fp = vp9_quantize_fp_c;
#else
  vp9_quantize_fp = vp9_quantize_fp_sse2;
  if (flags & HAS_SSSE3)
    vp9_quantize_fp = vp9_quantize_fp_ssse3;
#endif
  vp9_quantize_fp_32x32 = vp9_quantize_fp_32x32_c;
#ifndef WEBRTC_WEBKIT_DISABLE_HARDWARE_ACCELERATION
  if (flags & HAS_SSSE3)
    vp9_quantize_fp_32x32 = vp9_quantize_fp_32x32_ssse3;
#endif
  vp9_scale_and_extend_frame = vp9_scale_and_extend_frame_c;
#ifndef WEBRTC_WEBKIT_DISABLE_HARDWARE_ACCELERATION
  if (flags & HAS_SSSE3)
    vp9_scale_and_extend_frame = vp9_scale_and_extend_frame_ssse3;
#endif
  vp9_apply_temporal_filter = vp9_apply_temporal_filter_c;
#if !defined(WEBRTC_WEBKIT_DISABLE_HARDWARE_ACCELERATION) && !defined(WEBRTC_WEBKIT_MAC_CATALIST)
  if (flags & HAS_SSE4_1)
    vp9_apply_temporal_filter = vp9_apply_temporal_filter_sse4_1;
#endif
  vp9_highbd_apply_temporal_filter = vp9_highbd_apply_temporal_filter_c;
#if !defined(WEBRTC_WEBKIT_DISABLE_HARDWARE_ACCELERATION) && !defined(WEBRTC_WEBKIT_MAC_CATALIST)
  if (flags & HAS_SSE4_1)
    vp9_highbd_apply_temporal_filter = vp9_highbd_apply_temporal_filter_sse4_1;
#endif
}
#endif

#ifdef __cplusplus
}  // extern "C"
#endif

#endif
