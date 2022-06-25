// This file is generated. Do not edit.
#ifndef VPX_DSP_RTCD_H_
#define VPX_DSP_RTCD_H_

#ifdef RTCD_C
#define RTCD_EXTERN
#else
#define RTCD_EXTERN extern
#endif

/*
 * DSP
 */

#include "vpx/vpx_integer.h"
#include "vpx_dsp/vpx_dsp_common.h"
#include "vpx_dsp/vpx_filter.h"

#ifdef __cplusplus
extern "C" {
#endif

unsigned int vpx_avg_4x4_c(const uint8_t*, int p);
unsigned int vpx_avg_4x4_neon(const uint8_t*, int p);
RTCD_EXTERN unsigned int (*vpx_avg_4x4)(const uint8_t*, int p);

unsigned int vpx_avg_8x8_c(const uint8_t*, int p);
unsigned int vpx_avg_8x8_neon(const uint8_t*, int p);
RTCD_EXTERN unsigned int (*vpx_avg_8x8)(const uint8_t*, int p);

void vpx_comp_avg_pred_c(uint8_t* comp_pred,
                         const uint8_t* pred,
                         int width,
                         int height,
                         const uint8_t* ref,
                         int ref_stride);
void vpx_comp_avg_pred_neon(uint8_t* comp_pred,
                            const uint8_t* pred,
                            int width,
                            int height,
                            const uint8_t* ref,
                            int ref_stride);
RTCD_EXTERN void (*vpx_comp_avg_pred)(uint8_t* comp_pred,
                                      const uint8_t* pred,
                                      int width,
                                      int height,
                                      const uint8_t* ref,
                                      int ref_stride);

void vpx_convolve8_c(const uint8_t* src,
                     ptrdiff_t src_stride,
                     uint8_t* dst,
                     ptrdiff_t dst_stride,
                     const InterpKernel* filter,
                     int x0_q4,
                     int x_step_q4,
                     int y0_q4,
                     int y_step_q4,
                     int w,
                     int h);
void vpx_convolve8_neon(const uint8_t* src,
                        ptrdiff_t src_stride,
                        uint8_t* dst,
                        ptrdiff_t dst_stride,
                        const InterpKernel* filter,
                        int x0_q4,
                        int x_step_q4,
                        int y0_q4,
                        int y_step_q4,
                        int w,
                        int h);
RTCD_EXTERN void (*vpx_convolve8)(const uint8_t* src,
                                  ptrdiff_t src_stride,
                                  uint8_t* dst,
                                  ptrdiff_t dst_stride,
                                  const InterpKernel* filter,
                                  int x0_q4,
                                  int x_step_q4,
                                  int y0_q4,
                                  int y_step_q4,
                                  int w,
                                  int h);

void vpx_convolve8_avg_c(const uint8_t* src,
                         ptrdiff_t src_stride,
                         uint8_t* dst,
                         ptrdiff_t dst_stride,
                         const InterpKernel* filter,
                         int x0_q4,
                         int x_step_q4,
                         int y0_q4,
                         int y_step_q4,
                         int w,
                         int h);
void vpx_convolve8_avg_neon(const uint8_t* src,
                            ptrdiff_t src_stride,
                            uint8_t* dst,
                            ptrdiff_t dst_stride,
                            const InterpKernel* filter,
                            int x0_q4,
                            int x_step_q4,
                            int y0_q4,
                            int y_step_q4,
                            int w,
                            int h);
RTCD_EXTERN void (*vpx_convolve8_avg)(const uint8_t* src,
                                      ptrdiff_t src_stride,
                                      uint8_t* dst,
                                      ptrdiff_t dst_stride,
                                      const InterpKernel* filter,
                                      int x0_q4,
                                      int x_step_q4,
                                      int y0_q4,
                                      int y_step_q4,
                                      int w,
                                      int h);

void vpx_convolve8_avg_horiz_c(const uint8_t* src,
                               ptrdiff_t src_stride,
                               uint8_t* dst,
                               ptrdiff_t dst_stride,
                               const InterpKernel* filter,
                               int x0_q4,
                               int x_step_q4,
                               int y0_q4,
                               int y_step_q4,
                               int w,
                               int h);
void vpx_convolve8_avg_horiz_neon(const uint8_t* src,
                                  ptrdiff_t src_stride,
                                  uint8_t* dst,
                                  ptrdiff_t dst_stride,
                                  const InterpKernel* filter,
                                  int x0_q4,
                                  int x_step_q4,
                                  int y0_q4,
                                  int y_step_q4,
                                  int w,
                                  int h);
RTCD_EXTERN void (*vpx_convolve8_avg_horiz)(const uint8_t* src,
                                            ptrdiff_t src_stride,
                                            uint8_t* dst,
                                            ptrdiff_t dst_stride,
                                            const InterpKernel* filter,
                                            int x0_q4,
                                            int x_step_q4,
                                            int y0_q4,
                                            int y_step_q4,
                                            int w,
                                            int h);

void vpx_convolve8_avg_vert_c(const uint8_t* src,
                              ptrdiff_t src_stride,
                              uint8_t* dst,
                              ptrdiff_t dst_stride,
                              const InterpKernel* filter,
                              int x0_q4,
                              int x_step_q4,
                              int y0_q4,
                              int y_step_q4,
                              int w,
                              int h);
void vpx_convolve8_avg_vert_neon(const uint8_t* src,
                                 ptrdiff_t src_stride,
                                 uint8_t* dst,
                                 ptrdiff_t dst_stride,
                                 const InterpKernel* filter,
                                 int x0_q4,
                                 int x_step_q4,
                                 int y0_q4,
                                 int y_step_q4,
                                 int w,
                                 int h);
RTCD_EXTERN void (*vpx_convolve8_avg_vert)(const uint8_t* src,
                                           ptrdiff_t src_stride,
                                           uint8_t* dst,
                                           ptrdiff_t dst_stride,
                                           const InterpKernel* filter,
                                           int x0_q4,
                                           int x_step_q4,
                                           int y0_q4,
                                           int y_step_q4,
                                           int w,
                                           int h);

void vpx_convolve8_horiz_c(const uint8_t* src,
                           ptrdiff_t src_stride,
                           uint8_t* dst,
                           ptrdiff_t dst_stride,
                           const InterpKernel* filter,
                           int x0_q4,
                           int x_step_q4,
                           int y0_q4,
                           int y_step_q4,
                           int w,
                           int h);
void vpx_convolve8_horiz_neon(const uint8_t* src,
                              ptrdiff_t src_stride,
                              uint8_t* dst,
                              ptrdiff_t dst_stride,
                              const InterpKernel* filter,
                              int x0_q4,
                              int x_step_q4,
                              int y0_q4,
                              int y_step_q4,
                              int w,
                              int h);
RTCD_EXTERN void (*vpx_convolve8_horiz)(const uint8_t* src,
                                        ptrdiff_t src_stride,
                                        uint8_t* dst,
                                        ptrdiff_t dst_stride,
                                        const InterpKernel* filter,
                                        int x0_q4,
                                        int x_step_q4,
                                        int y0_q4,
                                        int y_step_q4,
                                        int w,
                                        int h);

void vpx_convolve8_vert_c(const uint8_t* src,
                          ptrdiff_t src_stride,
                          uint8_t* dst,
                          ptrdiff_t dst_stride,
                          const InterpKernel* filter,
                          int x0_q4,
                          int x_step_q4,
                          int y0_q4,
                          int y_step_q4,
                          int w,
                          int h);
void vpx_convolve8_vert_neon(const uint8_t* src,
                             ptrdiff_t src_stride,
                             uint8_t* dst,
                             ptrdiff_t dst_stride,
                             const InterpKernel* filter,
                             int x0_q4,
                             int x_step_q4,
                             int y0_q4,
                             int y_step_q4,
                             int w,
                             int h);
RTCD_EXTERN void (*vpx_convolve8_vert)(const uint8_t* src,
                                       ptrdiff_t src_stride,
                                       uint8_t* dst,
                                       ptrdiff_t dst_stride,
                                       const InterpKernel* filter,
                                       int x0_q4,
                                       int x_step_q4,
                                       int y0_q4,
                                       int y_step_q4,
                                       int w,
                                       int h);

void vpx_convolve_avg_c(const uint8_t* src,
                        ptrdiff_t src_stride,
                        uint8_t* dst,
                        ptrdiff_t dst_stride,
                        const InterpKernel* filter,
                        int x0_q4,
                        int x_step_q4,
                        int y0_q4,
                        int y_step_q4,
                        int w,
                        int h);
void vpx_convolve_avg_neon(const uint8_t* src,
                           ptrdiff_t src_stride,
                           uint8_t* dst,
                           ptrdiff_t dst_stride,
                           const InterpKernel* filter,
                           int x0_q4,
                           int x_step_q4,
                           int y0_q4,
                           int y_step_q4,
                           int w,
                           int h);
RTCD_EXTERN void (*vpx_convolve_avg)(const uint8_t* src,
                                     ptrdiff_t src_stride,
                                     uint8_t* dst,
                                     ptrdiff_t dst_stride,
                                     const InterpKernel* filter,
                                     int x0_q4,
                                     int x_step_q4,
                                     int y0_q4,
                                     int y_step_q4,
                                     int w,
                                     int h);

void vpx_convolve_copy_c(const uint8_t* src,
                         ptrdiff_t src_stride,
                         uint8_t* dst,
                         ptrdiff_t dst_stride,
                         const InterpKernel* filter,
                         int x0_q4,
                         int x_step_q4,
                         int y0_q4,
                         int y_step_q4,
                         int w,
                         int h);
void vpx_convolve_copy_neon(const uint8_t* src,
                            ptrdiff_t src_stride,
                            uint8_t* dst,
                            ptrdiff_t dst_stride,
                            const InterpKernel* filter,
                            int x0_q4,
                            int x_step_q4,
                            int y0_q4,
                            int y_step_q4,
                            int w,
                            int h);
RTCD_EXTERN void (*vpx_convolve_copy)(const uint8_t* src,
                                      ptrdiff_t src_stride,
                                      uint8_t* dst,
                                      ptrdiff_t dst_stride,
                                      const InterpKernel* filter,
                                      int x0_q4,
                                      int x_step_q4,
                                      int y0_q4,
                                      int y_step_q4,
                                      int w,
                                      int h);

void vpx_d117_predictor_16x16_c(uint8_t* dst,
                                ptrdiff_t stride,
                                const uint8_t* above,
                                const uint8_t* left);
#define vpx_d117_predictor_16x16 vpx_d117_predictor_16x16_c

void vpx_d117_predictor_32x32_c(uint8_t* dst,
                                ptrdiff_t stride,
                                const uint8_t* above,
                                const uint8_t* left);
#define vpx_d117_predictor_32x32 vpx_d117_predictor_32x32_c

void vpx_d117_predictor_4x4_c(uint8_t* dst,
                              ptrdiff_t stride,
                              const uint8_t* above,
                              const uint8_t* left);
#define vpx_d117_predictor_4x4 vpx_d117_predictor_4x4_c

void vpx_d117_predictor_8x8_c(uint8_t* dst,
                              ptrdiff_t stride,
                              const uint8_t* above,
                              const uint8_t* left);
#define vpx_d117_predictor_8x8 vpx_d117_predictor_8x8_c

void vpx_d135_predictor_16x16_c(uint8_t* dst,
                                ptrdiff_t stride,
                                const uint8_t* above,
                                const uint8_t* left);
void vpx_d135_predictor_16x16_neon(uint8_t* dst,
                                   ptrdiff_t stride,
                                   const uint8_t* above,
                                   const uint8_t* left);
RTCD_EXTERN void (*vpx_d135_predictor_16x16)(uint8_t* dst,
                                             ptrdiff_t stride,
                                             const uint8_t* above,
                                             const uint8_t* left);

void vpx_d135_predictor_32x32_c(uint8_t* dst,
                                ptrdiff_t stride,
                                const uint8_t* above,
                                const uint8_t* left);
void vpx_d135_predictor_32x32_neon(uint8_t* dst,
                                   ptrdiff_t stride,
                                   const uint8_t* above,
                                   const uint8_t* left);
RTCD_EXTERN void (*vpx_d135_predictor_32x32)(uint8_t* dst,
                                             ptrdiff_t stride,
                                             const uint8_t* above,
                                             const uint8_t* left);

void vpx_d135_predictor_4x4_c(uint8_t* dst,
                              ptrdiff_t stride,
                              const uint8_t* above,
                              const uint8_t* left);
void vpx_d135_predictor_4x4_neon(uint8_t* dst,
                                 ptrdiff_t stride,
                                 const uint8_t* above,
                                 const uint8_t* left);
RTCD_EXTERN void (*vpx_d135_predictor_4x4)(uint8_t* dst,
                                           ptrdiff_t stride,
                                           const uint8_t* above,
                                           const uint8_t* left);

void vpx_d135_predictor_8x8_c(uint8_t* dst,
                              ptrdiff_t stride,
                              const uint8_t* above,
                              const uint8_t* left);
void vpx_d135_predictor_8x8_neon(uint8_t* dst,
                                 ptrdiff_t stride,
                                 const uint8_t* above,
                                 const uint8_t* left);
RTCD_EXTERN void (*vpx_d135_predictor_8x8)(uint8_t* dst,
                                           ptrdiff_t stride,
                                           const uint8_t* above,
                                           const uint8_t* left);

void vpx_d153_predictor_16x16_c(uint8_t* dst,
                                ptrdiff_t stride,
                                const uint8_t* above,
                                const uint8_t* left);
#define vpx_d153_predictor_16x16 vpx_d153_predictor_16x16_c

void vpx_d153_predictor_32x32_c(uint8_t* dst,
                                ptrdiff_t stride,
                                const uint8_t* above,
                                const uint8_t* left);
#define vpx_d153_predictor_32x32 vpx_d153_predictor_32x32_c

void vpx_d153_predictor_4x4_c(uint8_t* dst,
                              ptrdiff_t stride,
                              const uint8_t* above,
                              const uint8_t* left);
#define vpx_d153_predictor_4x4 vpx_d153_predictor_4x4_c

void vpx_d153_predictor_8x8_c(uint8_t* dst,
                              ptrdiff_t stride,
                              const uint8_t* above,
                              const uint8_t* left);
#define vpx_d153_predictor_8x8 vpx_d153_predictor_8x8_c

void vpx_d207_predictor_16x16_c(uint8_t* dst,
                                ptrdiff_t stride,
                                const uint8_t* above,
                                const uint8_t* left);
#define vpx_d207_predictor_16x16 vpx_d207_predictor_16x16_c

void vpx_d207_predictor_32x32_c(uint8_t* dst,
                                ptrdiff_t stride,
                                const uint8_t* above,
                                const uint8_t* left);
#define vpx_d207_predictor_32x32 vpx_d207_predictor_32x32_c

void vpx_d207_predictor_4x4_c(uint8_t* dst,
                              ptrdiff_t stride,
                              const uint8_t* above,
                              const uint8_t* left);
#define vpx_d207_predictor_4x4 vpx_d207_predictor_4x4_c

void vpx_d207_predictor_8x8_c(uint8_t* dst,
                              ptrdiff_t stride,
                              const uint8_t* above,
                              const uint8_t* left);
#define vpx_d207_predictor_8x8 vpx_d207_predictor_8x8_c

void vpx_d45_predictor_16x16_c(uint8_t* dst,
                               ptrdiff_t stride,
                               const uint8_t* above,
                               const uint8_t* left);
void vpx_d45_predictor_16x16_neon(uint8_t* dst,
                                  ptrdiff_t stride,
                                  const uint8_t* above,
                                  const uint8_t* left);
RTCD_EXTERN void (*vpx_d45_predictor_16x16)(uint8_t* dst,
                                            ptrdiff_t stride,
                                            const uint8_t* above,
                                            const uint8_t* left);

void vpx_d45_predictor_32x32_c(uint8_t* dst,
                               ptrdiff_t stride,
                               const uint8_t* above,
                               const uint8_t* left);
void vpx_d45_predictor_32x32_neon(uint8_t* dst,
                                  ptrdiff_t stride,
                                  const uint8_t* above,
                                  const uint8_t* left);
RTCD_EXTERN void (*vpx_d45_predictor_32x32)(uint8_t* dst,
                                            ptrdiff_t stride,
                                            const uint8_t* above,
                                            const uint8_t* left);

void vpx_d45_predictor_4x4_c(uint8_t* dst,
                             ptrdiff_t stride,
                             const uint8_t* above,
                             const uint8_t* left);
void vpx_d45_predictor_4x4_neon(uint8_t* dst,
                                ptrdiff_t stride,
                                const uint8_t* above,
                                const uint8_t* left);
RTCD_EXTERN void (*vpx_d45_predictor_4x4)(uint8_t* dst,
                                          ptrdiff_t stride,
                                          const uint8_t* above,
                                          const uint8_t* left);

void vpx_d45_predictor_8x8_c(uint8_t* dst,
                             ptrdiff_t stride,
                             const uint8_t* above,
                             const uint8_t* left);
void vpx_d45_predictor_8x8_neon(uint8_t* dst,
                                ptrdiff_t stride,
                                const uint8_t* above,
                                const uint8_t* left);
RTCD_EXTERN void (*vpx_d45_predictor_8x8)(uint8_t* dst,
                                          ptrdiff_t stride,
                                          const uint8_t* above,
                                          const uint8_t* left);

void vpx_d45e_predictor_4x4_c(uint8_t* dst,
                              ptrdiff_t stride,
                              const uint8_t* above,
                              const uint8_t* left);
#define vpx_d45e_predictor_4x4 vpx_d45e_predictor_4x4_c

void vpx_d63_predictor_16x16_c(uint8_t* dst,
                               ptrdiff_t stride,
                               const uint8_t* above,
                               const uint8_t* left);
#define vpx_d63_predictor_16x16 vpx_d63_predictor_16x16_c

void vpx_d63_predictor_32x32_c(uint8_t* dst,
                               ptrdiff_t stride,
                               const uint8_t* above,
                               const uint8_t* left);
#define vpx_d63_predictor_32x32 vpx_d63_predictor_32x32_c

void vpx_d63_predictor_4x4_c(uint8_t* dst,
                             ptrdiff_t stride,
                             const uint8_t* above,
                             const uint8_t* left);
#define vpx_d63_predictor_4x4 vpx_d63_predictor_4x4_c

void vpx_d63_predictor_8x8_c(uint8_t* dst,
                             ptrdiff_t stride,
                             const uint8_t* above,
                             const uint8_t* left);
#define vpx_d63_predictor_8x8 vpx_d63_predictor_8x8_c

void vpx_d63e_predictor_4x4_c(uint8_t* dst,
                              ptrdiff_t stride,
                              const uint8_t* above,
                              const uint8_t* left);
#define vpx_d63e_predictor_4x4 vpx_d63e_predictor_4x4_c

void vpx_dc_128_predictor_16x16_c(uint8_t* dst,
                                  ptrdiff_t stride,
                                  const uint8_t* above,
                                  const uint8_t* left);
void vpx_dc_128_predictor_16x16_neon(uint8_t* dst,
                                     ptrdiff_t stride,
                                     const uint8_t* above,
                                     const uint8_t* left);
RTCD_EXTERN void (*vpx_dc_128_predictor_16x16)(uint8_t* dst,
                                               ptrdiff_t stride,
                                               const uint8_t* above,
                                               const uint8_t* left);

void vpx_dc_128_predictor_32x32_c(uint8_t* dst,
                                  ptrdiff_t stride,
                                  const uint8_t* above,
                                  const uint8_t* left);
void vpx_dc_128_predictor_32x32_neon(uint8_t* dst,
                                     ptrdiff_t stride,
                                     const uint8_t* above,
                                     const uint8_t* left);
RTCD_EXTERN void (*vpx_dc_128_predictor_32x32)(uint8_t* dst,
                                               ptrdiff_t stride,
                                               const uint8_t* above,
                                               const uint8_t* left);

void vpx_dc_128_predictor_4x4_c(uint8_t* dst,
                                ptrdiff_t stride,
                                const uint8_t* above,
                                const uint8_t* left);
void vpx_dc_128_predictor_4x4_neon(uint8_t* dst,
                                   ptrdiff_t stride,
                                   const uint8_t* above,
                                   const uint8_t* left);
RTCD_EXTERN void (*vpx_dc_128_predictor_4x4)(uint8_t* dst,
                                             ptrdiff_t stride,
                                             const uint8_t* above,
                                             const uint8_t* left);

void vpx_dc_128_predictor_8x8_c(uint8_t* dst,
                                ptrdiff_t stride,
                                const uint8_t* above,
                                const uint8_t* left);
void vpx_dc_128_predictor_8x8_neon(uint8_t* dst,
                                   ptrdiff_t stride,
                                   const uint8_t* above,
                                   const uint8_t* left);
RTCD_EXTERN void (*vpx_dc_128_predictor_8x8)(uint8_t* dst,
                                             ptrdiff_t stride,
                                             const uint8_t* above,
                                             const uint8_t* left);

void vpx_dc_left_predictor_16x16_c(uint8_t* dst,
                                   ptrdiff_t stride,
                                   const uint8_t* above,
                                   const uint8_t* left);
void vpx_dc_left_predictor_16x16_neon(uint8_t* dst,
                                      ptrdiff_t stride,
                                      const uint8_t* above,
                                      const uint8_t* left);
RTCD_EXTERN void (*vpx_dc_left_predictor_16x16)(uint8_t* dst,
                                                ptrdiff_t stride,
                                                const uint8_t* above,
                                                const uint8_t* left);

void vpx_dc_left_predictor_32x32_c(uint8_t* dst,
                                   ptrdiff_t stride,
                                   const uint8_t* above,
                                   const uint8_t* left);
void vpx_dc_left_predictor_32x32_neon(uint8_t* dst,
                                      ptrdiff_t stride,
                                      const uint8_t* above,
                                      const uint8_t* left);
RTCD_EXTERN void (*vpx_dc_left_predictor_32x32)(uint8_t* dst,
                                                ptrdiff_t stride,
                                                const uint8_t* above,
                                                const uint8_t* left);

void vpx_dc_left_predictor_4x4_c(uint8_t* dst,
                                 ptrdiff_t stride,
                                 const uint8_t* above,
                                 const uint8_t* left);
void vpx_dc_left_predictor_4x4_neon(uint8_t* dst,
                                    ptrdiff_t stride,
                                    const uint8_t* above,
                                    const uint8_t* left);
RTCD_EXTERN void (*vpx_dc_left_predictor_4x4)(uint8_t* dst,
                                              ptrdiff_t stride,
                                              const uint8_t* above,
                                              const uint8_t* left);

void vpx_dc_left_predictor_8x8_c(uint8_t* dst,
                                 ptrdiff_t stride,
                                 const uint8_t* above,
                                 const uint8_t* left);
void vpx_dc_left_predictor_8x8_neon(uint8_t* dst,
                                    ptrdiff_t stride,
                                    const uint8_t* above,
                                    const uint8_t* left);
RTCD_EXTERN void (*vpx_dc_left_predictor_8x8)(uint8_t* dst,
                                              ptrdiff_t stride,
                                              const uint8_t* above,
                                              const uint8_t* left);

void vpx_dc_predictor_16x16_c(uint8_t* dst,
                              ptrdiff_t stride,
                              const uint8_t* above,
                              const uint8_t* left);
void vpx_dc_predictor_16x16_neon(uint8_t* dst,
                                 ptrdiff_t stride,
                                 const uint8_t* above,
                                 const uint8_t* left);
RTCD_EXTERN void (*vpx_dc_predictor_16x16)(uint8_t* dst,
                                           ptrdiff_t stride,
                                           const uint8_t* above,
                                           const uint8_t* left);

void vpx_dc_predictor_32x32_c(uint8_t* dst,
                              ptrdiff_t stride,
                              const uint8_t* above,
                              const uint8_t* left);
void vpx_dc_predictor_32x32_neon(uint8_t* dst,
                                 ptrdiff_t stride,
                                 const uint8_t* above,
                                 const uint8_t* left);
RTCD_EXTERN void (*vpx_dc_predictor_32x32)(uint8_t* dst,
                                           ptrdiff_t stride,
                                           const uint8_t* above,
                                           const uint8_t* left);

void vpx_dc_predictor_4x4_c(uint8_t* dst,
                            ptrdiff_t stride,
                            const uint8_t* above,
                            const uint8_t* left);
void vpx_dc_predictor_4x4_neon(uint8_t* dst,
                               ptrdiff_t stride,
                               const uint8_t* above,
                               const uint8_t* left);
RTCD_EXTERN void (*vpx_dc_predictor_4x4)(uint8_t* dst,
                                         ptrdiff_t stride,
                                         const uint8_t* above,
                                         const uint8_t* left);

void vpx_dc_predictor_8x8_c(uint8_t* dst,
                            ptrdiff_t stride,
                            const uint8_t* above,
                            const uint8_t* left);
void vpx_dc_predictor_8x8_neon(uint8_t* dst,
                               ptrdiff_t stride,
                               const uint8_t* above,
                               const uint8_t* left);
RTCD_EXTERN void (*vpx_dc_predictor_8x8)(uint8_t* dst,
                                         ptrdiff_t stride,
                                         const uint8_t* above,
                                         const uint8_t* left);

void vpx_dc_top_predictor_16x16_c(uint8_t* dst,
                                  ptrdiff_t stride,
                                  const uint8_t* above,
                                  const uint8_t* left);
void vpx_dc_top_predictor_16x16_neon(uint8_t* dst,
                                     ptrdiff_t stride,
                                     const uint8_t* above,
                                     const uint8_t* left);
RTCD_EXTERN void (*vpx_dc_top_predictor_16x16)(uint8_t* dst,
                                               ptrdiff_t stride,
                                               const uint8_t* above,
                                               const uint8_t* left);

void vpx_dc_top_predictor_32x32_c(uint8_t* dst,
                                  ptrdiff_t stride,
                                  const uint8_t* above,
                                  const uint8_t* left);
void vpx_dc_top_predictor_32x32_neon(uint8_t* dst,
                                     ptrdiff_t stride,
                                     const uint8_t* above,
                                     const uint8_t* left);
RTCD_EXTERN void (*vpx_dc_top_predictor_32x32)(uint8_t* dst,
                                               ptrdiff_t stride,
                                               const uint8_t* above,
                                               const uint8_t* left);

void vpx_dc_top_predictor_4x4_c(uint8_t* dst,
                                ptrdiff_t stride,
                                const uint8_t* above,
                                const uint8_t* left);
void vpx_dc_top_predictor_4x4_neon(uint8_t* dst,
                                   ptrdiff_t stride,
                                   const uint8_t* above,
                                   const uint8_t* left);
RTCD_EXTERN void (*vpx_dc_top_predictor_4x4)(uint8_t* dst,
                                             ptrdiff_t stride,
                                             const uint8_t* above,
                                             const uint8_t* left);

void vpx_dc_top_predictor_8x8_c(uint8_t* dst,
                                ptrdiff_t stride,
                                const uint8_t* above,
                                const uint8_t* left);
void vpx_dc_top_predictor_8x8_neon(uint8_t* dst,
                                   ptrdiff_t stride,
                                   const uint8_t* above,
                                   const uint8_t* left);
RTCD_EXTERN void (*vpx_dc_top_predictor_8x8)(uint8_t* dst,
                                             ptrdiff_t stride,
                                             const uint8_t* above,
                                             const uint8_t* left);

void vpx_fdct16x16_c(const int16_t* input, tran_low_t* output, int stride);
void vpx_fdct16x16_neon(const int16_t* input, tran_low_t* output, int stride);
RTCD_EXTERN void (*vpx_fdct16x16)(const int16_t* input,
                                  tran_low_t* output,
                                  int stride);

void vpx_fdct16x16_1_c(const int16_t* input, tran_low_t* output, int stride);
void vpx_fdct16x16_1_neon(const int16_t* input, tran_low_t* output, int stride);
RTCD_EXTERN void (*vpx_fdct16x16_1)(const int16_t* input,
                                    tran_low_t* output,
                                    int stride);

void vpx_fdct32x32_c(const int16_t* input, tran_low_t* output, int stride);
void vpx_fdct32x32_neon(const int16_t* input, tran_low_t* output, int stride);
RTCD_EXTERN void (*vpx_fdct32x32)(const int16_t* input,
                                  tran_low_t* output,
                                  int stride);

void vpx_fdct32x32_1_c(const int16_t* input, tran_low_t* output, int stride);
void vpx_fdct32x32_1_neon(const int16_t* input, tran_low_t* output, int stride);
RTCD_EXTERN void (*vpx_fdct32x32_1)(const int16_t* input,
                                    tran_low_t* output,
                                    int stride);

void vpx_fdct32x32_rd_c(const int16_t* input, tran_low_t* output, int stride);
void vpx_fdct32x32_rd_neon(const int16_t* input,
                           tran_low_t* output,
                           int stride);
RTCD_EXTERN void (*vpx_fdct32x32_rd)(const int16_t* input,
                                     tran_low_t* output,
                                     int stride);

void vpx_fdct4x4_c(const int16_t* input, tran_low_t* output, int stride);
void vpx_fdct4x4_neon(const int16_t* input, tran_low_t* output, int stride);
RTCD_EXTERN void (*vpx_fdct4x4)(const int16_t* input,
                                tran_low_t* output,
                                int stride);

void vpx_fdct4x4_1_c(const int16_t* input, tran_low_t* output, int stride);
void vpx_fdct4x4_1_neon(const int16_t* input, tran_low_t* output, int stride);
RTCD_EXTERN void (*vpx_fdct4x4_1)(const int16_t* input,
                                  tran_low_t* output,
                                  int stride);

void vpx_fdct8x8_c(const int16_t* input, tran_low_t* output, int stride);
void vpx_fdct8x8_neon(const int16_t* input, tran_low_t* output, int stride);
RTCD_EXTERN void (*vpx_fdct8x8)(const int16_t* input,
                                tran_low_t* output,
                                int stride);

void vpx_fdct8x8_1_c(const int16_t* input, tran_low_t* output, int stride);
void vpx_fdct8x8_1_neon(const int16_t* input, tran_low_t* output, int stride);
RTCD_EXTERN void (*vpx_fdct8x8_1)(const int16_t* input,
                                  tran_low_t* output,
                                  int stride);

void vpx_get16x16var_c(const uint8_t* src_ptr,
                       int src_stride,
                       const uint8_t* ref_ptr,
                       int ref_stride,
                       unsigned int* sse,
                       int* sum);
void vpx_get16x16var_neon(const uint8_t* src_ptr,
                          int src_stride,
                          const uint8_t* ref_ptr,
                          int ref_stride,
                          unsigned int* sse,
                          int* sum);
RTCD_EXTERN void (*vpx_get16x16var)(const uint8_t* src_ptr,
                                    int src_stride,
                                    const uint8_t* ref_ptr,
                                    int ref_stride,
                                    unsigned int* sse,
                                    int* sum);

unsigned int vpx_get4x4sse_cs_c(const unsigned char* src_ptr,
                                int src_stride,
                                const unsigned char* ref_ptr,
                                int ref_stride);
unsigned int vpx_get4x4sse_cs_neon(const unsigned char* src_ptr,
                                   int src_stride,
                                   const unsigned char* ref_ptr,
                                   int ref_stride);
RTCD_EXTERN unsigned int (*vpx_get4x4sse_cs)(const unsigned char* src_ptr,
                                             int src_stride,
                                             const unsigned char* ref_ptr,
                                             int ref_stride);

void vpx_get8x8var_c(const uint8_t* src_ptr,
                     int src_stride,
                     const uint8_t* ref_ptr,
                     int ref_stride,
                     unsigned int* sse,
                     int* sum);
void vpx_get8x8var_neon(const uint8_t* src_ptr,
                        int src_stride,
                        const uint8_t* ref_ptr,
                        int ref_stride,
                        unsigned int* sse,
                        int* sum);
RTCD_EXTERN void (*vpx_get8x8var)(const uint8_t* src_ptr,
                                  int src_stride,
                                  const uint8_t* ref_ptr,
                                  int ref_stride,
                                  unsigned int* sse,
                                  int* sum);

unsigned int vpx_get_mb_ss_c(const int16_t*);
#define vpx_get_mb_ss vpx_get_mb_ss_c

void vpx_h_predictor_16x16_c(uint8_t* dst,
                             ptrdiff_t stride,
                             const uint8_t* above,
                             const uint8_t* left);
void vpx_h_predictor_16x16_neon(uint8_t* dst,
                                ptrdiff_t stride,
                                const uint8_t* above,
                                const uint8_t* left);
RTCD_EXTERN void (*vpx_h_predictor_16x16)(uint8_t* dst,
                                          ptrdiff_t stride,
                                          const uint8_t* above,
                                          const uint8_t* left);

void vpx_h_predictor_32x32_c(uint8_t* dst,
                             ptrdiff_t stride,
                             const uint8_t* above,
                             const uint8_t* left);
void vpx_h_predictor_32x32_neon(uint8_t* dst,
                                ptrdiff_t stride,
                                const uint8_t* above,
                                const uint8_t* left);
RTCD_EXTERN void (*vpx_h_predictor_32x32)(uint8_t* dst,
                                          ptrdiff_t stride,
                                          const uint8_t* above,
                                          const uint8_t* left);

void vpx_h_predictor_4x4_c(uint8_t* dst,
                           ptrdiff_t stride,
                           const uint8_t* above,
                           const uint8_t* left);
void vpx_h_predictor_4x4_neon(uint8_t* dst,
                              ptrdiff_t stride,
                              const uint8_t* above,
                              const uint8_t* left);
RTCD_EXTERN void (*vpx_h_predictor_4x4)(uint8_t* dst,
                                        ptrdiff_t stride,
                                        const uint8_t* above,
                                        const uint8_t* left);

void vpx_h_predictor_8x8_c(uint8_t* dst,
                           ptrdiff_t stride,
                           const uint8_t* above,
                           const uint8_t* left);
void vpx_h_predictor_8x8_neon(uint8_t* dst,
                              ptrdiff_t stride,
                              const uint8_t* above,
                              const uint8_t* left);
RTCD_EXTERN void (*vpx_h_predictor_8x8)(uint8_t* dst,
                                        ptrdiff_t stride,
                                        const uint8_t* above,
                                        const uint8_t* left);

void vpx_hadamard_16x16_c(const int16_t* src_diff,
                          ptrdiff_t src_stride,
                          int16_t* coeff);
void vpx_hadamard_16x16_neon(const int16_t* src_diff,
                             ptrdiff_t src_stride,
                             int16_t* coeff);
RTCD_EXTERN void (*vpx_hadamard_16x16)(const int16_t* src_diff,
                                       ptrdiff_t src_stride,
                                       int16_t* coeff);

void vpx_hadamard_32x32_c(const int16_t* src_diff,
                          ptrdiff_t src_stride,
                          int16_t* coeff);
#define vpx_hadamard_32x32 vpx_hadamard_32x32_c

void vpx_hadamard_8x8_c(const int16_t* src_diff,
                        ptrdiff_t src_stride,
                        int16_t* coeff);
void vpx_hadamard_8x8_neon(const int16_t* src_diff,
                           ptrdiff_t src_stride,
                           int16_t* coeff);
RTCD_EXTERN void (*vpx_hadamard_8x8)(const int16_t* src_diff,
                                     ptrdiff_t src_stride,
                                     int16_t* coeff);

void vpx_he_predictor_4x4_c(uint8_t* dst,
                            ptrdiff_t stride,
                            const uint8_t* above,
                            const uint8_t* left);
#define vpx_he_predictor_4x4 vpx_he_predictor_4x4_c

void vpx_idct16x16_10_add_c(const tran_low_t* input, uint8_t* dest, int stride);
void vpx_idct16x16_10_add_neon(const tran_low_t* input,
                               uint8_t* dest,
                               int stride);
RTCD_EXTERN void (*vpx_idct16x16_10_add)(const tran_low_t* input,
                                         uint8_t* dest,
                                         int stride);

void vpx_idct16x16_1_add_c(const tran_low_t* input, uint8_t* dest, int stride);
void vpx_idct16x16_1_add_neon(const tran_low_t* input,
                              uint8_t* dest,
                              int stride);
RTCD_EXTERN void (*vpx_idct16x16_1_add)(const tran_low_t* input,
                                        uint8_t* dest,
                                        int stride);

void vpx_idct16x16_256_add_c(const tran_low_t* input,
                             uint8_t* dest,
                             int stride);
void vpx_idct16x16_256_add_neon(const tran_low_t* input,
                                uint8_t* dest,
                                int stride);
RTCD_EXTERN void (*vpx_idct16x16_256_add)(const tran_low_t* input,
                                          uint8_t* dest,
                                          int stride);

void vpx_idct16x16_38_add_c(const tran_low_t* input, uint8_t* dest, int stride);
void vpx_idct16x16_38_add_neon(const tran_low_t* input,
                               uint8_t* dest,
                               int stride);
RTCD_EXTERN void (*vpx_idct16x16_38_add)(const tran_low_t* input,
                                         uint8_t* dest,
                                         int stride);

void vpx_idct32x32_1024_add_c(const tran_low_t* input,
                              uint8_t* dest,
                              int stride);
void vpx_idct32x32_1024_add_neon(const tran_low_t* input,
                                 uint8_t* dest,
                                 int stride);
RTCD_EXTERN void (*vpx_idct32x32_1024_add)(const tran_low_t* input,
                                           uint8_t* dest,
                                           int stride);

void vpx_idct32x32_135_add_c(const tran_low_t* input,
                             uint8_t* dest,
                             int stride);
void vpx_idct32x32_135_add_neon(const tran_low_t* input,
                                uint8_t* dest,
                                int stride);
RTCD_EXTERN void (*vpx_idct32x32_135_add)(const tran_low_t* input,
                                          uint8_t* dest,
                                          int stride);

void vpx_idct32x32_1_add_c(const tran_low_t* input, uint8_t* dest, int stride);
void vpx_idct32x32_1_add_neon(const tran_low_t* input,
                              uint8_t* dest,
                              int stride);
RTCD_EXTERN void (*vpx_idct32x32_1_add)(const tran_low_t* input,
                                        uint8_t* dest,
                                        int stride);

void vpx_idct32x32_34_add_c(const tran_low_t* input, uint8_t* dest, int stride);
void vpx_idct32x32_34_add_neon(const tran_low_t* input,
                               uint8_t* dest,
                               int stride);
RTCD_EXTERN void (*vpx_idct32x32_34_add)(const tran_low_t* input,
                                         uint8_t* dest,
                                         int stride);

void vpx_idct4x4_16_add_c(const tran_low_t* input, uint8_t* dest, int stride);
void vpx_idct4x4_16_add_neon(const tran_low_t* input,
                             uint8_t* dest,
                             int stride);
RTCD_EXTERN void (*vpx_idct4x4_16_add)(const tran_low_t* input,
                                       uint8_t* dest,
                                       int stride);

void vpx_idct4x4_1_add_c(const tran_low_t* input, uint8_t* dest, int stride);
void vpx_idct4x4_1_add_neon(const tran_low_t* input, uint8_t* dest, int stride);
RTCD_EXTERN void (*vpx_idct4x4_1_add)(const tran_low_t* input,
                                      uint8_t* dest,
                                      int stride);

void vpx_idct8x8_12_add_c(const tran_low_t* input, uint8_t* dest, int stride);
void vpx_idct8x8_12_add_neon(const tran_low_t* input,
                             uint8_t* dest,
                             int stride);
RTCD_EXTERN void (*vpx_idct8x8_12_add)(const tran_low_t* input,
                                       uint8_t* dest,
                                       int stride);

void vpx_idct8x8_1_add_c(const tran_low_t* input, uint8_t* dest, int stride);
void vpx_idct8x8_1_add_neon(const tran_low_t* input, uint8_t* dest, int stride);
RTCD_EXTERN void (*vpx_idct8x8_1_add)(const tran_low_t* input,
                                      uint8_t* dest,
                                      int stride);

void vpx_idct8x8_64_add_c(const tran_low_t* input, uint8_t* dest, int stride);
void vpx_idct8x8_64_add_neon(const tran_low_t* input,
                             uint8_t* dest,
                             int stride);
RTCD_EXTERN void (*vpx_idct8x8_64_add)(const tran_low_t* input,
                                       uint8_t* dest,
                                       int stride);

int16_t vpx_int_pro_col_c(const uint8_t* ref, const int width);
int16_t vpx_int_pro_col_neon(const uint8_t* ref, const int width);
RTCD_EXTERN int16_t (*vpx_int_pro_col)(const uint8_t* ref, const int width);

void vpx_int_pro_row_c(int16_t* hbuf,
                       const uint8_t* ref,
                       const int ref_stride,
                       const int height);
void vpx_int_pro_row_neon(int16_t* hbuf,
                          const uint8_t* ref,
                          const int ref_stride,
                          const int height);
RTCD_EXTERN void (*vpx_int_pro_row)(int16_t* hbuf,
                                    const uint8_t* ref,
                                    const int ref_stride,
                                    const int height);

void vpx_iwht4x4_16_add_c(const tran_low_t* input, uint8_t* dest, int stride);
#define vpx_iwht4x4_16_add vpx_iwht4x4_16_add_c

void vpx_iwht4x4_1_add_c(const tran_low_t* input, uint8_t* dest, int stride);
#define vpx_iwht4x4_1_add vpx_iwht4x4_1_add_c

void vpx_lpf_horizontal_16_c(uint8_t* s,
                             int pitch,
                             const uint8_t* blimit,
                             const uint8_t* limit,
                             const uint8_t* thresh);
void vpx_lpf_horizontal_16_neon(uint8_t* s,
                                int pitch,
                                const uint8_t* blimit,
                                const uint8_t* limit,
                                const uint8_t* thresh);
RTCD_EXTERN void (*vpx_lpf_horizontal_16)(uint8_t* s,
                                          int pitch,
                                          const uint8_t* blimit,
                                          const uint8_t* limit,
                                          const uint8_t* thresh);

void vpx_lpf_horizontal_16_dual_c(uint8_t* s,
                                  int pitch,
                                  const uint8_t* blimit,
                                  const uint8_t* limit,
                                  const uint8_t* thresh);
void vpx_lpf_horizontal_16_dual_neon(uint8_t* s,
                                     int pitch,
                                     const uint8_t* blimit,
                                     const uint8_t* limit,
                                     const uint8_t* thresh);
RTCD_EXTERN void (*vpx_lpf_horizontal_16_dual)(uint8_t* s,
                                               int pitch,
                                               const uint8_t* blimit,
                                               const uint8_t* limit,
                                               const uint8_t* thresh);

void vpx_lpf_horizontal_4_c(uint8_t* s,
                            int pitch,
                            const uint8_t* blimit,
                            const uint8_t* limit,
                            const uint8_t* thresh);
void vpx_lpf_horizontal_4_neon(uint8_t* s,
                               int pitch,
                               const uint8_t* blimit,
                               const uint8_t* limit,
                               const uint8_t* thresh);
RTCD_EXTERN void (*vpx_lpf_horizontal_4)(uint8_t* s,
                                         int pitch,
                                         const uint8_t* blimit,
                                         const uint8_t* limit,
                                         const uint8_t* thresh);

void vpx_lpf_horizontal_4_dual_c(uint8_t* s,
                                 int pitch,
                                 const uint8_t* blimit0,
                                 const uint8_t* limit0,
                                 const uint8_t* thresh0,
                                 const uint8_t* blimit1,
                                 const uint8_t* limit1,
                                 const uint8_t* thresh1);
void vpx_lpf_horizontal_4_dual_neon(uint8_t* s,
                                    int pitch,
                                    const uint8_t* blimit0,
                                    const uint8_t* limit0,
                                    const uint8_t* thresh0,
                                    const uint8_t* blimit1,
                                    const uint8_t* limit1,
                                    const uint8_t* thresh1);
RTCD_EXTERN void (*vpx_lpf_horizontal_4_dual)(uint8_t* s,
                                              int pitch,
                                              const uint8_t* blimit0,
                                              const uint8_t* limit0,
                                              const uint8_t* thresh0,
                                              const uint8_t* blimit1,
                                              const uint8_t* limit1,
                                              const uint8_t* thresh1);

void vpx_lpf_horizontal_8_c(uint8_t* s,
                            int pitch,
                            const uint8_t* blimit,
                            const uint8_t* limit,
                            const uint8_t* thresh);
void vpx_lpf_horizontal_8_neon(uint8_t* s,
                               int pitch,
                               const uint8_t* blimit,
                               const uint8_t* limit,
                               const uint8_t* thresh);
RTCD_EXTERN void (*vpx_lpf_horizontal_8)(uint8_t* s,
                                         int pitch,
                                         const uint8_t* blimit,
                                         const uint8_t* limit,
                                         const uint8_t* thresh);

void vpx_lpf_horizontal_8_dual_c(uint8_t* s,
                                 int pitch,
                                 const uint8_t* blimit0,
                                 const uint8_t* limit0,
                                 const uint8_t* thresh0,
                                 const uint8_t* blimit1,
                                 const uint8_t* limit1,
                                 const uint8_t* thresh1);
void vpx_lpf_horizontal_8_dual_neon(uint8_t* s,
                                    int pitch,
                                    const uint8_t* blimit0,
                                    const uint8_t* limit0,
                                    const uint8_t* thresh0,
                                    const uint8_t* blimit1,
                                    const uint8_t* limit1,
                                    const uint8_t* thresh1);
RTCD_EXTERN void (*vpx_lpf_horizontal_8_dual)(uint8_t* s,
                                              int pitch,
                                              const uint8_t* blimit0,
                                              const uint8_t* limit0,
                                              const uint8_t* thresh0,
                                              const uint8_t* blimit1,
                                              const uint8_t* limit1,
                                              const uint8_t* thresh1);

void vpx_lpf_vertical_16_c(uint8_t* s,
                           int pitch,
                           const uint8_t* blimit,
                           const uint8_t* limit,
                           const uint8_t* thresh);
void vpx_lpf_vertical_16_neon(uint8_t* s,
                              int pitch,
                              const uint8_t* blimit,
                              const uint8_t* limit,
                              const uint8_t* thresh);
RTCD_EXTERN void (*vpx_lpf_vertical_16)(uint8_t* s,
                                        int pitch,
                                        const uint8_t* blimit,
                                        const uint8_t* limit,
                                        const uint8_t* thresh);

void vpx_lpf_vertical_16_dual_c(uint8_t* s,
                                int pitch,
                                const uint8_t* blimit,
                                const uint8_t* limit,
                                const uint8_t* thresh);
void vpx_lpf_vertical_16_dual_neon(uint8_t* s,
                                   int pitch,
                                   const uint8_t* blimit,
                                   const uint8_t* limit,
                                   const uint8_t* thresh);
RTCD_EXTERN void (*vpx_lpf_vertical_16_dual)(uint8_t* s,
                                             int pitch,
                                             const uint8_t* blimit,
                                             const uint8_t* limit,
                                             const uint8_t* thresh);

void vpx_lpf_vertical_4_c(uint8_t* s,
                          int pitch,
                          const uint8_t* blimit,
                          const uint8_t* limit,
                          const uint8_t* thresh);
void vpx_lpf_vertical_4_neon(uint8_t* s,
                             int pitch,
                             const uint8_t* blimit,
                             const uint8_t* limit,
                             const uint8_t* thresh);
RTCD_EXTERN void (*vpx_lpf_vertical_4)(uint8_t* s,
                                       int pitch,
                                       const uint8_t* blimit,
                                       const uint8_t* limit,
                                       const uint8_t* thresh);

void vpx_lpf_vertical_4_dual_c(uint8_t* s,
                               int pitch,
                               const uint8_t* blimit0,
                               const uint8_t* limit0,
                               const uint8_t* thresh0,
                               const uint8_t* blimit1,
                               const uint8_t* limit1,
                               const uint8_t* thresh1);
void vpx_lpf_vertical_4_dual_neon(uint8_t* s,
                                  int pitch,
                                  const uint8_t* blimit0,
                                  const uint8_t* limit0,
                                  const uint8_t* thresh0,
                                  const uint8_t* blimit1,
                                  const uint8_t* limit1,
                                  const uint8_t* thresh1);
RTCD_EXTERN void (*vpx_lpf_vertical_4_dual)(uint8_t* s,
                                            int pitch,
                                            const uint8_t* blimit0,
                                            const uint8_t* limit0,
                                            const uint8_t* thresh0,
                                            const uint8_t* blimit1,
                                            const uint8_t* limit1,
                                            const uint8_t* thresh1);

void vpx_lpf_vertical_8_c(uint8_t* s,
                          int pitch,
                          const uint8_t* blimit,
                          const uint8_t* limit,
                          const uint8_t* thresh);
void vpx_lpf_vertical_8_neon(uint8_t* s,
                             int pitch,
                             const uint8_t* blimit,
                             const uint8_t* limit,
                             const uint8_t* thresh);
RTCD_EXTERN void (*vpx_lpf_vertical_8)(uint8_t* s,
                                       int pitch,
                                       const uint8_t* blimit,
                                       const uint8_t* limit,
                                       const uint8_t* thresh);

void vpx_lpf_vertical_8_dual_c(uint8_t* s,
                               int pitch,
                               const uint8_t* blimit0,
                               const uint8_t* limit0,
                               const uint8_t* thresh0,
                               const uint8_t* blimit1,
                               const uint8_t* limit1,
                               const uint8_t* thresh1);
void vpx_lpf_vertical_8_dual_neon(uint8_t* s,
                                  int pitch,
                                  const uint8_t* blimit0,
                                  const uint8_t* limit0,
                                  const uint8_t* thresh0,
                                  const uint8_t* blimit1,
                                  const uint8_t* limit1,
                                  const uint8_t* thresh1);
RTCD_EXTERN void (*vpx_lpf_vertical_8_dual)(uint8_t* s,
                                            int pitch,
                                            const uint8_t* blimit0,
                                            const uint8_t* limit0,
                                            const uint8_t* thresh0,
                                            const uint8_t* blimit1,
                                            const uint8_t* limit1,
                                            const uint8_t* thresh1);

void vpx_mbpost_proc_across_ip_c(unsigned char* src,
                                 int pitch,
                                 int rows,
                                 int cols,
                                 int flimit);
void vpx_mbpost_proc_across_ip_neon(unsigned char* src,
                                    int pitch,
                                    int rows,
                                    int cols,
                                    int flimit);
RTCD_EXTERN void (*vpx_mbpost_proc_across_ip)(unsigned char* src,
                                              int pitch,
                                              int rows,
                                              int cols,
                                              int flimit);

void vpx_mbpost_proc_down_c(unsigned char* dst,
                            int pitch,
                            int rows,
                            int cols,
                            int flimit);
void vpx_mbpost_proc_down_neon(unsigned char* dst,
                               int pitch,
                               int rows,
                               int cols,
                               int flimit);
RTCD_EXTERN void (*vpx_mbpost_proc_down)(unsigned char* dst,
                                         int pitch,
                                         int rows,
                                         int cols,
                                         int flimit);

void vpx_minmax_8x8_c(const uint8_t* s,
                      int p,
                      const uint8_t* d,
                      int dp,
                      int* min,
                      int* max);
void vpx_minmax_8x8_neon(const uint8_t* s,
                         int p,
                         const uint8_t* d,
                         int dp,
                         int* min,
                         int* max);
RTCD_EXTERN void (*vpx_minmax_8x8)(const uint8_t* s,
                                   int p,
                                   const uint8_t* d,
                                   int dp,
                                   int* min,
                                   int* max);

unsigned int vpx_mse16x16_c(const uint8_t* src_ptr,
                            int src_stride,
                            const uint8_t* ref_ptr,
                            int ref_stride,
                            unsigned int* sse);
unsigned int vpx_mse16x16_neon(const uint8_t* src_ptr,
                               int src_stride,
                               const uint8_t* ref_ptr,
                               int ref_stride,
                               unsigned int* sse);
RTCD_EXTERN unsigned int (*vpx_mse16x16)(const uint8_t* src_ptr,
                                         int src_stride,
                                         const uint8_t* ref_ptr,
                                         int ref_stride,
                                         unsigned int* sse);

unsigned int vpx_mse16x8_c(const uint8_t* src_ptr,
                           int src_stride,
                           const uint8_t* ref_ptr,
                           int ref_stride,
                           unsigned int* sse);
#define vpx_mse16x8 vpx_mse16x8_c

unsigned int vpx_mse8x16_c(const uint8_t* src_ptr,
                           int src_stride,
                           const uint8_t* ref_ptr,
                           int ref_stride,
                           unsigned int* sse);
#define vpx_mse8x16 vpx_mse8x16_c

unsigned int vpx_mse8x8_c(const uint8_t* src_ptr,
                          int src_stride,
                          const uint8_t* ref_ptr,
                          int ref_stride,
                          unsigned int* sse);
#define vpx_mse8x8 vpx_mse8x8_c

void vpx_plane_add_noise_c(uint8_t* start,
                           const int8_t* noise,
                           int blackclamp,
                           int whiteclamp,
                           int width,
                           int height,
                           int pitch);
#define vpx_plane_add_noise vpx_plane_add_noise_c

void vpx_post_proc_down_and_across_mb_row_c(unsigned char* src,
                                            unsigned char* dst,
                                            int src_pitch,
                                            int dst_pitch,
                                            int cols,
                                            unsigned char* flimits,
                                            int size);
void vpx_post_proc_down_and_across_mb_row_neon(unsigned char* src,
                                               unsigned char* dst,
                                               int src_pitch,
                                               int dst_pitch,
                                               int cols,
                                               unsigned char* flimits,
                                               int size);
RTCD_EXTERN void (*vpx_post_proc_down_and_across_mb_row)(unsigned char* src,
                                                         unsigned char* dst,
                                                         int src_pitch,
                                                         int dst_pitch,
                                                         int cols,
                                                         unsigned char* flimits,
                                                         int size);

void vpx_quantize_b_c(const tran_low_t* coeff_ptr,
                      intptr_t n_coeffs,
                      int skip_block,
                      const int16_t* zbin_ptr,
                      const int16_t* round_ptr,
                      const int16_t* quant_ptr,
                      const int16_t* quant_shift_ptr,
                      tran_low_t* qcoeff_ptr,
                      tran_low_t* dqcoeff_ptr,
                      const int16_t* dequant_ptr,
                      uint16_t* eob_ptr,
                      const int16_t* scan,
                      const int16_t* iscan);
void vpx_quantize_b_neon(const tran_low_t* coeff_ptr,
                         intptr_t n_coeffs,
                         int skip_block,
                         const int16_t* zbin_ptr,
                         const int16_t* round_ptr,
                         const int16_t* quant_ptr,
                         const int16_t* quant_shift_ptr,
                         tran_low_t* qcoeff_ptr,
                         tran_low_t* dqcoeff_ptr,
                         const int16_t* dequant_ptr,
                         uint16_t* eob_ptr,
                         const int16_t* scan,
                         const int16_t* iscan);
RTCD_EXTERN void (*vpx_quantize_b)(const tran_low_t* coeff_ptr,
                                   intptr_t n_coeffs,
                                   int skip_block,
                                   const int16_t* zbin_ptr,
                                   const int16_t* round_ptr,
                                   const int16_t* quant_ptr,
                                   const int16_t* quant_shift_ptr,
                                   tran_low_t* qcoeff_ptr,
                                   tran_low_t* dqcoeff_ptr,
                                   const int16_t* dequant_ptr,
                                   uint16_t* eob_ptr,
                                   const int16_t* scan,
                                   const int16_t* iscan);

void vpx_quantize_b_32x32_c(const tran_low_t* coeff_ptr,
                            intptr_t n_coeffs,
                            int skip_block,
                            const int16_t* zbin_ptr,
                            const int16_t* round_ptr,
                            const int16_t* quant_ptr,
                            const int16_t* quant_shift_ptr,
                            tran_low_t* qcoeff_ptr,
                            tran_low_t* dqcoeff_ptr,
                            const int16_t* dequant_ptr,
                            uint16_t* eob_ptr,
                            const int16_t* scan,
                            const int16_t* iscan);
void vpx_quantize_b_32x32_neon(const tran_low_t* coeff_ptr,
                               intptr_t n_coeffs,
                               int skip_block,
                               const int16_t* zbin_ptr,
                               const int16_t* round_ptr,
                               const int16_t* quant_ptr,
                               const int16_t* quant_shift_ptr,
                               tran_low_t* qcoeff_ptr,
                               tran_low_t* dqcoeff_ptr,
                               const int16_t* dequant_ptr,
                               uint16_t* eob_ptr,
                               const int16_t* scan,
                               const int16_t* iscan);
RTCD_EXTERN void (*vpx_quantize_b_32x32)(const tran_low_t* coeff_ptr,
                                         intptr_t n_coeffs,
                                         int skip_block,
                                         const int16_t* zbin_ptr,
                                         const int16_t* round_ptr,
                                         const int16_t* quant_ptr,
                                         const int16_t* quant_shift_ptr,
                                         tran_low_t* qcoeff_ptr,
                                         tran_low_t* dqcoeff_ptr,
                                         const int16_t* dequant_ptr,
                                         uint16_t* eob_ptr,
                                         const int16_t* scan,
                                         const int16_t* iscan);

unsigned int vpx_sad16x16_c(const uint8_t* src_ptr,
                            int src_stride,
                            const uint8_t* ref_ptr,
                            int ref_stride);
unsigned int vpx_sad16x16_neon(const uint8_t* src_ptr,
                               int src_stride,
                               const uint8_t* ref_ptr,
                               int ref_stride);
RTCD_EXTERN unsigned int (*vpx_sad16x16)(const uint8_t* src_ptr,
                                         int src_stride,
                                         const uint8_t* ref_ptr,
                                         int ref_stride);

unsigned int vpx_sad16x16_avg_c(const uint8_t* src_ptr,
                                int src_stride,
                                const uint8_t* ref_ptr,
                                int ref_stride,
                                const uint8_t* second_pred);
unsigned int vpx_sad16x16_avg_neon(const uint8_t* src_ptr,
                                   int src_stride,
                                   const uint8_t* ref_ptr,
                                   int ref_stride,
                                   const uint8_t* second_pred);
RTCD_EXTERN unsigned int (*vpx_sad16x16_avg)(const uint8_t* src_ptr,
                                             int src_stride,
                                             const uint8_t* ref_ptr,
                                             int ref_stride,
                                             const uint8_t* second_pred);

void vpx_sad16x16x3_c(const uint8_t* src_ptr,
                      int src_stride,
                      const uint8_t* ref_ptr,
                      int ref_stride,
                      uint32_t* sad_array);
#define vpx_sad16x16x3 vpx_sad16x16x3_c

void vpx_sad16x16x4d_c(const uint8_t* src_ptr,
                       int src_stride,
                       const uint8_t* const ref_array[],
                       int ref_stride,
                       uint32_t* sad_array);
void vpx_sad16x16x4d_neon(const uint8_t* src_ptr,
                          int src_stride,
                          const uint8_t* const ref_array[],
                          int ref_stride,
                          uint32_t* sad_array);
RTCD_EXTERN void (*vpx_sad16x16x4d)(const uint8_t* src_ptr,
                                    int src_stride,
                                    const uint8_t* const ref_array[],
                                    int ref_stride,
                                    uint32_t* sad_array);

void vpx_sad16x16x8_c(const uint8_t* src_ptr,
                      int src_stride,
                      const uint8_t* ref_ptr,
                      int ref_stride,
                      uint32_t* sad_array);
#define vpx_sad16x16x8 vpx_sad16x16x8_c

unsigned int vpx_sad16x32_c(const uint8_t* src_ptr,
                            int src_stride,
                            const uint8_t* ref_ptr,
                            int ref_stride);
unsigned int vpx_sad16x32_neon(const uint8_t* src_ptr,
                               int src_stride,
                               const uint8_t* ref_ptr,
                               int ref_stride);
RTCD_EXTERN unsigned int (*vpx_sad16x32)(const uint8_t* src_ptr,
                                         int src_stride,
                                         const uint8_t* ref_ptr,
                                         int ref_stride);

unsigned int vpx_sad16x32_avg_c(const uint8_t* src_ptr,
                                int src_stride,
                                const uint8_t* ref_ptr,
                                int ref_stride,
                                const uint8_t* second_pred);
unsigned int vpx_sad16x32_avg_neon(const uint8_t* src_ptr,
                                   int src_stride,
                                   const uint8_t* ref_ptr,
                                   int ref_stride,
                                   const uint8_t* second_pred);
RTCD_EXTERN unsigned int (*vpx_sad16x32_avg)(const uint8_t* src_ptr,
                                             int src_stride,
                                             const uint8_t* ref_ptr,
                                             int ref_stride,
                                             const uint8_t* second_pred);

void vpx_sad16x32x4d_c(const uint8_t* src_ptr,
                       int src_stride,
                       const uint8_t* const ref_array[],
                       int ref_stride,
                       uint32_t* sad_array);
void vpx_sad16x32x4d_neon(const uint8_t* src_ptr,
                          int src_stride,
                          const uint8_t* const ref_array[],
                          int ref_stride,
                          uint32_t* sad_array);
RTCD_EXTERN void (*vpx_sad16x32x4d)(const uint8_t* src_ptr,
                                    int src_stride,
                                    const uint8_t* const ref_array[],
                                    int ref_stride,
                                    uint32_t* sad_array);

unsigned int vpx_sad16x8_c(const uint8_t* src_ptr,
                           int src_stride,
                           const uint8_t* ref_ptr,
                           int ref_stride);
unsigned int vpx_sad16x8_neon(const uint8_t* src_ptr,
                              int src_stride,
                              const uint8_t* ref_ptr,
                              int ref_stride);
RTCD_EXTERN unsigned int (*vpx_sad16x8)(const uint8_t* src_ptr,
                                        int src_stride,
                                        const uint8_t* ref_ptr,
                                        int ref_stride);

unsigned int vpx_sad16x8_avg_c(const uint8_t* src_ptr,
                               int src_stride,
                               const uint8_t* ref_ptr,
                               int ref_stride,
                               const uint8_t* second_pred);
unsigned int vpx_sad16x8_avg_neon(const uint8_t* src_ptr,
                                  int src_stride,
                                  const uint8_t* ref_ptr,
                                  int ref_stride,
                                  const uint8_t* second_pred);
RTCD_EXTERN unsigned int (*vpx_sad16x8_avg)(const uint8_t* src_ptr,
                                            int src_stride,
                                            const uint8_t* ref_ptr,
                                            int ref_stride,
                                            const uint8_t* second_pred);

void vpx_sad16x8x3_c(const uint8_t* src_ptr,
                     int src_stride,
                     const uint8_t* ref_ptr,
                     int ref_stride,
                     uint32_t* sad_array);
#define vpx_sad16x8x3 vpx_sad16x8x3_c

void vpx_sad16x8x4d_c(const uint8_t* src_ptr,
                      int src_stride,
                      const uint8_t* const ref_array[],
                      int ref_stride,
                      uint32_t* sad_array);
void vpx_sad16x8x4d_neon(const uint8_t* src_ptr,
                         int src_stride,
                         const uint8_t* const ref_array[],
                         int ref_stride,
                         uint32_t* sad_array);
RTCD_EXTERN void (*vpx_sad16x8x4d)(const uint8_t* src_ptr,
                                   int src_stride,
                                   const uint8_t* const ref_array[],
                                   int ref_stride,
                                   uint32_t* sad_array);

void vpx_sad16x8x8_c(const uint8_t* src_ptr,
                     int src_stride,
                     const uint8_t* ref_ptr,
                     int ref_stride,
                     uint32_t* sad_array);
#define vpx_sad16x8x8 vpx_sad16x8x8_c

unsigned int vpx_sad32x16_c(const uint8_t* src_ptr,
                            int src_stride,
                            const uint8_t* ref_ptr,
                            int ref_stride);
unsigned int vpx_sad32x16_neon(const uint8_t* src_ptr,
                               int src_stride,
                               const uint8_t* ref_ptr,
                               int ref_stride);
RTCD_EXTERN unsigned int (*vpx_sad32x16)(const uint8_t* src_ptr,
                                         int src_stride,
                                         const uint8_t* ref_ptr,
                                         int ref_stride);

unsigned int vpx_sad32x16_avg_c(const uint8_t* src_ptr,
                                int src_stride,
                                const uint8_t* ref_ptr,
                                int ref_stride,
                                const uint8_t* second_pred);
unsigned int vpx_sad32x16_avg_neon(const uint8_t* src_ptr,
                                   int src_stride,
                                   const uint8_t* ref_ptr,
                                   int ref_stride,
                                   const uint8_t* second_pred);
RTCD_EXTERN unsigned int (*vpx_sad32x16_avg)(const uint8_t* src_ptr,
                                             int src_stride,
                                             const uint8_t* ref_ptr,
                                             int ref_stride,
                                             const uint8_t* second_pred);

void vpx_sad32x16x4d_c(const uint8_t* src_ptr,
                       int src_stride,
                       const uint8_t* const ref_array[],
                       int ref_stride,
                       uint32_t* sad_array);
void vpx_sad32x16x4d_neon(const uint8_t* src_ptr,
                          int src_stride,
                          const uint8_t* const ref_array[],
                          int ref_stride,
                          uint32_t* sad_array);
RTCD_EXTERN void (*vpx_sad32x16x4d)(const uint8_t* src_ptr,
                                    int src_stride,
                                    const uint8_t* const ref_array[],
                                    int ref_stride,
                                    uint32_t* sad_array);

unsigned int vpx_sad32x32_c(const uint8_t* src_ptr,
                            int src_stride,
                            const uint8_t* ref_ptr,
                            int ref_stride);
unsigned int vpx_sad32x32_neon(const uint8_t* src_ptr,
                               int src_stride,
                               const uint8_t* ref_ptr,
                               int ref_stride);
RTCD_EXTERN unsigned int (*vpx_sad32x32)(const uint8_t* src_ptr,
                                         int src_stride,
                                         const uint8_t* ref_ptr,
                                         int ref_stride);

unsigned int vpx_sad32x32_avg_c(const uint8_t* src_ptr,
                                int src_stride,
                                const uint8_t* ref_ptr,
                                int ref_stride,
                                const uint8_t* second_pred);
unsigned int vpx_sad32x32_avg_neon(const uint8_t* src_ptr,
                                   int src_stride,
                                   const uint8_t* ref_ptr,
                                   int ref_stride,
                                   const uint8_t* second_pred);
RTCD_EXTERN unsigned int (*vpx_sad32x32_avg)(const uint8_t* src_ptr,
                                             int src_stride,
                                             const uint8_t* ref_ptr,
                                             int ref_stride,
                                             const uint8_t* second_pred);

void vpx_sad32x32x4d_c(const uint8_t* src_ptr,
                       int src_stride,
                       const uint8_t* const ref_array[],
                       int ref_stride,
                       uint32_t* sad_array);
void vpx_sad32x32x4d_neon(const uint8_t* src_ptr,
                          int src_stride,
                          const uint8_t* const ref_array[],
                          int ref_stride,
                          uint32_t* sad_array);
RTCD_EXTERN void (*vpx_sad32x32x4d)(const uint8_t* src_ptr,
                                    int src_stride,
                                    const uint8_t* const ref_array[],
                                    int ref_stride,
                                    uint32_t* sad_array);

void vpx_sad32x32x8_c(const uint8_t* src_ptr,
                      int src_stride,
                      const uint8_t* ref_ptr,
                      int ref_stride,
                      uint32_t* sad_array);
#define vpx_sad32x32x8 vpx_sad32x32x8_c

unsigned int vpx_sad32x64_c(const uint8_t* src_ptr,
                            int src_stride,
                            const uint8_t* ref_ptr,
                            int ref_stride);
unsigned int vpx_sad32x64_neon(const uint8_t* src_ptr,
                               int src_stride,
                               const uint8_t* ref_ptr,
                               int ref_stride);
RTCD_EXTERN unsigned int (*vpx_sad32x64)(const uint8_t* src_ptr,
                                         int src_stride,
                                         const uint8_t* ref_ptr,
                                         int ref_stride);

unsigned int vpx_sad32x64_avg_c(const uint8_t* src_ptr,
                                int src_stride,
                                const uint8_t* ref_ptr,
                                int ref_stride,
                                const uint8_t* second_pred);
unsigned int vpx_sad32x64_avg_neon(const uint8_t* src_ptr,
                                   int src_stride,
                                   const uint8_t* ref_ptr,
                                   int ref_stride,
                                   const uint8_t* second_pred);
RTCD_EXTERN unsigned int (*vpx_sad32x64_avg)(const uint8_t* src_ptr,
                                             int src_stride,
                                             const uint8_t* ref_ptr,
                                             int ref_stride,
                                             const uint8_t* second_pred);

void vpx_sad32x64x4d_c(const uint8_t* src_ptr,
                       int src_stride,
                       const uint8_t* const ref_array[],
                       int ref_stride,
                       uint32_t* sad_array);
void vpx_sad32x64x4d_neon(const uint8_t* src_ptr,
                          int src_stride,
                          const uint8_t* const ref_array[],
                          int ref_stride,
                          uint32_t* sad_array);
RTCD_EXTERN void (*vpx_sad32x64x4d)(const uint8_t* src_ptr,
                                    int src_stride,
                                    const uint8_t* const ref_array[],
                                    int ref_stride,
                                    uint32_t* sad_array);

unsigned int vpx_sad4x4_c(const uint8_t* src_ptr,
                          int src_stride,
                          const uint8_t* ref_ptr,
                          int ref_stride);
unsigned int vpx_sad4x4_neon(const uint8_t* src_ptr,
                             int src_stride,
                             const uint8_t* ref_ptr,
                             int ref_stride);
RTCD_EXTERN unsigned int (*vpx_sad4x4)(const uint8_t* src_ptr,
                                       int src_stride,
                                       const uint8_t* ref_ptr,
                                       int ref_stride);

unsigned int vpx_sad4x4_avg_c(const uint8_t* src_ptr,
                              int src_stride,
                              const uint8_t* ref_ptr,
                              int ref_stride,
                              const uint8_t* second_pred);
unsigned int vpx_sad4x4_avg_neon(const uint8_t* src_ptr,
                                 int src_stride,
                                 const uint8_t* ref_ptr,
                                 int ref_stride,
                                 const uint8_t* second_pred);
RTCD_EXTERN unsigned int (*vpx_sad4x4_avg)(const uint8_t* src_ptr,
                                           int src_stride,
                                           const uint8_t* ref_ptr,
                                           int ref_stride,
                                           const uint8_t* second_pred);

void vpx_sad4x4x3_c(const uint8_t* src_ptr,
                    int src_stride,
                    const uint8_t* ref_ptr,
                    int ref_stride,
                    uint32_t* sad_array);
#define vpx_sad4x4x3 vpx_sad4x4x3_c

void vpx_sad4x4x4d_c(const uint8_t* src_ptr,
                     int src_stride,
                     const uint8_t* const ref_array[],
                     int ref_stride,
                     uint32_t* sad_array);
void vpx_sad4x4x4d_neon(const uint8_t* src_ptr,
                        int src_stride,
                        const uint8_t* const ref_array[],
                        int ref_stride,
                        uint32_t* sad_array);
RTCD_EXTERN void (*vpx_sad4x4x4d)(const uint8_t* src_ptr,
                                  int src_stride,
                                  const uint8_t* const ref_array[],
                                  int ref_stride,
                                  uint32_t* sad_array);

void vpx_sad4x4x8_c(const uint8_t* src_ptr,
                    int src_stride,
                    const uint8_t* ref_ptr,
                    int ref_stride,
                    uint32_t* sad_array);
#define vpx_sad4x4x8 vpx_sad4x4x8_c

unsigned int vpx_sad4x8_c(const uint8_t* src_ptr,
                          int src_stride,
                          const uint8_t* ref_ptr,
                          int ref_stride);
unsigned int vpx_sad4x8_neon(const uint8_t* src_ptr,
                             int src_stride,
                             const uint8_t* ref_ptr,
                             int ref_stride);
RTCD_EXTERN unsigned int (*vpx_sad4x8)(const uint8_t* src_ptr,
                                       int src_stride,
                                       const uint8_t* ref_ptr,
                                       int ref_stride);

unsigned int vpx_sad4x8_avg_c(const uint8_t* src_ptr,
                              int src_stride,
                              const uint8_t* ref_ptr,
                              int ref_stride,
                              const uint8_t* second_pred);
unsigned int vpx_sad4x8_avg_neon(const uint8_t* src_ptr,
                                 int src_stride,
                                 const uint8_t* ref_ptr,
                                 int ref_stride,
                                 const uint8_t* second_pred);
RTCD_EXTERN unsigned int (*vpx_sad4x8_avg)(const uint8_t* src_ptr,
                                           int src_stride,
                                           const uint8_t* ref_ptr,
                                           int ref_stride,
                                           const uint8_t* second_pred);

void vpx_sad4x8x4d_c(const uint8_t* src_ptr,
                     int src_stride,
                     const uint8_t* const ref_array[],
                     int ref_stride,
                     uint32_t* sad_array);
void vpx_sad4x8x4d_neon(const uint8_t* src_ptr,
                        int src_stride,
                        const uint8_t* const ref_array[],
                        int ref_stride,
                        uint32_t* sad_array);
RTCD_EXTERN void (*vpx_sad4x8x4d)(const uint8_t* src_ptr,
                                  int src_stride,
                                  const uint8_t* const ref_array[],
                                  int ref_stride,
                                  uint32_t* sad_array);

unsigned int vpx_sad64x32_c(const uint8_t* src_ptr,
                            int src_stride,
                            const uint8_t* ref_ptr,
                            int ref_stride);
unsigned int vpx_sad64x32_neon(const uint8_t* src_ptr,
                               int src_stride,
                               const uint8_t* ref_ptr,
                               int ref_stride);
RTCD_EXTERN unsigned int (*vpx_sad64x32)(const uint8_t* src_ptr,
                                         int src_stride,
                                         const uint8_t* ref_ptr,
                                         int ref_stride);

unsigned int vpx_sad64x32_avg_c(const uint8_t* src_ptr,
                                int src_stride,
                                const uint8_t* ref_ptr,
                                int ref_stride,
                                const uint8_t* second_pred);
unsigned int vpx_sad64x32_avg_neon(const uint8_t* src_ptr,
                                   int src_stride,
                                   const uint8_t* ref_ptr,
                                   int ref_stride,
                                   const uint8_t* second_pred);
RTCD_EXTERN unsigned int (*vpx_sad64x32_avg)(const uint8_t* src_ptr,
                                             int src_stride,
                                             const uint8_t* ref_ptr,
                                             int ref_stride,
                                             const uint8_t* second_pred);

void vpx_sad64x32x4d_c(const uint8_t* src_ptr,
                       int src_stride,
                       const uint8_t* const ref_array[],
                       int ref_stride,
                       uint32_t* sad_array);
void vpx_sad64x32x4d_neon(const uint8_t* src_ptr,
                          int src_stride,
                          const uint8_t* const ref_array[],
                          int ref_stride,
                          uint32_t* sad_array);
RTCD_EXTERN void (*vpx_sad64x32x4d)(const uint8_t* src_ptr,
                                    int src_stride,
                                    const uint8_t* const ref_array[],
                                    int ref_stride,
                                    uint32_t* sad_array);

unsigned int vpx_sad64x64_c(const uint8_t* src_ptr,
                            int src_stride,
                            const uint8_t* ref_ptr,
                            int ref_stride);
unsigned int vpx_sad64x64_neon(const uint8_t* src_ptr,
                               int src_stride,
                               const uint8_t* ref_ptr,
                               int ref_stride);
RTCD_EXTERN unsigned int (*vpx_sad64x64)(const uint8_t* src_ptr,
                                         int src_stride,
                                         const uint8_t* ref_ptr,
                                         int ref_stride);

unsigned int vpx_sad64x64_avg_c(const uint8_t* src_ptr,
                                int src_stride,
                                const uint8_t* ref_ptr,
                                int ref_stride,
                                const uint8_t* second_pred);
unsigned int vpx_sad64x64_avg_neon(const uint8_t* src_ptr,
                                   int src_stride,
                                   const uint8_t* ref_ptr,
                                   int ref_stride,
                                   const uint8_t* second_pred);
RTCD_EXTERN unsigned int (*vpx_sad64x64_avg)(const uint8_t* src_ptr,
                                             int src_stride,
                                             const uint8_t* ref_ptr,
                                             int ref_stride,
                                             const uint8_t* second_pred);

void vpx_sad64x64x4d_c(const uint8_t* src_ptr,
                       int src_stride,
                       const uint8_t* const ref_array[],
                       int ref_stride,
                       uint32_t* sad_array);
void vpx_sad64x64x4d_neon(const uint8_t* src_ptr,
                          int src_stride,
                          const uint8_t* const ref_array[],
                          int ref_stride,
                          uint32_t* sad_array);
RTCD_EXTERN void (*vpx_sad64x64x4d)(const uint8_t* src_ptr,
                                    int src_stride,
                                    const uint8_t* const ref_array[],
                                    int ref_stride,
                                    uint32_t* sad_array);

unsigned int vpx_sad8x16_c(const uint8_t* src_ptr,
                           int src_stride,
                           const uint8_t* ref_ptr,
                           int ref_stride);
unsigned int vpx_sad8x16_neon(const uint8_t* src_ptr,
                              int src_stride,
                              const uint8_t* ref_ptr,
                              int ref_stride);
RTCD_EXTERN unsigned int (*vpx_sad8x16)(const uint8_t* src_ptr,
                                        int src_stride,
                                        const uint8_t* ref_ptr,
                                        int ref_stride);

unsigned int vpx_sad8x16_avg_c(const uint8_t* src_ptr,
                               int src_stride,
                               const uint8_t* ref_ptr,
                               int ref_stride,
                               const uint8_t* second_pred);
unsigned int vpx_sad8x16_avg_neon(const uint8_t* src_ptr,
                                  int src_stride,
                                  const uint8_t* ref_ptr,
                                  int ref_stride,
                                  const uint8_t* second_pred);
RTCD_EXTERN unsigned int (*vpx_sad8x16_avg)(const uint8_t* src_ptr,
                                            int src_stride,
                                            const uint8_t* ref_ptr,
                                            int ref_stride,
                                            const uint8_t* second_pred);

void vpx_sad8x16x3_c(const uint8_t* src_ptr,
                     int src_stride,
                     const uint8_t* ref_ptr,
                     int ref_stride,
                     uint32_t* sad_array);
#define vpx_sad8x16x3 vpx_sad8x16x3_c

void vpx_sad8x16x4d_c(const uint8_t* src_ptr,
                      int src_stride,
                      const uint8_t* const ref_array[],
                      int ref_stride,
                      uint32_t* sad_array);
void vpx_sad8x16x4d_neon(const uint8_t* src_ptr,
                         int src_stride,
                         const uint8_t* const ref_array[],
                         int ref_stride,
                         uint32_t* sad_array);
RTCD_EXTERN void (*vpx_sad8x16x4d)(const uint8_t* src_ptr,
                                   int src_stride,
                                   const uint8_t* const ref_array[],
                                   int ref_stride,
                                   uint32_t* sad_array);

void vpx_sad8x16x8_c(const uint8_t* src_ptr,
                     int src_stride,
                     const uint8_t* ref_ptr,
                     int ref_stride,
                     uint32_t* sad_array);
#define vpx_sad8x16x8 vpx_sad8x16x8_c

unsigned int vpx_sad8x4_c(const uint8_t* src_ptr,
                          int src_stride,
                          const uint8_t* ref_ptr,
                          int ref_stride);
unsigned int vpx_sad8x4_neon(const uint8_t* src_ptr,
                             int src_stride,
                             const uint8_t* ref_ptr,
                             int ref_stride);
RTCD_EXTERN unsigned int (*vpx_sad8x4)(const uint8_t* src_ptr,
                                       int src_stride,
                                       const uint8_t* ref_ptr,
                                       int ref_stride);

unsigned int vpx_sad8x4_avg_c(const uint8_t* src_ptr,
                              int src_stride,
                              const uint8_t* ref_ptr,
                              int ref_stride,
                              const uint8_t* second_pred);
unsigned int vpx_sad8x4_avg_neon(const uint8_t* src_ptr,
                                 int src_stride,
                                 const uint8_t* ref_ptr,
                                 int ref_stride,
                                 const uint8_t* second_pred);
RTCD_EXTERN unsigned int (*vpx_sad8x4_avg)(const uint8_t* src_ptr,
                                           int src_stride,
                                           const uint8_t* ref_ptr,
                                           int ref_stride,
                                           const uint8_t* second_pred);

void vpx_sad8x4x4d_c(const uint8_t* src_ptr,
                     int src_stride,
                     const uint8_t* const ref_array[],
                     int ref_stride,
                     uint32_t* sad_array);
void vpx_sad8x4x4d_neon(const uint8_t* src_ptr,
                        int src_stride,
                        const uint8_t* const ref_array[],
                        int ref_stride,
                        uint32_t* sad_array);
RTCD_EXTERN void (*vpx_sad8x4x4d)(const uint8_t* src_ptr,
                                  int src_stride,
                                  const uint8_t* const ref_array[],
                                  int ref_stride,
                                  uint32_t* sad_array);

unsigned int vpx_sad8x8_c(const uint8_t* src_ptr,
                          int src_stride,
                          const uint8_t* ref_ptr,
                          int ref_stride);
unsigned int vpx_sad8x8_neon(const uint8_t* src_ptr,
                             int src_stride,
                             const uint8_t* ref_ptr,
                             int ref_stride);
RTCD_EXTERN unsigned int (*vpx_sad8x8)(const uint8_t* src_ptr,
                                       int src_stride,
                                       const uint8_t* ref_ptr,
                                       int ref_stride);

unsigned int vpx_sad8x8_avg_c(const uint8_t* src_ptr,
                              int src_stride,
                              const uint8_t* ref_ptr,
                              int ref_stride,
                              const uint8_t* second_pred);
unsigned int vpx_sad8x8_avg_neon(const uint8_t* src_ptr,
                                 int src_stride,
                                 const uint8_t* ref_ptr,
                                 int ref_stride,
                                 const uint8_t* second_pred);
RTCD_EXTERN unsigned int (*vpx_sad8x8_avg)(const uint8_t* src_ptr,
                                           int src_stride,
                                           const uint8_t* ref_ptr,
                                           int ref_stride,
                                           const uint8_t* second_pred);

void vpx_sad8x8x3_c(const uint8_t* src_ptr,
                    int src_stride,
                    const uint8_t* ref_ptr,
                    int ref_stride,
                    uint32_t* sad_array);
#define vpx_sad8x8x3 vpx_sad8x8x3_c

void vpx_sad8x8x4d_c(const uint8_t* src_ptr,
                     int src_stride,
                     const uint8_t* const ref_array[],
                     int ref_stride,
                     uint32_t* sad_array);
void vpx_sad8x8x4d_neon(const uint8_t* src_ptr,
                        int src_stride,
                        const uint8_t* const ref_array[],
                        int ref_stride,
                        uint32_t* sad_array);
RTCD_EXTERN void (*vpx_sad8x8x4d)(const uint8_t* src_ptr,
                                  int src_stride,
                                  const uint8_t* const ref_array[],
                                  int ref_stride,
                                  uint32_t* sad_array);

void vpx_sad8x8x8_c(const uint8_t* src_ptr,
                    int src_stride,
                    const uint8_t* ref_ptr,
                    int ref_stride,
                    uint32_t* sad_array);
#define vpx_sad8x8x8 vpx_sad8x8x8_c

int vpx_satd_c(const int16_t* coeff, int length);
int vpx_satd_neon(const int16_t* coeff, int length);
RTCD_EXTERN int (*vpx_satd)(const int16_t* coeff, int length);

void vpx_scaled_2d_c(const uint8_t* src,
                     ptrdiff_t src_stride,
                     uint8_t* dst,
                     ptrdiff_t dst_stride,
                     const InterpKernel* filter,
                     int x0_q4,
                     int x_step_q4,
                     int y0_q4,
                     int y_step_q4,
                     int w,
                     int h);
void vpx_scaled_2d_neon(const uint8_t* src,
                        ptrdiff_t src_stride,
                        uint8_t* dst,
                        ptrdiff_t dst_stride,
                        const InterpKernel* filter,
                        int x0_q4,
                        int x_step_q4,
                        int y0_q4,
                        int y_step_q4,
                        int w,
                        int h);
RTCD_EXTERN void (*vpx_scaled_2d)(const uint8_t* src,
                                  ptrdiff_t src_stride,
                                  uint8_t* dst,
                                  ptrdiff_t dst_stride,
                                  const InterpKernel* filter,
                                  int x0_q4,
                                  int x_step_q4,
                                  int y0_q4,
                                  int y_step_q4,
                                  int w,
                                  int h);

void vpx_scaled_avg_2d_c(const uint8_t* src,
                         ptrdiff_t src_stride,
                         uint8_t* dst,
                         ptrdiff_t dst_stride,
                         const InterpKernel* filter,
                         int x0_q4,
                         int x_step_q4,
                         int y0_q4,
                         int y_step_q4,
                         int w,
                         int h);
#define vpx_scaled_avg_2d vpx_scaled_avg_2d_c

void vpx_scaled_avg_horiz_c(const uint8_t* src,
                            ptrdiff_t src_stride,
                            uint8_t* dst,
                            ptrdiff_t dst_stride,
                            const InterpKernel* filter,
                            int x0_q4,
                            int x_step_q4,
                            int y0_q4,
                            int y_step_q4,
                            int w,
                            int h);
#define vpx_scaled_avg_horiz vpx_scaled_avg_horiz_c

void vpx_scaled_avg_vert_c(const uint8_t* src,
                           ptrdiff_t src_stride,
                           uint8_t* dst,
                           ptrdiff_t dst_stride,
                           const InterpKernel* filter,
                           int x0_q4,
                           int x_step_q4,
                           int y0_q4,
                           int y_step_q4,
                           int w,
                           int h);
#define vpx_scaled_avg_vert vpx_scaled_avg_vert_c

void vpx_scaled_horiz_c(const uint8_t* src,
                        ptrdiff_t src_stride,
                        uint8_t* dst,
                        ptrdiff_t dst_stride,
                        const InterpKernel* filter,
                        int x0_q4,
                        int x_step_q4,
                        int y0_q4,
                        int y_step_q4,
                        int w,
                        int h);
#define vpx_scaled_horiz vpx_scaled_horiz_c

void vpx_scaled_vert_c(const uint8_t* src,
                       ptrdiff_t src_stride,
                       uint8_t* dst,
                       ptrdiff_t dst_stride,
                       const InterpKernel* filter,
                       int x0_q4,
                       int x_step_q4,
                       int y0_q4,
                       int y_step_q4,
                       int w,
                       int h);
#define vpx_scaled_vert vpx_scaled_vert_c

uint32_t vpx_sub_pixel_avg_variance16x16_c(const uint8_t* src_ptr,
                                           int src_stride,
                                           int x_offset,
                                           int y_offset,
                                           const uint8_t* ref_ptr,
                                           int ref_stride,
                                           uint32_t* sse,
                                           const uint8_t* second_pred);
uint32_t vpx_sub_pixel_avg_variance16x16_neon(const uint8_t* src_ptr,
                                              int src_stride,
                                              int x_offset,
                                              int y_offset,
                                              const uint8_t* ref_ptr,
                                              int ref_stride,
                                              uint32_t* sse,
                                              const uint8_t* second_pred);
RTCD_EXTERN uint32_t (*vpx_sub_pixel_avg_variance16x16)(
    const uint8_t* src_ptr,
    int src_stride,
    int x_offset,
    int y_offset,
    const uint8_t* ref_ptr,
    int ref_stride,
    uint32_t* sse,
    const uint8_t* second_pred);

uint32_t vpx_sub_pixel_avg_variance16x32_c(const uint8_t* src_ptr,
                                           int src_stride,
                                           int x_offset,
                                           int y_offset,
                                           const uint8_t* ref_ptr,
                                           int ref_stride,
                                           uint32_t* sse,
                                           const uint8_t* second_pred);
uint32_t vpx_sub_pixel_avg_variance16x32_neon(const uint8_t* src_ptr,
                                              int src_stride,
                                              int x_offset,
                                              int y_offset,
                                              const uint8_t* ref_ptr,
                                              int ref_stride,
                                              uint32_t* sse,
                                              const uint8_t* second_pred);
RTCD_EXTERN uint32_t (*vpx_sub_pixel_avg_variance16x32)(
    const uint8_t* src_ptr,
    int src_stride,
    int x_offset,
    int y_offset,
    const uint8_t* ref_ptr,
    int ref_stride,
    uint32_t* sse,
    const uint8_t* second_pred);

uint32_t vpx_sub_pixel_avg_variance16x8_c(const uint8_t* src_ptr,
                                          int src_stride,
                                          int x_offset,
                                          int y_offset,
                                          const uint8_t* ref_ptr,
                                          int ref_stride,
                                          uint32_t* sse,
                                          const uint8_t* second_pred);
uint32_t vpx_sub_pixel_avg_variance16x8_neon(const uint8_t* src_ptr,
                                             int src_stride,
                                             int x_offset,
                                             int y_offset,
                                             const uint8_t* ref_ptr,
                                             int ref_stride,
                                             uint32_t* sse,
                                             const uint8_t* second_pred);
RTCD_EXTERN uint32_t (*vpx_sub_pixel_avg_variance16x8)(
    const uint8_t* src_ptr,
    int src_stride,
    int x_offset,
    int y_offset,
    const uint8_t* ref_ptr,
    int ref_stride,
    uint32_t* sse,
    const uint8_t* second_pred);

uint32_t vpx_sub_pixel_avg_variance32x16_c(const uint8_t* src_ptr,
                                           int src_stride,
                                           int x_offset,
                                           int y_offset,
                                           const uint8_t* ref_ptr,
                                           int ref_stride,
                                           uint32_t* sse,
                                           const uint8_t* second_pred);
uint32_t vpx_sub_pixel_avg_variance32x16_neon(const uint8_t* src_ptr,
                                              int src_stride,
                                              int x_offset,
                                              int y_offset,
                                              const uint8_t* ref_ptr,
                                              int ref_stride,
                                              uint32_t* sse,
                                              const uint8_t* second_pred);
RTCD_EXTERN uint32_t (*vpx_sub_pixel_avg_variance32x16)(
    const uint8_t* src_ptr,
    int src_stride,
    int x_offset,
    int y_offset,
    const uint8_t* ref_ptr,
    int ref_stride,
    uint32_t* sse,
    const uint8_t* second_pred);

uint32_t vpx_sub_pixel_avg_variance32x32_c(const uint8_t* src_ptr,
                                           int src_stride,
                                           int x_offset,
                                           int y_offset,
                                           const uint8_t* ref_ptr,
                                           int ref_stride,
                                           uint32_t* sse,
                                           const uint8_t* second_pred);
uint32_t vpx_sub_pixel_avg_variance32x32_neon(const uint8_t* src_ptr,
                                              int src_stride,
                                              int x_offset,
                                              int y_offset,
                                              const uint8_t* ref_ptr,
                                              int ref_stride,
                                              uint32_t* sse,
                                              const uint8_t* second_pred);
RTCD_EXTERN uint32_t (*vpx_sub_pixel_avg_variance32x32)(
    const uint8_t* src_ptr,
    int src_stride,
    int x_offset,
    int y_offset,
    const uint8_t* ref_ptr,
    int ref_stride,
    uint32_t* sse,
    const uint8_t* second_pred);

uint32_t vpx_sub_pixel_avg_variance32x64_c(const uint8_t* src_ptr,
                                           int src_stride,
                                           int x_offset,
                                           int y_offset,
                                           const uint8_t* ref_ptr,
                                           int ref_stride,
                                           uint32_t* sse,
                                           const uint8_t* second_pred);
uint32_t vpx_sub_pixel_avg_variance32x64_neon(const uint8_t* src_ptr,
                                              int src_stride,
                                              int x_offset,
                                              int y_offset,
                                              const uint8_t* ref_ptr,
                                              int ref_stride,
                                              uint32_t* sse,
                                              const uint8_t* second_pred);
RTCD_EXTERN uint32_t (*vpx_sub_pixel_avg_variance32x64)(
    const uint8_t* src_ptr,
    int src_stride,
    int x_offset,
    int y_offset,
    const uint8_t* ref_ptr,
    int ref_stride,
    uint32_t* sse,
    const uint8_t* second_pred);

uint32_t vpx_sub_pixel_avg_variance4x4_c(const uint8_t* src_ptr,
                                         int src_stride,
                                         int x_offset,
                                         int y_offset,
                                         const uint8_t* ref_ptr,
                                         int ref_stride,
                                         uint32_t* sse,
                                         const uint8_t* second_pred);
uint32_t vpx_sub_pixel_avg_variance4x4_neon(const uint8_t* src_ptr,
                                            int src_stride,
                                            int x_offset,
                                            int y_offset,
                                            const uint8_t* ref_ptr,
                                            int ref_stride,
                                            uint32_t* sse,
                                            const uint8_t* second_pred);
RTCD_EXTERN uint32_t (*vpx_sub_pixel_avg_variance4x4)(
    const uint8_t* src_ptr,
    int src_stride,
    int x_offset,
    int y_offset,
    const uint8_t* ref_ptr,
    int ref_stride,
    uint32_t* sse,
    const uint8_t* second_pred);

uint32_t vpx_sub_pixel_avg_variance4x8_c(const uint8_t* src_ptr,
                                         int src_stride,
                                         int x_offset,
                                         int y_offset,
                                         const uint8_t* ref_ptr,
                                         int ref_stride,
                                         uint32_t* sse,
                                         const uint8_t* second_pred);
uint32_t vpx_sub_pixel_avg_variance4x8_neon(const uint8_t* src_ptr,
                                            int src_stride,
                                            int x_offset,
                                            int y_offset,
                                            const uint8_t* ref_ptr,
                                            int ref_stride,
                                            uint32_t* sse,
                                            const uint8_t* second_pred);
RTCD_EXTERN uint32_t (*vpx_sub_pixel_avg_variance4x8)(
    const uint8_t* src_ptr,
    int src_stride,
    int x_offset,
    int y_offset,
    const uint8_t* ref_ptr,
    int ref_stride,
    uint32_t* sse,
    const uint8_t* second_pred);

uint32_t vpx_sub_pixel_avg_variance64x32_c(const uint8_t* src_ptr,
                                           int src_stride,
                                           int x_offset,
                                           int y_offset,
                                           const uint8_t* ref_ptr,
                                           int ref_stride,
                                           uint32_t* sse,
                                           const uint8_t* second_pred);
uint32_t vpx_sub_pixel_avg_variance64x32_neon(const uint8_t* src_ptr,
                                              int src_stride,
                                              int x_offset,
                                              int y_offset,
                                              const uint8_t* ref_ptr,
                                              int ref_stride,
                                              uint32_t* sse,
                                              const uint8_t* second_pred);
RTCD_EXTERN uint32_t (*vpx_sub_pixel_avg_variance64x32)(
    const uint8_t* src_ptr,
    int src_stride,
    int x_offset,
    int y_offset,
    const uint8_t* ref_ptr,
    int ref_stride,
    uint32_t* sse,
    const uint8_t* second_pred);

uint32_t vpx_sub_pixel_avg_variance64x64_c(const uint8_t* src_ptr,
                                           int src_stride,
                                           int x_offset,
                                           int y_offset,
                                           const uint8_t* ref_ptr,
                                           int ref_stride,
                                           uint32_t* sse,
                                           const uint8_t* second_pred);
uint32_t vpx_sub_pixel_avg_variance64x64_neon(const uint8_t* src_ptr,
                                              int src_stride,
                                              int x_offset,
                                              int y_offset,
                                              const uint8_t* ref_ptr,
                                              int ref_stride,
                                              uint32_t* sse,
                                              const uint8_t* second_pred);
RTCD_EXTERN uint32_t (*vpx_sub_pixel_avg_variance64x64)(
    const uint8_t* src_ptr,
    int src_stride,
    int x_offset,
    int y_offset,
    const uint8_t* ref_ptr,
    int ref_stride,
    uint32_t* sse,
    const uint8_t* second_pred);

uint32_t vpx_sub_pixel_avg_variance8x16_c(const uint8_t* src_ptr,
                                          int src_stride,
                                          int x_offset,
                                          int y_offset,
                                          const uint8_t* ref_ptr,
                                          int ref_stride,
                                          uint32_t* sse,
                                          const uint8_t* second_pred);
uint32_t vpx_sub_pixel_avg_variance8x16_neon(const uint8_t* src_ptr,
                                             int src_stride,
                                             int x_offset,
                                             int y_offset,
                                             const uint8_t* ref_ptr,
                                             int ref_stride,
                                             uint32_t* sse,
                                             const uint8_t* second_pred);
RTCD_EXTERN uint32_t (*vpx_sub_pixel_avg_variance8x16)(
    const uint8_t* src_ptr,
    int src_stride,
    int x_offset,
    int y_offset,
    const uint8_t* ref_ptr,
    int ref_stride,
    uint32_t* sse,
    const uint8_t* second_pred);

uint32_t vpx_sub_pixel_avg_variance8x4_c(const uint8_t* src_ptr,
                                         int src_stride,
                                         int x_offset,
                                         int y_offset,
                                         const uint8_t* ref_ptr,
                                         int ref_stride,
                                         uint32_t* sse,
                                         const uint8_t* second_pred);
uint32_t vpx_sub_pixel_avg_variance8x4_neon(const uint8_t* src_ptr,
                                            int src_stride,
                                            int x_offset,
                                            int y_offset,
                                            const uint8_t* ref_ptr,
                                            int ref_stride,
                                            uint32_t* sse,
                                            const uint8_t* second_pred);
RTCD_EXTERN uint32_t (*vpx_sub_pixel_avg_variance8x4)(
    const uint8_t* src_ptr,
    int src_stride,
    int x_offset,
    int y_offset,
    const uint8_t* ref_ptr,
    int ref_stride,
    uint32_t* sse,
    const uint8_t* second_pred);

uint32_t vpx_sub_pixel_avg_variance8x8_c(const uint8_t* src_ptr,
                                         int src_stride,
                                         int x_offset,
                                         int y_offset,
                                         const uint8_t* ref_ptr,
                                         int ref_stride,
                                         uint32_t* sse,
                                         const uint8_t* second_pred);
uint32_t vpx_sub_pixel_avg_variance8x8_neon(const uint8_t* src_ptr,
                                            int src_stride,
                                            int x_offset,
                                            int y_offset,
                                            const uint8_t* ref_ptr,
                                            int ref_stride,
                                            uint32_t* sse,
                                            const uint8_t* second_pred);
RTCD_EXTERN uint32_t (*vpx_sub_pixel_avg_variance8x8)(
    const uint8_t* src_ptr,
    int src_stride,
    int x_offset,
    int y_offset,
    const uint8_t* ref_ptr,
    int ref_stride,
    uint32_t* sse,
    const uint8_t* second_pred);

uint32_t vpx_sub_pixel_variance16x16_c(const uint8_t* src_ptr,
                                       int src_stride,
                                       int x_offset,
                                       int y_offset,
                                       const uint8_t* ref_ptr,
                                       int ref_stride,
                                       uint32_t* sse);
uint32_t vpx_sub_pixel_variance16x16_neon(const uint8_t* src_ptr,
                                          int src_stride,
                                          int x_offset,
                                          int y_offset,
                                          const uint8_t* ref_ptr,
                                          int ref_stride,
                                          uint32_t* sse);
RTCD_EXTERN uint32_t (*vpx_sub_pixel_variance16x16)(const uint8_t* src_ptr,
                                                    int src_stride,
                                                    int x_offset,
                                                    int y_offset,
                                                    const uint8_t* ref_ptr,
                                                    int ref_stride,
                                                    uint32_t* sse);

uint32_t vpx_sub_pixel_variance16x32_c(const uint8_t* src_ptr,
                                       int src_stride,
                                       int x_offset,
                                       int y_offset,
                                       const uint8_t* ref_ptr,
                                       int ref_stride,
                                       uint32_t* sse);
uint32_t vpx_sub_pixel_variance16x32_neon(const uint8_t* src_ptr,
                                          int src_stride,
                                          int x_offset,
                                          int y_offset,
                                          const uint8_t* ref_ptr,
                                          int ref_stride,
                                          uint32_t* sse);
RTCD_EXTERN uint32_t (*vpx_sub_pixel_variance16x32)(const uint8_t* src_ptr,
                                                    int src_stride,
                                                    int x_offset,
                                                    int y_offset,
                                                    const uint8_t* ref_ptr,
                                                    int ref_stride,
                                                    uint32_t* sse);

uint32_t vpx_sub_pixel_variance16x8_c(const uint8_t* src_ptr,
                                      int src_stride,
                                      int x_offset,
                                      int y_offset,
                                      const uint8_t* ref_ptr,
                                      int ref_stride,
                                      uint32_t* sse);
uint32_t vpx_sub_pixel_variance16x8_neon(const uint8_t* src_ptr,
                                         int src_stride,
                                         int x_offset,
                                         int y_offset,
                                         const uint8_t* ref_ptr,
                                         int ref_stride,
                                         uint32_t* sse);
RTCD_EXTERN uint32_t (*vpx_sub_pixel_variance16x8)(const uint8_t* src_ptr,
                                                   int src_stride,
                                                   int x_offset,
                                                   int y_offset,
                                                   const uint8_t* ref_ptr,
                                                   int ref_stride,
                                                   uint32_t* sse);

uint32_t vpx_sub_pixel_variance32x16_c(const uint8_t* src_ptr,
                                       int src_stride,
                                       int x_offset,
                                       int y_offset,
                                       const uint8_t* ref_ptr,
                                       int ref_stride,
                                       uint32_t* sse);
uint32_t vpx_sub_pixel_variance32x16_neon(const uint8_t* src_ptr,
                                          int src_stride,
                                          int x_offset,
                                          int y_offset,
                                          const uint8_t* ref_ptr,
                                          int ref_stride,
                                          uint32_t* sse);
RTCD_EXTERN uint32_t (*vpx_sub_pixel_variance32x16)(const uint8_t* src_ptr,
                                                    int src_stride,
                                                    int x_offset,
                                                    int y_offset,
                                                    const uint8_t* ref_ptr,
                                                    int ref_stride,
                                                    uint32_t* sse);

uint32_t vpx_sub_pixel_variance32x32_c(const uint8_t* src_ptr,
                                       int src_stride,
                                       int x_offset,
                                       int y_offset,
                                       const uint8_t* ref_ptr,
                                       int ref_stride,
                                       uint32_t* sse);
uint32_t vpx_sub_pixel_variance32x32_neon(const uint8_t* src_ptr,
                                          int src_stride,
                                          int x_offset,
                                          int y_offset,
                                          const uint8_t* ref_ptr,
                                          int ref_stride,
                                          uint32_t* sse);
RTCD_EXTERN uint32_t (*vpx_sub_pixel_variance32x32)(const uint8_t* src_ptr,
                                                    int src_stride,
                                                    int x_offset,
                                                    int y_offset,
                                                    const uint8_t* ref_ptr,
                                                    int ref_stride,
                                                    uint32_t* sse);

uint32_t vpx_sub_pixel_variance32x64_c(const uint8_t* src_ptr,
                                       int src_stride,
                                       int x_offset,
                                       int y_offset,
                                       const uint8_t* ref_ptr,
                                       int ref_stride,
                                       uint32_t* sse);
uint32_t vpx_sub_pixel_variance32x64_neon(const uint8_t* src_ptr,
                                          int src_stride,
                                          int x_offset,
                                          int y_offset,
                                          const uint8_t* ref_ptr,
                                          int ref_stride,
                                          uint32_t* sse);
RTCD_EXTERN uint32_t (*vpx_sub_pixel_variance32x64)(const uint8_t* src_ptr,
                                                    int src_stride,
                                                    int x_offset,
                                                    int y_offset,
                                                    const uint8_t* ref_ptr,
                                                    int ref_stride,
                                                    uint32_t* sse);

uint32_t vpx_sub_pixel_variance4x4_c(const uint8_t* src_ptr,
                                     int src_stride,
                                     int x_offset,
                                     int y_offset,
                                     const uint8_t* ref_ptr,
                                     int ref_stride,
                                     uint32_t* sse);
uint32_t vpx_sub_pixel_variance4x4_neon(const uint8_t* src_ptr,
                                        int src_stride,
                                        int x_offset,
                                        int y_offset,
                                        const uint8_t* ref_ptr,
                                        int ref_stride,
                                        uint32_t* sse);
RTCD_EXTERN uint32_t (*vpx_sub_pixel_variance4x4)(const uint8_t* src_ptr,
                                                  int src_stride,
                                                  int x_offset,
                                                  int y_offset,
                                                  const uint8_t* ref_ptr,
                                                  int ref_stride,
                                                  uint32_t* sse);

uint32_t vpx_sub_pixel_variance4x8_c(const uint8_t* src_ptr,
                                     int src_stride,
                                     int x_offset,
                                     int y_offset,
                                     const uint8_t* ref_ptr,
                                     int ref_stride,
                                     uint32_t* sse);
uint32_t vpx_sub_pixel_variance4x8_neon(const uint8_t* src_ptr,
                                        int src_stride,
                                        int x_offset,
                                        int y_offset,
                                        const uint8_t* ref_ptr,
                                        int ref_stride,
                                        uint32_t* sse);
RTCD_EXTERN uint32_t (*vpx_sub_pixel_variance4x8)(const uint8_t* src_ptr,
                                                  int src_stride,
                                                  int x_offset,
                                                  int y_offset,
                                                  const uint8_t* ref_ptr,
                                                  int ref_stride,
                                                  uint32_t* sse);

uint32_t vpx_sub_pixel_variance64x32_c(const uint8_t* src_ptr,
                                       int src_stride,
                                       int x_offset,
                                       int y_offset,
                                       const uint8_t* ref_ptr,
                                       int ref_stride,
                                       uint32_t* sse);
uint32_t vpx_sub_pixel_variance64x32_neon(const uint8_t* src_ptr,
                                          int src_stride,
                                          int x_offset,
                                          int y_offset,
                                          const uint8_t* ref_ptr,
                                          int ref_stride,
                                          uint32_t* sse);
RTCD_EXTERN uint32_t (*vpx_sub_pixel_variance64x32)(const uint8_t* src_ptr,
                                                    int src_stride,
                                                    int x_offset,
                                                    int y_offset,
                                                    const uint8_t* ref_ptr,
                                                    int ref_stride,
                                                    uint32_t* sse);

uint32_t vpx_sub_pixel_variance64x64_c(const uint8_t* src_ptr,
                                       int src_stride,
                                       int x_offset,
                                       int y_offset,
                                       const uint8_t* ref_ptr,
                                       int ref_stride,
                                       uint32_t* sse);
uint32_t vpx_sub_pixel_variance64x64_neon(const uint8_t* src_ptr,
                                          int src_stride,
                                          int x_offset,
                                          int y_offset,
                                          const uint8_t* ref_ptr,
                                          int ref_stride,
                                          uint32_t* sse);
RTCD_EXTERN uint32_t (*vpx_sub_pixel_variance64x64)(const uint8_t* src_ptr,
                                                    int src_stride,
                                                    int x_offset,
                                                    int y_offset,
                                                    const uint8_t* ref_ptr,
                                                    int ref_stride,
                                                    uint32_t* sse);

uint32_t vpx_sub_pixel_variance8x16_c(const uint8_t* src_ptr,
                                      int src_stride,
                                      int x_offset,
                                      int y_offset,
                                      const uint8_t* ref_ptr,
                                      int ref_stride,
                                      uint32_t* sse);
uint32_t vpx_sub_pixel_variance8x16_neon(const uint8_t* src_ptr,
                                         int src_stride,
                                         int x_offset,
                                         int y_offset,
                                         const uint8_t* ref_ptr,
                                         int ref_stride,
                                         uint32_t* sse);
RTCD_EXTERN uint32_t (*vpx_sub_pixel_variance8x16)(const uint8_t* src_ptr,
                                                   int src_stride,
                                                   int x_offset,
                                                   int y_offset,
                                                   const uint8_t* ref_ptr,
                                                   int ref_stride,
                                                   uint32_t* sse);

uint32_t vpx_sub_pixel_variance8x4_c(const uint8_t* src_ptr,
                                     int src_stride,
                                     int x_offset,
                                     int y_offset,
                                     const uint8_t* ref_ptr,
                                     int ref_stride,
                                     uint32_t* sse);
uint32_t vpx_sub_pixel_variance8x4_neon(const uint8_t* src_ptr,
                                        int src_stride,
                                        int x_offset,
                                        int y_offset,
                                        const uint8_t* ref_ptr,
                                        int ref_stride,
                                        uint32_t* sse);
RTCD_EXTERN uint32_t (*vpx_sub_pixel_variance8x4)(const uint8_t* src_ptr,
                                                  int src_stride,
                                                  int x_offset,
                                                  int y_offset,
                                                  const uint8_t* ref_ptr,
                                                  int ref_stride,
                                                  uint32_t* sse);

uint32_t vpx_sub_pixel_variance8x8_c(const uint8_t* src_ptr,
                                     int src_stride,
                                     int x_offset,
                                     int y_offset,
                                     const uint8_t* ref_ptr,
                                     int ref_stride,
                                     uint32_t* sse);
uint32_t vpx_sub_pixel_variance8x8_neon(const uint8_t* src_ptr,
                                        int src_stride,
                                        int x_offset,
                                        int y_offset,
                                        const uint8_t* ref_ptr,
                                        int ref_stride,
                                        uint32_t* sse);
RTCD_EXTERN uint32_t (*vpx_sub_pixel_variance8x8)(const uint8_t* src_ptr,
                                                  int src_stride,
                                                  int x_offset,
                                                  int y_offset,
                                                  const uint8_t* ref_ptr,
                                                  int ref_stride,
                                                  uint32_t* sse);

void vpx_subtract_block_c(int rows,
                          int cols,
                          int16_t* diff_ptr,
                          ptrdiff_t diff_stride,
                          const uint8_t* src_ptr,
                          ptrdiff_t src_stride,
                          const uint8_t* pred_ptr,
                          ptrdiff_t pred_stride);
void vpx_subtract_block_neon(int rows,
                             int cols,
                             int16_t* diff_ptr,
                             ptrdiff_t diff_stride,
                             const uint8_t* src_ptr,
                             ptrdiff_t src_stride,
                             const uint8_t* pred_ptr,
                             ptrdiff_t pred_stride);
RTCD_EXTERN void (*vpx_subtract_block)(int rows,
                                       int cols,
                                       int16_t* diff_ptr,
                                       ptrdiff_t diff_stride,
                                       const uint8_t* src_ptr,
                                       ptrdiff_t src_stride,
                                       const uint8_t* pred_ptr,
                                       ptrdiff_t pred_stride);

uint64_t vpx_sum_squares_2d_i16_c(const int16_t* src, int stride, int size);
uint64_t vpx_sum_squares_2d_i16_neon(const int16_t* src, int stride, int size);
RTCD_EXTERN uint64_t (*vpx_sum_squares_2d_i16)(const int16_t* src,
                                               int stride,
                                               int size);

void vpx_tm_predictor_16x16_c(uint8_t* dst,
                              ptrdiff_t stride,
                              const uint8_t* above,
                              const uint8_t* left);
void vpx_tm_predictor_16x16_neon(uint8_t* dst,
                                 ptrdiff_t stride,
                                 const uint8_t* above,
                                 const uint8_t* left);
RTCD_EXTERN void (*vpx_tm_predictor_16x16)(uint8_t* dst,
                                           ptrdiff_t stride,
                                           const uint8_t* above,
                                           const uint8_t* left);

void vpx_tm_predictor_32x32_c(uint8_t* dst,
                              ptrdiff_t stride,
                              const uint8_t* above,
                              const uint8_t* left);
void vpx_tm_predictor_32x32_neon(uint8_t* dst,
                                 ptrdiff_t stride,
                                 const uint8_t* above,
                                 const uint8_t* left);
RTCD_EXTERN void (*vpx_tm_predictor_32x32)(uint8_t* dst,
                                           ptrdiff_t stride,
                                           const uint8_t* above,
                                           const uint8_t* left);

void vpx_tm_predictor_4x4_c(uint8_t* dst,
                            ptrdiff_t stride,
                            const uint8_t* above,
                            const uint8_t* left);
void vpx_tm_predictor_4x4_neon(uint8_t* dst,
                               ptrdiff_t stride,
                               const uint8_t* above,
                               const uint8_t* left);
RTCD_EXTERN void (*vpx_tm_predictor_4x4)(uint8_t* dst,
                                         ptrdiff_t stride,
                                         const uint8_t* above,
                                         const uint8_t* left);

void vpx_tm_predictor_8x8_c(uint8_t* dst,
                            ptrdiff_t stride,
                            const uint8_t* above,
                            const uint8_t* left);
void vpx_tm_predictor_8x8_neon(uint8_t* dst,
                               ptrdiff_t stride,
                               const uint8_t* above,
                               const uint8_t* left);
RTCD_EXTERN void (*vpx_tm_predictor_8x8)(uint8_t* dst,
                                         ptrdiff_t stride,
                                         const uint8_t* above,
                                         const uint8_t* left);

void vpx_v_predictor_16x16_c(uint8_t* dst,
                             ptrdiff_t stride,
                             const uint8_t* above,
                             const uint8_t* left);
void vpx_v_predictor_16x16_neon(uint8_t* dst,
                                ptrdiff_t stride,
                                const uint8_t* above,
                                const uint8_t* left);
RTCD_EXTERN void (*vpx_v_predictor_16x16)(uint8_t* dst,
                                          ptrdiff_t stride,
                                          const uint8_t* above,
                                          const uint8_t* left);

void vpx_v_predictor_32x32_c(uint8_t* dst,
                             ptrdiff_t stride,
                             const uint8_t* above,
                             const uint8_t* left);
void vpx_v_predictor_32x32_neon(uint8_t* dst,
                                ptrdiff_t stride,
                                const uint8_t* above,
                                const uint8_t* left);
RTCD_EXTERN void (*vpx_v_predictor_32x32)(uint8_t* dst,
                                          ptrdiff_t stride,
                                          const uint8_t* above,
                                          const uint8_t* left);

void vpx_v_predictor_4x4_c(uint8_t* dst,
                           ptrdiff_t stride,
                           const uint8_t* above,
                           const uint8_t* left);
void vpx_v_predictor_4x4_neon(uint8_t* dst,
                              ptrdiff_t stride,
                              const uint8_t* above,
                              const uint8_t* left);
RTCD_EXTERN void (*vpx_v_predictor_4x4)(uint8_t* dst,
                                        ptrdiff_t stride,
                                        const uint8_t* above,
                                        const uint8_t* left);

void vpx_v_predictor_8x8_c(uint8_t* dst,
                           ptrdiff_t stride,
                           const uint8_t* above,
                           const uint8_t* left);
void vpx_v_predictor_8x8_neon(uint8_t* dst,
                              ptrdiff_t stride,
                              const uint8_t* above,
                              const uint8_t* left);
RTCD_EXTERN void (*vpx_v_predictor_8x8)(uint8_t* dst,
                                        ptrdiff_t stride,
                                        const uint8_t* above,
                                        const uint8_t* left);

unsigned int vpx_variance16x16_c(const uint8_t* src_ptr,
                                 int src_stride,
                                 const uint8_t* ref_ptr,
                                 int ref_stride,
                                 unsigned int* sse);
unsigned int vpx_variance16x16_neon(const uint8_t* src_ptr,
                                    int src_stride,
                                    const uint8_t* ref_ptr,
                                    int ref_stride,
                                    unsigned int* sse);
RTCD_EXTERN unsigned int (*vpx_variance16x16)(const uint8_t* src_ptr,
                                              int src_stride,
                                              const uint8_t* ref_ptr,
                                              int ref_stride,
                                              unsigned int* sse);

unsigned int vpx_variance16x32_c(const uint8_t* src_ptr,
                                 int src_stride,
                                 const uint8_t* ref_ptr,
                                 int ref_stride,
                                 unsigned int* sse);
unsigned int vpx_variance16x32_neon(const uint8_t* src_ptr,
                                    int src_stride,
                                    const uint8_t* ref_ptr,
                                    int ref_stride,
                                    unsigned int* sse);
RTCD_EXTERN unsigned int (*vpx_variance16x32)(const uint8_t* src_ptr,
                                              int src_stride,
                                              const uint8_t* ref_ptr,
                                              int ref_stride,
                                              unsigned int* sse);

unsigned int vpx_variance16x8_c(const uint8_t* src_ptr,
                                int src_stride,
                                const uint8_t* ref_ptr,
                                int ref_stride,
                                unsigned int* sse);
unsigned int vpx_variance16x8_neon(const uint8_t* src_ptr,
                                   int src_stride,
                                   const uint8_t* ref_ptr,
                                   int ref_stride,
                                   unsigned int* sse);
RTCD_EXTERN unsigned int (*vpx_variance16x8)(const uint8_t* src_ptr,
                                             int src_stride,
                                             const uint8_t* ref_ptr,
                                             int ref_stride,
                                             unsigned int* sse);

unsigned int vpx_variance32x16_c(const uint8_t* src_ptr,
                                 int src_stride,
                                 const uint8_t* ref_ptr,
                                 int ref_stride,
                                 unsigned int* sse);
unsigned int vpx_variance32x16_neon(const uint8_t* src_ptr,
                                    int src_stride,
                                    const uint8_t* ref_ptr,
                                    int ref_stride,
                                    unsigned int* sse);
RTCD_EXTERN unsigned int (*vpx_variance32x16)(const uint8_t* src_ptr,
                                              int src_stride,
                                              const uint8_t* ref_ptr,
                                              int ref_stride,
                                              unsigned int* sse);

unsigned int vpx_variance32x32_c(const uint8_t* src_ptr,
                                 int src_stride,
                                 const uint8_t* ref_ptr,
                                 int ref_stride,
                                 unsigned int* sse);
unsigned int vpx_variance32x32_neon(const uint8_t* src_ptr,
                                    int src_stride,
                                    const uint8_t* ref_ptr,
                                    int ref_stride,
                                    unsigned int* sse);
RTCD_EXTERN unsigned int (*vpx_variance32x32)(const uint8_t* src_ptr,
                                              int src_stride,
                                              const uint8_t* ref_ptr,
                                              int ref_stride,
                                              unsigned int* sse);

unsigned int vpx_variance32x64_c(const uint8_t* src_ptr,
                                 int src_stride,
                                 const uint8_t* ref_ptr,
                                 int ref_stride,
                                 unsigned int* sse);
unsigned int vpx_variance32x64_neon(const uint8_t* src_ptr,
                                    int src_stride,
                                    const uint8_t* ref_ptr,
                                    int ref_stride,
                                    unsigned int* sse);
RTCD_EXTERN unsigned int (*vpx_variance32x64)(const uint8_t* src_ptr,
                                              int src_stride,
                                              const uint8_t* ref_ptr,
                                              int ref_stride,
                                              unsigned int* sse);

unsigned int vpx_variance4x4_c(const uint8_t* src_ptr,
                               int src_stride,
                               const uint8_t* ref_ptr,
                               int ref_stride,
                               unsigned int* sse);
unsigned int vpx_variance4x4_neon(const uint8_t* src_ptr,
                                  int src_stride,
                                  const uint8_t* ref_ptr,
                                  int ref_stride,
                                  unsigned int* sse);
RTCD_EXTERN unsigned int (*vpx_variance4x4)(const uint8_t* src_ptr,
                                            int src_stride,
                                            const uint8_t* ref_ptr,
                                            int ref_stride,
                                            unsigned int* sse);

unsigned int vpx_variance4x8_c(const uint8_t* src_ptr,
                               int src_stride,
                               const uint8_t* ref_ptr,
                               int ref_stride,
                               unsigned int* sse);
unsigned int vpx_variance4x8_neon(const uint8_t* src_ptr,
                                  int src_stride,
                                  const uint8_t* ref_ptr,
                                  int ref_stride,
                                  unsigned int* sse);
RTCD_EXTERN unsigned int (*vpx_variance4x8)(const uint8_t* src_ptr,
                                            int src_stride,
                                            const uint8_t* ref_ptr,
                                            int ref_stride,
                                            unsigned int* sse);

unsigned int vpx_variance64x32_c(const uint8_t* src_ptr,
                                 int src_stride,
                                 const uint8_t* ref_ptr,
                                 int ref_stride,
                                 unsigned int* sse);
unsigned int vpx_variance64x32_neon(const uint8_t* src_ptr,
                                    int src_stride,
                                    const uint8_t* ref_ptr,
                                    int ref_stride,
                                    unsigned int* sse);
RTCD_EXTERN unsigned int (*vpx_variance64x32)(const uint8_t* src_ptr,
                                              int src_stride,
                                              const uint8_t* ref_ptr,
                                              int ref_stride,
                                              unsigned int* sse);

unsigned int vpx_variance64x64_c(const uint8_t* src_ptr,
                                 int src_stride,
                                 const uint8_t* ref_ptr,
                                 int ref_stride,
                                 unsigned int* sse);
unsigned int vpx_variance64x64_neon(const uint8_t* src_ptr,
                                    int src_stride,
                                    const uint8_t* ref_ptr,
                                    int ref_stride,
                                    unsigned int* sse);
RTCD_EXTERN unsigned int (*vpx_variance64x64)(const uint8_t* src_ptr,
                                              int src_stride,
                                              const uint8_t* ref_ptr,
                                              int ref_stride,
                                              unsigned int* sse);

unsigned int vpx_variance8x16_c(const uint8_t* src_ptr,
                                int src_stride,
                                const uint8_t* ref_ptr,
                                int ref_stride,
                                unsigned int* sse);
unsigned int vpx_variance8x16_neon(const uint8_t* src_ptr,
                                   int src_stride,
                                   const uint8_t* ref_ptr,
                                   int ref_stride,
                                   unsigned int* sse);
RTCD_EXTERN unsigned int (*vpx_variance8x16)(const uint8_t* src_ptr,
                                             int src_stride,
                                             const uint8_t* ref_ptr,
                                             int ref_stride,
                                             unsigned int* sse);

unsigned int vpx_variance8x4_c(const uint8_t* src_ptr,
                               int src_stride,
                               const uint8_t* ref_ptr,
                               int ref_stride,
                               unsigned int* sse);
unsigned int vpx_variance8x4_neon(const uint8_t* src_ptr,
                                  int src_stride,
                                  const uint8_t* ref_ptr,
                                  int ref_stride,
                                  unsigned int* sse);
RTCD_EXTERN unsigned int (*vpx_variance8x4)(const uint8_t* src_ptr,
                                            int src_stride,
                                            const uint8_t* ref_ptr,
                                            int ref_stride,
                                            unsigned int* sse);

unsigned int vpx_variance8x8_c(const uint8_t* src_ptr,
                               int src_stride,
                               const uint8_t* ref_ptr,
                               int ref_stride,
                               unsigned int* sse);
unsigned int vpx_variance8x8_neon(const uint8_t* src_ptr,
                                  int src_stride,
                                  const uint8_t* ref_ptr,
                                  int ref_stride,
                                  unsigned int* sse);
RTCD_EXTERN unsigned int (*vpx_variance8x8)(const uint8_t* src_ptr,
                                            int src_stride,
                                            const uint8_t* ref_ptr,
                                            int ref_stride,
                                            unsigned int* sse);

void vpx_ve_predictor_4x4_c(uint8_t* dst,
                            ptrdiff_t stride,
                            const uint8_t* above,
                            const uint8_t* left);
#define vpx_ve_predictor_4x4 vpx_ve_predictor_4x4_c

int vpx_vector_var_c(const int16_t* ref, const int16_t* src, const int bwl);
int vpx_vector_var_neon(const int16_t* ref, const int16_t* src, const int bwl);
RTCD_EXTERN int (*vpx_vector_var)(const int16_t* ref,
                                  const int16_t* src,
                                  const int bwl);

void vpx_dsp_rtcd(void);

#include "vpx_config.h"

#ifdef RTCD_C
#include "vpx_ports/arm.h"
static void setup_rtcd_internal(void) {
  int flags = arm_cpu_caps();

  (void)flags;

  vpx_avg_4x4 = vpx_avg_4x4_c;
  if (flags & HAS_NEON)
    vpx_avg_4x4 = vpx_avg_4x4_neon;
  vpx_avg_8x8 = vpx_avg_8x8_c;
  if (flags & HAS_NEON)
    vpx_avg_8x8 = vpx_avg_8x8_neon;
  vpx_comp_avg_pred = vpx_comp_avg_pred_c;
  if (flags & HAS_NEON)
    vpx_comp_avg_pred = vpx_comp_avg_pred_neon;
  vpx_convolve8 = vpx_convolve8_c;
  if (flags & HAS_NEON)
    vpx_convolve8 = vpx_convolve8_neon;
  vpx_convolve8_avg = vpx_convolve8_avg_c;
  if (flags & HAS_NEON)
    vpx_convolve8_avg = vpx_convolve8_avg_neon;
  vpx_convolve8_avg_horiz = vpx_convolve8_avg_horiz_c;
  if (flags & HAS_NEON)
    vpx_convolve8_avg_horiz = vpx_convolve8_avg_horiz_neon;
  vpx_convolve8_avg_vert = vpx_convolve8_avg_vert_c;
  if (flags & HAS_NEON)
    vpx_convolve8_avg_vert = vpx_convolve8_avg_vert_neon;
  vpx_convolve8_horiz = vpx_convolve8_horiz_c;
  if (flags & HAS_NEON)
    vpx_convolve8_horiz = vpx_convolve8_horiz_neon;
  vpx_convolve8_vert = vpx_convolve8_vert_c;
  if (flags & HAS_NEON)
    vpx_convolve8_vert = vpx_convolve8_vert_neon;
  vpx_convolve_avg = vpx_convolve_avg_c;
  if (flags & HAS_NEON)
    vpx_convolve_avg = vpx_convolve_avg_neon;
  vpx_convolve_copy = vpx_convolve_copy_c;
  if (flags & HAS_NEON)
    vpx_convolve_copy = vpx_convolve_copy_neon;
  vpx_d135_predictor_16x16 = vpx_d135_predictor_16x16_c;
  if (flags & HAS_NEON)
    vpx_d135_predictor_16x16 = vpx_d135_predictor_16x16_neon;
  vpx_d135_predictor_32x32 = vpx_d135_predictor_32x32_c;
  if (flags & HAS_NEON)
    vpx_d135_predictor_32x32 = vpx_d135_predictor_32x32_neon;
  vpx_d135_predictor_4x4 = vpx_d135_predictor_4x4_c;
  if (flags & HAS_NEON)
    vpx_d135_predictor_4x4 = vpx_d135_predictor_4x4_neon;
  vpx_d135_predictor_8x8 = vpx_d135_predictor_8x8_c;
  if (flags & HAS_NEON)
    vpx_d135_predictor_8x8 = vpx_d135_predictor_8x8_neon;
  vpx_d45_predictor_16x16 = vpx_d45_predictor_16x16_c;
  if (flags & HAS_NEON)
    vpx_d45_predictor_16x16 = vpx_d45_predictor_16x16_neon;
  vpx_d45_predictor_32x32 = vpx_d45_predictor_32x32_c;
  if (flags & HAS_NEON)
    vpx_d45_predictor_32x32 = vpx_d45_predictor_32x32_neon;
  vpx_d45_predictor_4x4 = vpx_d45_predictor_4x4_c;
  if (flags & HAS_NEON)
    vpx_d45_predictor_4x4 = vpx_d45_predictor_4x4_neon;
  vpx_d45_predictor_8x8 = vpx_d45_predictor_8x8_c;
  if (flags & HAS_NEON)
    vpx_d45_predictor_8x8 = vpx_d45_predictor_8x8_neon;
  vpx_dc_128_predictor_16x16 = vpx_dc_128_predictor_16x16_c;
  if (flags & HAS_NEON)
    vpx_dc_128_predictor_16x16 = vpx_dc_128_predictor_16x16_neon;
  vpx_dc_128_predictor_32x32 = vpx_dc_128_predictor_32x32_c;
  if (flags & HAS_NEON)
    vpx_dc_128_predictor_32x32 = vpx_dc_128_predictor_32x32_neon;
  vpx_dc_128_predictor_4x4 = vpx_dc_128_predictor_4x4_c;
  if (flags & HAS_NEON)
    vpx_dc_128_predictor_4x4 = vpx_dc_128_predictor_4x4_neon;
  vpx_dc_128_predictor_8x8 = vpx_dc_128_predictor_8x8_c;
  if (flags & HAS_NEON)
    vpx_dc_128_predictor_8x8 = vpx_dc_128_predictor_8x8_neon;
  vpx_dc_left_predictor_16x16 = vpx_dc_left_predictor_16x16_c;
  if (flags & HAS_NEON)
    vpx_dc_left_predictor_16x16 = vpx_dc_left_predictor_16x16_neon;
  vpx_dc_left_predictor_32x32 = vpx_dc_left_predictor_32x32_c;
  if (flags & HAS_NEON)
    vpx_dc_left_predictor_32x32 = vpx_dc_left_predictor_32x32_neon;
  vpx_dc_left_predictor_4x4 = vpx_dc_left_predictor_4x4_c;
  if (flags & HAS_NEON)
    vpx_dc_left_predictor_4x4 = vpx_dc_left_predictor_4x4_neon;
  vpx_dc_left_predictor_8x8 = vpx_dc_left_predictor_8x8_c;
  if (flags & HAS_NEON)
    vpx_dc_left_predictor_8x8 = vpx_dc_left_predictor_8x8_neon;
  vpx_dc_predictor_16x16 = vpx_dc_predictor_16x16_c;
  if (flags & HAS_NEON)
    vpx_dc_predictor_16x16 = vpx_dc_predictor_16x16_neon;
  vpx_dc_predictor_32x32 = vpx_dc_predictor_32x32_c;
  if (flags & HAS_NEON)
    vpx_dc_predictor_32x32 = vpx_dc_predictor_32x32_neon;
  vpx_dc_predictor_4x4 = vpx_dc_predictor_4x4_c;
  if (flags & HAS_NEON)
    vpx_dc_predictor_4x4 = vpx_dc_predictor_4x4_neon;
  vpx_dc_predictor_8x8 = vpx_dc_predictor_8x8_c;
  if (flags & HAS_NEON)
    vpx_dc_predictor_8x8 = vpx_dc_predictor_8x8_neon;
  vpx_dc_top_predictor_16x16 = vpx_dc_top_predictor_16x16_c;
  if (flags & HAS_NEON)
    vpx_dc_top_predictor_16x16 = vpx_dc_top_predictor_16x16_neon;
  vpx_dc_top_predictor_32x32 = vpx_dc_top_predictor_32x32_c;
  if (flags & HAS_NEON)
    vpx_dc_top_predictor_32x32 = vpx_dc_top_predictor_32x32_neon;
  vpx_dc_top_predictor_4x4 = vpx_dc_top_predictor_4x4_c;
  if (flags & HAS_NEON)
    vpx_dc_top_predictor_4x4 = vpx_dc_top_predictor_4x4_neon;
  vpx_dc_top_predictor_8x8 = vpx_dc_top_predictor_8x8_c;
  if (flags & HAS_NEON)
    vpx_dc_top_predictor_8x8 = vpx_dc_top_predictor_8x8_neon;
  vpx_fdct16x16 = vpx_fdct16x16_c;
  if (flags & HAS_NEON)
    vpx_fdct16x16 = vpx_fdct16x16_neon;
  vpx_fdct16x16_1 = vpx_fdct16x16_1_c;
  if (flags & HAS_NEON)
    vpx_fdct16x16_1 = vpx_fdct16x16_1_neon;
  vpx_fdct32x32 = vpx_fdct32x32_c;
  if (flags & HAS_NEON)
    vpx_fdct32x32 = vpx_fdct32x32_neon;
  vpx_fdct32x32_1 = vpx_fdct32x32_1_c;
  if (flags & HAS_NEON)
    vpx_fdct32x32_1 = vpx_fdct32x32_1_neon;
  vpx_fdct32x32_rd = vpx_fdct32x32_rd_c;
  if (flags & HAS_NEON)
    vpx_fdct32x32_rd = vpx_fdct32x32_rd_neon;
  vpx_fdct4x4 = vpx_fdct4x4_c;
  if (flags & HAS_NEON)
    vpx_fdct4x4 = vpx_fdct4x4_neon;
  vpx_fdct4x4_1 = vpx_fdct4x4_1_c;
  if (flags & HAS_NEON)
    vpx_fdct4x4_1 = vpx_fdct4x4_1_neon;
  vpx_fdct8x8 = vpx_fdct8x8_c;
  if (flags & HAS_NEON)
    vpx_fdct8x8 = vpx_fdct8x8_neon;
  vpx_fdct8x8_1 = vpx_fdct8x8_1_c;
  if (flags & HAS_NEON)
    vpx_fdct8x8_1 = vpx_fdct8x8_1_neon;
  vpx_get16x16var = vpx_get16x16var_c;
  if (flags & HAS_NEON)
    vpx_get16x16var = vpx_get16x16var_neon;
  vpx_get4x4sse_cs = vpx_get4x4sse_cs_c;
  if (flags & HAS_NEON)
    vpx_get4x4sse_cs = vpx_get4x4sse_cs_neon;
  vpx_get8x8var = vpx_get8x8var_c;
  if (flags & HAS_NEON)
    vpx_get8x8var = vpx_get8x8var_neon;
  vpx_h_predictor_16x16 = vpx_h_predictor_16x16_c;
  if (flags & HAS_NEON)
    vpx_h_predictor_16x16 = vpx_h_predictor_16x16_neon;
  vpx_h_predictor_32x32 = vpx_h_predictor_32x32_c;
  if (flags & HAS_NEON)
    vpx_h_predictor_32x32 = vpx_h_predictor_32x32_neon;
  vpx_h_predictor_4x4 = vpx_h_predictor_4x4_c;
  if (flags & HAS_NEON)
    vpx_h_predictor_4x4 = vpx_h_predictor_4x4_neon;
  vpx_h_predictor_8x8 = vpx_h_predictor_8x8_c;
  if (flags & HAS_NEON)
    vpx_h_predictor_8x8 = vpx_h_predictor_8x8_neon;
  vpx_hadamard_16x16 = vpx_hadamard_16x16_c;
  if (flags & HAS_NEON)
    vpx_hadamard_16x16 = vpx_hadamard_16x16_neon;
  vpx_hadamard_8x8 = vpx_hadamard_8x8_c;
  if (flags & HAS_NEON)
    vpx_hadamard_8x8 = vpx_hadamard_8x8_neon;
  vpx_idct16x16_10_add = vpx_idct16x16_10_add_c;
  if (flags & HAS_NEON)
    vpx_idct16x16_10_add = vpx_idct16x16_10_add_neon;
  vpx_idct16x16_1_add = vpx_idct16x16_1_add_c;
  if (flags & HAS_NEON)
    vpx_idct16x16_1_add = vpx_idct16x16_1_add_neon;
  vpx_idct16x16_256_add = vpx_idct16x16_256_add_c;
  if (flags & HAS_NEON)
    vpx_idct16x16_256_add = vpx_idct16x16_256_add_neon;
  vpx_idct16x16_38_add = vpx_idct16x16_38_add_c;
  if (flags & HAS_NEON)
    vpx_idct16x16_38_add = vpx_idct16x16_38_add_neon;
  vpx_idct32x32_1024_add = vpx_idct32x32_1024_add_c;
  if (flags & HAS_NEON)
    vpx_idct32x32_1024_add = vpx_idct32x32_1024_add_neon;
  vpx_idct32x32_135_add = vpx_idct32x32_135_add_c;
  if (flags & HAS_NEON)
    vpx_idct32x32_135_add = vpx_idct32x32_135_add_neon;
  vpx_idct32x32_1_add = vpx_idct32x32_1_add_c;
  if (flags & HAS_NEON)
    vpx_idct32x32_1_add = vpx_idct32x32_1_add_neon;
  vpx_idct32x32_34_add = vpx_idct32x32_34_add_c;
  if (flags & HAS_NEON)
    vpx_idct32x32_34_add = vpx_idct32x32_34_add_neon;
  vpx_idct4x4_16_add = vpx_idct4x4_16_add_c;
  if (flags & HAS_NEON)
    vpx_idct4x4_16_add = vpx_idct4x4_16_add_neon;
  vpx_idct4x4_1_add = vpx_idct4x4_1_add_c;
  if (flags & HAS_NEON)
    vpx_idct4x4_1_add = vpx_idct4x4_1_add_neon;
  vpx_idct8x8_12_add = vpx_idct8x8_12_add_c;
  if (flags & HAS_NEON)
    vpx_idct8x8_12_add = vpx_idct8x8_12_add_neon;
  vpx_idct8x8_1_add = vpx_idct8x8_1_add_c;
  if (flags & HAS_NEON)
    vpx_idct8x8_1_add = vpx_idct8x8_1_add_neon;
  vpx_idct8x8_64_add = vpx_idct8x8_64_add_c;
  if (flags & HAS_NEON)
    vpx_idct8x8_64_add = vpx_idct8x8_64_add_neon;
  vpx_int_pro_col = vpx_int_pro_col_c;
  if (flags & HAS_NEON)
    vpx_int_pro_col = vpx_int_pro_col_neon;
  vpx_int_pro_row = vpx_int_pro_row_c;
  if (flags & HAS_NEON)
    vpx_int_pro_row = vpx_int_pro_row_neon;
  vpx_lpf_horizontal_16 = vpx_lpf_horizontal_16_c;
  if (flags & HAS_NEON)
    vpx_lpf_horizontal_16 = vpx_lpf_horizontal_16_neon;
  vpx_lpf_horizontal_16_dual = vpx_lpf_horizontal_16_dual_c;
  if (flags & HAS_NEON)
    vpx_lpf_horizontal_16_dual = vpx_lpf_horizontal_16_dual_neon;
  vpx_lpf_horizontal_4 = vpx_lpf_horizontal_4_c;
  if (flags & HAS_NEON)
    vpx_lpf_horizontal_4 = vpx_lpf_horizontal_4_neon;
  vpx_lpf_horizontal_4_dual = vpx_lpf_horizontal_4_dual_c;
  if (flags & HAS_NEON)
    vpx_lpf_horizontal_4_dual = vpx_lpf_horizontal_4_dual_neon;
  vpx_lpf_horizontal_8 = vpx_lpf_horizontal_8_c;
  if (flags & HAS_NEON)
    vpx_lpf_horizontal_8 = vpx_lpf_horizontal_8_neon;
  vpx_lpf_horizontal_8_dual = vpx_lpf_horizontal_8_dual_c;
  if (flags & HAS_NEON)
    vpx_lpf_horizontal_8_dual = vpx_lpf_horizontal_8_dual_neon;
  vpx_lpf_vertical_16 = vpx_lpf_vertical_16_c;
  if (flags & HAS_NEON)
    vpx_lpf_vertical_16 = vpx_lpf_vertical_16_neon;
  vpx_lpf_vertical_16_dual = vpx_lpf_vertical_16_dual_c;
  if (flags & HAS_NEON)
    vpx_lpf_vertical_16_dual = vpx_lpf_vertical_16_dual_neon;
  vpx_lpf_vertical_4 = vpx_lpf_vertical_4_c;
  if (flags & HAS_NEON)
    vpx_lpf_vertical_4 = vpx_lpf_vertical_4_neon;
  vpx_lpf_vertical_4_dual = vpx_lpf_vertical_4_dual_c;
  if (flags & HAS_NEON)
    vpx_lpf_vertical_4_dual = vpx_lpf_vertical_4_dual_neon;
  vpx_lpf_vertical_8 = vpx_lpf_vertical_8_c;
  if (flags & HAS_NEON)
    vpx_lpf_vertical_8 = vpx_lpf_vertical_8_neon;
  vpx_lpf_vertical_8_dual = vpx_lpf_vertical_8_dual_c;
  if (flags & HAS_NEON)
    vpx_lpf_vertical_8_dual = vpx_lpf_vertical_8_dual_neon;
  vpx_mbpost_proc_across_ip = vpx_mbpost_proc_across_ip_c;
  if (flags & HAS_NEON)
    vpx_mbpost_proc_across_ip = vpx_mbpost_proc_across_ip_neon;
  vpx_mbpost_proc_down = vpx_mbpost_proc_down_c;
  if (flags & HAS_NEON)
    vpx_mbpost_proc_down = vpx_mbpost_proc_down_neon;
  vpx_minmax_8x8 = vpx_minmax_8x8_c;
  if (flags & HAS_NEON)
    vpx_minmax_8x8 = vpx_minmax_8x8_neon;
  vpx_mse16x16 = vpx_mse16x16_c;
  if (flags & HAS_NEON)
    vpx_mse16x16 = vpx_mse16x16_neon;
  vpx_post_proc_down_and_across_mb_row = vpx_post_proc_down_and_across_mb_row_c;
  if (flags & HAS_NEON)
    vpx_post_proc_down_and_across_mb_row =
        vpx_post_proc_down_and_across_mb_row_neon;
  vpx_quantize_b = vpx_quantize_b_c;
  if (flags & HAS_NEON)
    vpx_quantize_b = vpx_quantize_b_neon;
  vpx_quantize_b_32x32 = vpx_quantize_b_32x32_c;
  if (flags & HAS_NEON)
    vpx_quantize_b_32x32 = vpx_quantize_b_32x32_neon;
  vpx_sad16x16 = vpx_sad16x16_c;
  if (flags & HAS_NEON)
    vpx_sad16x16 = vpx_sad16x16_neon;
  vpx_sad16x16_avg = vpx_sad16x16_avg_c;
  if (flags & HAS_NEON)
    vpx_sad16x16_avg = vpx_sad16x16_avg_neon;
  vpx_sad16x16x4d = vpx_sad16x16x4d_c;
  if (flags & HAS_NEON)
    vpx_sad16x16x4d = vpx_sad16x16x4d_neon;
  vpx_sad16x32 = vpx_sad16x32_c;
  if (flags & HAS_NEON)
    vpx_sad16x32 = vpx_sad16x32_neon;
  vpx_sad16x32_avg = vpx_sad16x32_avg_c;
  if (flags & HAS_NEON)
    vpx_sad16x32_avg = vpx_sad16x32_avg_neon;
  vpx_sad16x32x4d = vpx_sad16x32x4d_c;
  if (flags & HAS_NEON)
    vpx_sad16x32x4d = vpx_sad16x32x4d_neon;
  vpx_sad16x8 = vpx_sad16x8_c;
  if (flags & HAS_NEON)
    vpx_sad16x8 = vpx_sad16x8_neon;
  vpx_sad16x8_avg = vpx_sad16x8_avg_c;
  if (flags & HAS_NEON)
    vpx_sad16x8_avg = vpx_sad16x8_avg_neon;
  vpx_sad16x8x4d = vpx_sad16x8x4d_c;
  if (flags & HAS_NEON)
    vpx_sad16x8x4d = vpx_sad16x8x4d_neon;
  vpx_sad32x16 = vpx_sad32x16_c;
  if (flags & HAS_NEON)
    vpx_sad32x16 = vpx_sad32x16_neon;
  vpx_sad32x16_avg = vpx_sad32x16_avg_c;
  if (flags & HAS_NEON)
    vpx_sad32x16_avg = vpx_sad32x16_avg_neon;
  vpx_sad32x16x4d = vpx_sad32x16x4d_c;
  if (flags & HAS_NEON)
    vpx_sad32x16x4d = vpx_sad32x16x4d_neon;
  vpx_sad32x32 = vpx_sad32x32_c;
  if (flags & HAS_NEON)
    vpx_sad32x32 = vpx_sad32x32_neon;
  vpx_sad32x32_avg = vpx_sad32x32_avg_c;
  if (flags & HAS_NEON)
    vpx_sad32x32_avg = vpx_sad32x32_avg_neon;
  vpx_sad32x32x4d = vpx_sad32x32x4d_c;
  if (flags & HAS_NEON)
    vpx_sad32x32x4d = vpx_sad32x32x4d_neon;
  vpx_sad32x64 = vpx_sad32x64_c;
  if (flags & HAS_NEON)
    vpx_sad32x64 = vpx_sad32x64_neon;
  vpx_sad32x64_avg = vpx_sad32x64_avg_c;
  if (flags & HAS_NEON)
    vpx_sad32x64_avg = vpx_sad32x64_avg_neon;
  vpx_sad32x64x4d = vpx_sad32x64x4d_c;
  if (flags & HAS_NEON)
    vpx_sad32x64x4d = vpx_sad32x64x4d_neon;
  vpx_sad4x4 = vpx_sad4x4_c;
  if (flags & HAS_NEON)
    vpx_sad4x4 = vpx_sad4x4_neon;
  vpx_sad4x4_avg = vpx_sad4x4_avg_c;
  if (flags & HAS_NEON)
    vpx_sad4x4_avg = vpx_sad4x4_avg_neon;
  vpx_sad4x4x4d = vpx_sad4x4x4d_c;
  if (flags & HAS_NEON)
    vpx_sad4x4x4d = vpx_sad4x4x4d_neon;
  vpx_sad4x8 = vpx_sad4x8_c;
  if (flags & HAS_NEON)
    vpx_sad4x8 = vpx_sad4x8_neon;
  vpx_sad4x8_avg = vpx_sad4x8_avg_c;
  if (flags & HAS_NEON)
    vpx_sad4x8_avg = vpx_sad4x8_avg_neon;
  vpx_sad4x8x4d = vpx_sad4x8x4d_c;
  if (flags & HAS_NEON)
    vpx_sad4x8x4d = vpx_sad4x8x4d_neon;
  vpx_sad64x32 = vpx_sad64x32_c;
  if (flags & HAS_NEON)
    vpx_sad64x32 = vpx_sad64x32_neon;
  vpx_sad64x32_avg = vpx_sad64x32_avg_c;
  if (flags & HAS_NEON)
    vpx_sad64x32_avg = vpx_sad64x32_avg_neon;
  vpx_sad64x32x4d = vpx_sad64x32x4d_c;
  if (flags & HAS_NEON)
    vpx_sad64x32x4d = vpx_sad64x32x4d_neon;
  vpx_sad64x64 = vpx_sad64x64_c;
  if (flags & HAS_NEON)
    vpx_sad64x64 = vpx_sad64x64_neon;
  vpx_sad64x64_avg = vpx_sad64x64_avg_c;
  if (flags & HAS_NEON)
    vpx_sad64x64_avg = vpx_sad64x64_avg_neon;
  vpx_sad64x64x4d = vpx_sad64x64x4d_c;
  if (flags & HAS_NEON)
    vpx_sad64x64x4d = vpx_sad64x64x4d_neon;
  vpx_sad8x16 = vpx_sad8x16_c;
  if (flags & HAS_NEON)
    vpx_sad8x16 = vpx_sad8x16_neon;
  vpx_sad8x16_avg = vpx_sad8x16_avg_c;
  if (flags & HAS_NEON)
    vpx_sad8x16_avg = vpx_sad8x16_avg_neon;
  vpx_sad8x16x4d = vpx_sad8x16x4d_c;
  if (flags & HAS_NEON)
    vpx_sad8x16x4d = vpx_sad8x16x4d_neon;
  vpx_sad8x4 = vpx_sad8x4_c;
  if (flags & HAS_NEON)
    vpx_sad8x4 = vpx_sad8x4_neon;
  vpx_sad8x4_avg = vpx_sad8x4_avg_c;
  if (flags & HAS_NEON)
    vpx_sad8x4_avg = vpx_sad8x4_avg_neon;
  vpx_sad8x4x4d = vpx_sad8x4x4d_c;
  if (flags & HAS_NEON)
    vpx_sad8x4x4d = vpx_sad8x4x4d_neon;
  vpx_sad8x8 = vpx_sad8x8_c;
  if (flags & HAS_NEON)
    vpx_sad8x8 = vpx_sad8x8_neon;
  vpx_sad8x8_avg = vpx_sad8x8_avg_c;
  if (flags & HAS_NEON)
    vpx_sad8x8_avg = vpx_sad8x8_avg_neon;
  vpx_sad8x8x4d = vpx_sad8x8x4d_c;
  if (flags & HAS_NEON)
    vpx_sad8x8x4d = vpx_sad8x8x4d_neon;
  vpx_satd = vpx_satd_c;
  if (flags & HAS_NEON)
    vpx_satd = vpx_satd_neon;
  vpx_scaled_2d = vpx_scaled_2d_c;
  if (flags & HAS_NEON)
    vpx_scaled_2d = vpx_scaled_2d_neon;
  vpx_sub_pixel_avg_variance16x16 = vpx_sub_pixel_avg_variance16x16_c;
  if (flags & HAS_NEON)
    vpx_sub_pixel_avg_variance16x16 = vpx_sub_pixel_avg_variance16x16_neon;
  vpx_sub_pixel_avg_variance16x32 = vpx_sub_pixel_avg_variance16x32_c;
  if (flags & HAS_NEON)
    vpx_sub_pixel_avg_variance16x32 = vpx_sub_pixel_avg_variance16x32_neon;
  vpx_sub_pixel_avg_variance16x8 = vpx_sub_pixel_avg_variance16x8_c;
  if (flags & HAS_NEON)
    vpx_sub_pixel_avg_variance16x8 = vpx_sub_pixel_avg_variance16x8_neon;
  vpx_sub_pixel_avg_variance32x16 = vpx_sub_pixel_avg_variance32x16_c;
  if (flags & HAS_NEON)
    vpx_sub_pixel_avg_variance32x16 = vpx_sub_pixel_avg_variance32x16_neon;
  vpx_sub_pixel_avg_variance32x32 = vpx_sub_pixel_avg_variance32x32_c;
  if (flags & HAS_NEON)
    vpx_sub_pixel_avg_variance32x32 = vpx_sub_pixel_avg_variance32x32_neon;
  vpx_sub_pixel_avg_variance32x64 = vpx_sub_pixel_avg_variance32x64_c;
  if (flags & HAS_NEON)
    vpx_sub_pixel_avg_variance32x64 = vpx_sub_pixel_avg_variance32x64_neon;
  vpx_sub_pixel_avg_variance4x4 = vpx_sub_pixel_avg_variance4x4_c;
  if (flags & HAS_NEON)
    vpx_sub_pixel_avg_variance4x4 = vpx_sub_pixel_avg_variance4x4_neon;
  vpx_sub_pixel_avg_variance4x8 = vpx_sub_pixel_avg_variance4x8_c;
  if (flags & HAS_NEON)
    vpx_sub_pixel_avg_variance4x8 = vpx_sub_pixel_avg_variance4x8_neon;
  vpx_sub_pixel_avg_variance64x32 = vpx_sub_pixel_avg_variance64x32_c;
  if (flags & HAS_NEON)
    vpx_sub_pixel_avg_variance64x32 = vpx_sub_pixel_avg_variance64x32_neon;
  vpx_sub_pixel_avg_variance64x64 = vpx_sub_pixel_avg_variance64x64_c;
  if (flags & HAS_NEON)
    vpx_sub_pixel_avg_variance64x64 = vpx_sub_pixel_avg_variance64x64_neon;
  vpx_sub_pixel_avg_variance8x16 = vpx_sub_pixel_avg_variance8x16_c;
  if (flags & HAS_NEON)
    vpx_sub_pixel_avg_variance8x16 = vpx_sub_pixel_avg_variance8x16_neon;
  vpx_sub_pixel_avg_variance8x4 = vpx_sub_pixel_avg_variance8x4_c;
  if (flags & HAS_NEON)
    vpx_sub_pixel_avg_variance8x4 = vpx_sub_pixel_avg_variance8x4_neon;
  vpx_sub_pixel_avg_variance8x8 = vpx_sub_pixel_avg_variance8x8_c;
  if (flags & HAS_NEON)
    vpx_sub_pixel_avg_variance8x8 = vpx_sub_pixel_avg_variance8x8_neon;
  vpx_sub_pixel_variance16x16 = vpx_sub_pixel_variance16x16_c;
  if (flags & HAS_NEON)
    vpx_sub_pixel_variance16x16 = vpx_sub_pixel_variance16x16_neon;
  vpx_sub_pixel_variance16x32 = vpx_sub_pixel_variance16x32_c;
  if (flags & HAS_NEON)
    vpx_sub_pixel_variance16x32 = vpx_sub_pixel_variance16x32_neon;
  vpx_sub_pixel_variance16x8 = vpx_sub_pixel_variance16x8_c;
  if (flags & HAS_NEON)
    vpx_sub_pixel_variance16x8 = vpx_sub_pixel_variance16x8_neon;
  vpx_sub_pixel_variance32x16 = vpx_sub_pixel_variance32x16_c;
  if (flags & HAS_NEON)
    vpx_sub_pixel_variance32x16 = vpx_sub_pixel_variance32x16_neon;
  vpx_sub_pixel_variance32x32 = vpx_sub_pixel_variance32x32_c;
  if (flags & HAS_NEON)
    vpx_sub_pixel_variance32x32 = vpx_sub_pixel_variance32x32_neon;
  vpx_sub_pixel_variance32x64 = vpx_sub_pixel_variance32x64_c;
  if (flags & HAS_NEON)
    vpx_sub_pixel_variance32x64 = vpx_sub_pixel_variance32x64_neon;
  vpx_sub_pixel_variance4x4 = vpx_sub_pixel_variance4x4_c;
  if (flags & HAS_NEON)
    vpx_sub_pixel_variance4x4 = vpx_sub_pixel_variance4x4_neon;
  vpx_sub_pixel_variance4x8 = vpx_sub_pixel_variance4x8_c;
  if (flags & HAS_NEON)
    vpx_sub_pixel_variance4x8 = vpx_sub_pixel_variance4x8_neon;
  vpx_sub_pixel_variance64x32 = vpx_sub_pixel_variance64x32_c;
  if (flags & HAS_NEON)
    vpx_sub_pixel_variance64x32 = vpx_sub_pixel_variance64x32_neon;
  vpx_sub_pixel_variance64x64 = vpx_sub_pixel_variance64x64_c;
  if (flags & HAS_NEON)
    vpx_sub_pixel_variance64x64 = vpx_sub_pixel_variance64x64_neon;
  vpx_sub_pixel_variance8x16 = vpx_sub_pixel_variance8x16_c;
  if (flags & HAS_NEON)
    vpx_sub_pixel_variance8x16 = vpx_sub_pixel_variance8x16_neon;
  vpx_sub_pixel_variance8x4 = vpx_sub_pixel_variance8x4_c;
  if (flags & HAS_NEON)
    vpx_sub_pixel_variance8x4 = vpx_sub_pixel_variance8x4_neon;
  vpx_sub_pixel_variance8x8 = vpx_sub_pixel_variance8x8_c;
  if (flags & HAS_NEON)
    vpx_sub_pixel_variance8x8 = vpx_sub_pixel_variance8x8_neon;
  vpx_subtract_block = vpx_subtract_block_c;
  if (flags & HAS_NEON)
    vpx_subtract_block = vpx_subtract_block_neon;
  vpx_sum_squares_2d_i16 = vpx_sum_squares_2d_i16_c;
  if (flags & HAS_NEON)
    vpx_sum_squares_2d_i16 = vpx_sum_squares_2d_i16_neon;
  vpx_tm_predictor_16x16 = vpx_tm_predictor_16x16_c;
  if (flags & HAS_NEON)
    vpx_tm_predictor_16x16 = vpx_tm_predictor_16x16_neon;
  vpx_tm_predictor_32x32 = vpx_tm_predictor_32x32_c;
  if (flags & HAS_NEON)
    vpx_tm_predictor_32x32 = vpx_tm_predictor_32x32_neon;
  vpx_tm_predictor_4x4 = vpx_tm_predictor_4x4_c;
  if (flags & HAS_NEON)
    vpx_tm_predictor_4x4 = vpx_tm_predictor_4x4_neon;
  vpx_tm_predictor_8x8 = vpx_tm_predictor_8x8_c;
  if (flags & HAS_NEON)
    vpx_tm_predictor_8x8 = vpx_tm_predictor_8x8_neon;
  vpx_v_predictor_16x16 = vpx_v_predictor_16x16_c;
  if (flags & HAS_NEON)
    vpx_v_predictor_16x16 = vpx_v_predictor_16x16_neon;
  vpx_v_predictor_32x32 = vpx_v_predictor_32x32_c;
  if (flags & HAS_NEON)
    vpx_v_predictor_32x32 = vpx_v_predictor_32x32_neon;
  vpx_v_predictor_4x4 = vpx_v_predictor_4x4_c;
  if (flags & HAS_NEON)
    vpx_v_predictor_4x4 = vpx_v_predictor_4x4_neon;
  vpx_v_predictor_8x8 = vpx_v_predictor_8x8_c;
  if (flags & HAS_NEON)
    vpx_v_predictor_8x8 = vpx_v_predictor_8x8_neon;
  vpx_variance16x16 = vpx_variance16x16_c;
  if (flags & HAS_NEON)
    vpx_variance16x16 = vpx_variance16x16_neon;
  vpx_variance16x32 = vpx_variance16x32_c;
  if (flags & HAS_NEON)
    vpx_variance16x32 = vpx_variance16x32_neon;
  vpx_variance16x8 = vpx_variance16x8_c;
  if (flags & HAS_NEON)
    vpx_variance16x8 = vpx_variance16x8_neon;
  vpx_variance32x16 = vpx_variance32x16_c;
  if (flags & HAS_NEON)
    vpx_variance32x16 = vpx_variance32x16_neon;
  vpx_variance32x32 = vpx_variance32x32_c;
  if (flags & HAS_NEON)
    vpx_variance32x32 = vpx_variance32x32_neon;
  vpx_variance32x64 = vpx_variance32x64_c;
  if (flags & HAS_NEON)
    vpx_variance32x64 = vpx_variance32x64_neon;
  vpx_variance4x4 = vpx_variance4x4_c;
  if (flags & HAS_NEON)
    vpx_variance4x4 = vpx_variance4x4_neon;
  vpx_variance4x8 = vpx_variance4x8_c;
  if (flags & HAS_NEON)
    vpx_variance4x8 = vpx_variance4x8_neon;
  vpx_variance64x32 = vpx_variance64x32_c;
  if (flags & HAS_NEON)
    vpx_variance64x32 = vpx_variance64x32_neon;
  vpx_variance64x64 = vpx_variance64x64_c;
  if (flags & HAS_NEON)
    vpx_variance64x64 = vpx_variance64x64_neon;
  vpx_variance8x16 = vpx_variance8x16_c;
  if (flags & HAS_NEON)
    vpx_variance8x16 = vpx_variance8x16_neon;
  vpx_variance8x4 = vpx_variance8x4_c;
  if (flags & HAS_NEON)
    vpx_variance8x4 = vpx_variance8x4_neon;
  vpx_variance8x8 = vpx_variance8x8_c;
  if (flags & HAS_NEON)
    vpx_variance8x8 = vpx_variance8x8_neon;
  vpx_vector_var = vpx_vector_var_c;
  if (flags & HAS_NEON)
    vpx_vector_var = vpx_vector_var_neon;
}
#endif

#ifdef __cplusplus
}  // extern "C"
#endif

#endif
