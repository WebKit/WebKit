;
;  Copyright (c) 2013 The WebM project authors. All Rights Reserved.
;
;  Use of this source code is governed by a BSD-style license
;  that can be found in the LICENSE file in the root of the source
;  tree. An additional intellectual property rights grant can be found
;  in the file PATENTS.  All contributing project authors may
;  be found in the AUTHORS file in the root of the source tree.
;


    ; These functions are only valid when:
    ; x_step_q4 == 16
    ; w%4 == 0
    ; h%4 == 0
    ; taps == 8
    ; VP9_FILTER_WEIGHT == 128
    ; VP9_FILTER_SHIFT == 7

    EXPORT  |vpx_convolve8_avg_horiz_neon|
    EXPORT  |vpx_convolve8_avg_vert_neon|
    ARM
    REQUIRE8
    PRESERVE8

    AREA ||.text||, CODE, READONLY, ALIGN=2

    ; Multiply and accumulate by q0
    MACRO
    MULTIPLY_BY_Q0 $dst, $src0, $src1, $src2, $src3, $src4, $src5, $src6, $src7
    vmull.s16 $dst, $src0, d0[0]
    vmlal.s16 $dst, $src1, d0[1]
    vmlal.s16 $dst, $src2, d0[2]
    vmlal.s16 $dst, $src3, d0[3]
    vmlal.s16 $dst, $src4, d1[0]
    vmlal.s16 $dst, $src5, d1[1]
    vmlal.s16 $dst, $src6, d1[2]
    vmlal.s16 $dst, $src7, d1[3]
    MEND

; r0    const uint8_t *src
; r1    int src_stride
; r2    uint8_t *dst
; r3    int dst_stride
; sp[]const int16_t *filter
; sp[]int x0_q4
; sp[]int x_step_q4 ; unused
; sp[]int y0_q4
; sp[]int y_step_q4 ; unused
; sp[]int w
; sp[]int h

|vpx_convolve8_avg_horiz_neon| PROC
    push            {r4-r10, lr}

    sub             r0, r0, #3              ; adjust for taps

    ldrd            r4, r5, [sp, #32]       ; filter, x0_q4
    add             r4, r5, lsl #4
    ldrd            r6, r7, [sp, #52]       ; w, h

    vld1.s16        {q0}, [r4]              ; filter

    sub             r8, r1, r1, lsl #2      ; -src_stride * 3
    add             r8, r8, #4              ; -src_stride * 3 + 4

    sub             r4, r3, r3, lsl #2      ; -dst_stride * 3
    add             r4, r4, #4              ; -dst_stride * 3 + 4

    rsb             r9, r6, r1, lsl #2      ; reset src for outer loop
    sub             r9, r9, #7
    rsb             r12, r6, r3, lsl #2     ; reset dst for outer loop

    mov             r10, r6                 ; w loop counter

vpx_convolve8_avg_loop_horiz_v
    vld1.8          {d24}, [r0], r1
    vld1.8          {d25}, [r0], r1
    vld1.8          {d26}, [r0], r1
    vld1.8          {d27}, [r0], r8

    vtrn.16         q12, q13
    vtrn.8          d24, d25
    vtrn.8          d26, d27

    pld             [r0, r1, lsl #2]

    vmovl.u8        q8, d24
    vmovl.u8        q9, d25
    vmovl.u8        q10, d26
    vmovl.u8        q11, d27

    ; save a few instructions in the inner loop
    vswp            d17, d18
    vmov            d23, d21

    add             r0, r0, #3

vpx_convolve8_avg_loop_horiz
    add             r5, r0, #64

    vld1.32         {d28[]}, [r0], r1
    vld1.32         {d29[]}, [r0], r1
    vld1.32         {d31[]}, [r0], r1
    vld1.32         {d30[]}, [r0], r8

    pld             [r5]

    vtrn.16         d28, d31
    vtrn.16         d29, d30
    vtrn.8          d28, d29
    vtrn.8          d31, d30

    pld             [r5, r1]

    ; extract to s16
    vtrn.32         q14, q15
    vmovl.u8        q12, d28
    vmovl.u8        q13, d29

    pld             [r5, r1, lsl #1]

    ; slightly out of order load to match the existing data
    vld1.u32        {d6[0]}, [r2], r3
    vld1.u32        {d7[0]}, [r2], r3
    vld1.u32        {d6[1]}, [r2], r3
    vld1.u32        {d7[1]}, [r2], r3

    sub             r2, r2, r3, lsl #2      ; reset for store

    ; src[] * filter
    MULTIPLY_BY_Q0  q1,  d16, d17, d20, d22, d18, d19, d23, d24
    MULTIPLY_BY_Q0  q2,  d17, d20, d22, d18, d19, d23, d24, d26
    MULTIPLY_BY_Q0  q14, d20, d22, d18, d19, d23, d24, d26, d27
    MULTIPLY_BY_Q0  q15, d22, d18, d19, d23, d24, d26, d27, d25

    pld             [r5, -r8]

    ; += 64 >> 7
    vqrshrun.s32    d2, q1, #7
    vqrshrun.s32    d3, q2, #7
    vqrshrun.s32    d4, q14, #7
    vqrshrun.s32    d5, q15, #7

    ; saturate
    vqmovn.u16      d2, q1
    vqmovn.u16      d3, q2

    ; transpose
    vtrn.16         d2, d3
    vtrn.32         d2, d3
    vtrn.8          d2, d3

    ; average the new value and the dst value
    vrhadd.u8       q1, q1, q3

    vst1.u32        {d2[0]}, [r2@32], r3
    vst1.u32        {d3[0]}, [r2@32], r3
    vst1.u32        {d2[1]}, [r2@32], r3
    vst1.u32        {d3[1]}, [r2@32], r4

    vmov            q8,  q9
    vmov            d20, d23
    vmov            q11, q12
    vmov            q9,  q13

    subs            r6, r6, #4              ; w -= 4
    bgt             vpx_convolve8_avg_loop_horiz

    ; outer loop
    mov             r6, r10                 ; restore w counter
    add             r0, r0, r9              ; src += src_stride * 4 - w
    add             r2, r2, r12             ; dst += dst_stride * 4 - w
    subs            r7, r7, #4              ; h -= 4
    bgt vpx_convolve8_avg_loop_horiz_v

    pop             {r4-r10, pc}

    ENDP

|vpx_convolve8_avg_vert_neon| PROC
    push            {r4-r8, lr}

    ; adjust for taps
    sub             r0, r0, r1
    sub             r0, r0, r1, lsl #1

    ldr             r4, [sp, #24]           ; filter
    ldr             r5, [sp, #36]           ; y0_q4
    add             r4, r5, lsl #4
    ldr             r6, [sp, #44]           ; w
    ldr             lr, [sp, #48]           ; h

    vld1.s16        {q0}, [r4]              ; filter

    lsl             r1, r1, #1
    lsl             r3, r3, #1

vpx_convolve8_avg_loop_vert_h
    mov             r4, r0
    add             r7, r0, r1, asr #1
    mov             r5, r2
    add             r8, r2, r3, asr #1
    mov             r12, lr                 ; h loop counter

    vld1.u32        {d16[0]}, [r4], r1
    vld1.u32        {d16[1]}, [r7], r1
    vld1.u32        {d18[0]}, [r4], r1
    vld1.u32        {d18[1]}, [r7], r1
    vld1.u32        {d20[0]}, [r4], r1
    vld1.u32        {d20[1]}, [r7], r1
    vld1.u32        {d22[0]}, [r4], r1

    vmovl.u8        q8, d16
    vmovl.u8        q9, d18
    vmovl.u8        q10, d20
    vmovl.u8        q11, d22

vpx_convolve8_avg_loop_vert
    ; always process a 4x4 block at a time
    vld1.u32        {d24[0]}, [r7], r1
    vld1.u32        {d26[0]}, [r4], r1
    vld1.u32        {d26[1]}, [r7], r1
    vld1.u32        {d24[1]}, [r4], r1

    ; extract to s16
    vmovl.u8        q12, d24
    vmovl.u8        q13, d26

    vld1.u32        {d6[0]}, [r5@32], r3
    vld1.u32        {d6[1]}, [r8@32], r3
    vld1.u32        {d7[0]}, [r5@32], r3
    vld1.u32        {d7[1]}, [r8@32], r3

    pld             [r7]
    pld             [r4]

    ; src[] * filter
    MULTIPLY_BY_Q0  q1,  d16, d17, d18, d19, d20, d21, d22, d24

    pld             [r7, r1]
    pld             [r4, r1]

    MULTIPLY_BY_Q0  q2,  d17, d18, d19, d20, d21, d22, d24, d26

    pld             [r5]
    pld             [r8]

    MULTIPLY_BY_Q0  q14, d18, d19, d20, d21, d22, d24, d26, d27

    pld             [r5, r3]
    pld             [r8, r3]

    MULTIPLY_BY_Q0  q15, d19, d20, d21, d22, d24, d26, d27, d25

    ; += 64 >> 7
    vqrshrun.s32    d2, q1, #7
    vqrshrun.s32    d3, q2, #7
    vqrshrun.s32    d4, q14, #7
    vqrshrun.s32    d5, q15, #7

    ; saturate
    vqmovn.u16      d2, q1
    vqmovn.u16      d3, q2

    ; average the new value and the dst value
    vrhadd.u8       q1, q1, q3

    sub             r5, r5, r3, lsl #1      ; reset for store
    sub             r8, r8, r3, lsl #1

    vst1.u32        {d2[0]}, [r5@32], r3
    vst1.u32        {d2[1]}, [r8@32], r3
    vst1.u32        {d3[0]}, [r5@32], r3
    vst1.u32        {d3[1]}, [r8@32], r3

    vmov            q8, q10
    vmov            d18, d22
    vmov            d19, d24
    vmov            q10, q13
    vmov            d22, d25

    subs            r12, r12, #4            ; h -= 4
    bgt             vpx_convolve8_avg_loop_vert

    ; outer loop
    add             r0, r0, #4
    add             r2, r2, #4
    subs            r6, r6, #4              ; w -= 4
    bgt             vpx_convolve8_avg_loop_vert_h

    pop             {r4-r8, pc}

    ENDP
    END
