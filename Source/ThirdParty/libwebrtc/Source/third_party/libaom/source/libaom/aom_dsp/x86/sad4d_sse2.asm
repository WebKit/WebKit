;
; Copyright (c) 2016, Alliance for Open Media. All rights reserved
;
; This source code is subject to the terms of the BSD 2 Clause License and
; the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
; was not distributed with this source code in the LICENSE file, you can
; obtain it at www.aomedia.org/license/software. If the Alliance for Open
; Media Patent License 1.0 was not distributed with this source code in the
; PATENTS file, you can obtain it at www.aomedia.org/license/patent.
;

;

%include "third_party/x86inc/x86inc.asm"

SECTION .text

%macro AVG_4x2x4 2
  movh                  m2, [second_predq]
  movlhps               m2, m2
  pavgb                 %1, m2
  pavgb                 %2, m2
  lea                   second_predq, [second_predq+8]
%endmacro
; 'mflag' affect a lot how the code works.
;
; When 'mflag' is false, the 'src_strideq' resides in register,
; [srcq + src_strideq + offset] is allowed, so we can simply
; use such form to access src memory and don't bother to update
; 'srcq' at each line. We only update 'srcq' each two-lines using
; a compact LEA instruction like [srcq+src_strideq*2].
;
; When 'mflag' is true, the 'src_strideq' resides in memory.
; we cannot use above form to access memory, we have to update
; 'srcq' at each line break. As we process two parts (first,second)
; together in each macro function, the second part may also sit
; in the next line, which means we also need to possibly add
; one 'src_strideq' to 'srcq' before processing second part.

%macro HANDLE_FIRST_OFFSET 2
  %define first_offset %2
  %if mflag == 0 && %1 == 1
    %define first_offset (src_strideq + %2)
  %endif
%endmacro

; first_extraline, second_extraline, in_line_offset
%macro HANDLE_SECOND_OFFSET 3
  %define second_offset %3
  %if mflag && %1 == 0 && %2 == 1
    add srcq, src_strideq
  %endif
  %if mflag == 0 && %2 == 1
    %define second_offset (src_strideq + %3)
  %endif
%endmacro

; Notes for line_ending:
; 0 -- not a line ending
; 1 -- line ending of a odd line [line numbers starts from one]
; 2 -- line ending of a even line
; This is specically designed to handle when src_strideq is a
; memory position, under such case, we can not accomplish
; complex address calculation using LEA, and fall back to
; using simple ADD instruction at each line ending.
%macro ADVANCE_END_OF_LINE 1
  %if mflag
    add srcq, src_strideq
  %endif
  %if mflag == 0 && %1 == 2
    lea                 srcq, [srcq +src_strideq*2]
  %endif

  %if %1 == 2
    lea                ref1q, [ref1q+ref_strideq*2]
    lea                ref2q, [ref2q+ref_strideq*2]
    lea                ref3q, [ref3q+ref_strideq*2]
    lea                ref4q, [ref4q+ref_strideq*2]
  %endif
%endmacro

; Please note that the second_offset of src is for in_line_offset,
; so it is less than src_stride.
; PROCESS_4x2x4 first, off_{first,second}_{src,ref}, do_avg,
;               {first, second}_extraline, line_ending
%macro PROCESS_4x2x4 9
  HANDLE_FIRST_OFFSET   %7, %2
  movd                  m0, [srcq + first_offset]
  HANDLE_SECOND_OFFSET  %7, %8, %4
%if %1 == 1
  movd                  m6, [ref1q+%3]
  movd                  m4, [ref2q+%3]
  movd                  m7, [ref3q+%3]
  movd                  m5, [ref4q+%3]

  movd                  m1, [srcq + second_offset]
  movd                  m2, [ref1q+%5]
  punpckldq             m0, m1
  punpckldq             m6, m2
  movd                  m1, [ref2q+%5]
  movd                  m2, [ref3q+%5]
  movd                  m3, [ref4q+%5]
  punpckldq             m4, m1
  punpckldq             m7, m2
  punpckldq             m5, m3
  movlhps               m0, m0
  movlhps               m6, m4
  movlhps               m7, m5
%if %6 == 1
  AVG_4x2x4             m6, m7
%endif
  psadbw                m6, m0
  psadbw                m7, m0
%else
  movd                  m1, [ref1q+%3]
  movd                  m5, [ref1q+%5]
  movd                  m2, [ref2q+%3]
  movd                  m4, [ref2q+%5]
  punpckldq             m1, m5
  punpckldq             m2, m4
  movd                  m3, [ref3q+%3]
  movd                  m5, [ref3q+%5]
  punpckldq             m3, m5
  movd                  m4, [ref4q+%3]
  movd                  m5, [ref4q+%5]
  punpckldq             m4, m5
  movd                  m5, [srcq + second_offset]
  punpckldq             m0, m5
  movlhps               m0, m0
  movlhps               m1, m2
  movlhps               m3, m4
%if %6 == 1
  AVG_4x2x4             m1, m3
%endif
  psadbw                m1, m0
  psadbw                m3, m0
  paddd                 m6, m1
  paddd                 m7, m3
%endif
%if %9 > 0
  ADVANCE_END_OF_LINE %9
%endif
%endmacro

; PROCESS_8x2x4 first, off_{first,second}_{src,ref}, do_avg,
;               {first,second}_extraline, line_ending
%macro PROCESS_8x2x4 9
  HANDLE_FIRST_OFFSET   %7, %2
  movh                  m0, [srcq + first_offset]
  HANDLE_SECOND_OFFSET  %7, %8, %4
%if %1 == 1
  movh                  m4, [ref1q+%3]
  movh                  m5, [ref2q+%3]
  movh                  m6, [ref3q+%3]
  movh                  m7, [ref4q+%3]
  movhps                m0, [srcq + second_offset]
  movhps                m4, [ref1q+%5]
  movhps                m5, [ref2q+%5]
  movhps                m6, [ref3q+%5]
  movhps                m7, [ref4q+%5]
%if %6 == 1
  movu                  m3, [second_predq]
  pavgb                 m4, m3
  pavgb                 m5, m3
  pavgb                 m6, m3
  pavgb                 m7, m3
  lea                   second_predq, [second_predq+mmsize]
%endif
  psadbw                m4, m0
  psadbw                m5, m0
  psadbw                m6, m0
  psadbw                m7, m0
%else
  movh                  m1, [ref1q+%3]
  movh                  m2, [ref2q+%3]
  movhps                m0, [srcq + second_offset]
  movhps                m1, [ref1q+%5]
  movhps                m2, [ref2q+%5]
%if %6 == 1
  movu                  m3, [second_predq]
  pavgb                 m1, m3
  pavgb                 m2, m3
%endif
  psadbw                m1, m0
  psadbw                m2, m0
  paddd                 m4, m1
  paddd                 m5, m2

  movh                  m1, [ref3q+%3]
  movhps                m1, [ref3q+%5]
  movh                  m2, [ref4q+%3]
  movhps                m2, [ref4q+%5]
%if %6 == 1
  pavgb                 m1, m3
  pavgb                 m2, m3
  lea                   second_predq, [second_predq+mmsize]
%endif
  psadbw                m1, m0
  psadbw                m2, m0
  paddd                 m6, m1
  paddd                 m7, m2
%endif
%if %9 > 0
  ADVANCE_END_OF_LINE %9
%endif
%endmacro

; PROCESS_16x2x4 first, off_{first,second}_{src,ref}, do_avg,
;                {first,second}_extraline, line_ending
%macro PROCESS_16x2x4 9
  ; 1st 16 px
  HANDLE_FIRST_OFFSET   %7, %2
  mova                  m0, [srcq + first_offset]
  HANDLE_SECOND_OFFSET  %7, %8, %4
%if %1 == 1
  movu                  m4, [ref1q+%3]
  movu                  m5, [ref2q+%3]
  movu                  m6, [ref3q+%3]
  movu                  m7, [ref4q+%3]
%if %6 == 1
  movu                  m3, [second_predq]
  pavgb                 m4, m3
  pavgb                 m5, m3
  pavgb                 m6, m3
  pavgb                 m7, m3
  lea                   second_predq, [second_predq+mmsize]
%endif
  psadbw                m4, m0
  psadbw                m5, m0
  psadbw                m6, m0
  psadbw                m7, m0
%else ; %1 == 1
  movu                  m1, [ref1q+%3]
  movu                  m2, [ref2q+%3]
%if %6 == 1
  movu                  m3, [second_predq]
  pavgb                 m1, m3
  pavgb                 m2, m3
%endif
  psadbw                m1, m0
  psadbw                m2, m0
  paddd                 m4, m1
  paddd                 m5, m2

  movu                  m1, [ref3q+%3]
  movu                  m2, [ref4q+%3]
%if %6 == 1
  pavgb                 m1, m3
  pavgb                 m2, m3
  lea                   second_predq, [second_predq+mmsize]
%endif
  psadbw                m1, m0
  psadbw                m2, m0
  paddd                 m6, m1
  paddd                 m7, m2
%endif ; %1 == 1

  ; 2nd 16 px
  mova                  m0, [srcq + second_offset]
  movu                  m1, [ref1q+%5]
  movu                  m2, [ref2q+%5]

%if %6 == 1
  movu                  m3, [second_predq]
  pavgb                 m1, m3
  pavgb                 m2, m3
%endif
  psadbw                m1, m0
  psadbw                m2, m0
  paddd                 m4, m1
  paddd                 m5, m2

  movu                  m1, [ref3q+%5]
  movu                  m2, [ref4q+%5]

%if %9 > 0
  ADVANCE_END_OF_LINE %9
%endif

%if %6 == 1
  pavgb                 m1, m3
  pavgb                 m2, m3
  lea                   second_predq, [second_predq+mmsize]
%endif
  psadbw                m1, m0
  psadbw                m2, m0
  paddd                 m6, m1
  paddd                 m7, m2
%endmacro

; PROCESS_32x2x4 first, off_{first,second}_{src,ref}, do_avg,
;                {first,second}_extraline, line_ending
%macro PROCESS_32x2x4 9
  PROCESS_16x2x4 %1, %2, %3, %2 + 16, %3 + 16, %6, %7, %7, %8 - %7
  PROCESS_16x2x4  0, %4, %5, %4 + 16, %5 + 16, %6, %8, %8, %9
%endmacro

; PROCESS_64x2x4 first, off_{first,second}_{src,ref}, do_avg,
;                {first,second}_extraline, line_ending
%macro PROCESS_64x2x4 9
  PROCESS_32x2x4 %1, %2, %3, %2 + 32, %3 + 32, %6, %7, %7, %8 - %7
  PROCESS_32x2x4  0, %4, %5, %4 + 32, %5 + 32, %6, %8, %8, %9
%endmacro

; PROCESS_128x2x4 first, off_{first,second}_{src,ref}, do_avg,
;                 {first,second}_extraline, line_ending
%macro PROCESS_128x2x4 9
  PROCESS_64x2x4 %1, %2, %3, %2 + 64, %3 + 64, %6, %7, %7, %8 - %7
  PROCESS_64x2x4  0, %4, %5, %4 + 64, %5 + 64, %6, %8, %8, %9
%endmacro

; void aom_sadNxNx4d_sse2(uint8_t *src,    int src_stride,
;                         uint8_t *ref[4], int ref_stride,
;                         uint32_t res[4]);
; Macro Arguments:
;   1: Width
;   2: Height
;   3: If 0, then normal sad, else avg
;   4: If 0, then normal sad, else skip rows
%macro SADNXN4D 2-4 0,0
%if %4 == 1  ; skip rows
%if ARCH_X86_64
cglobal sad_skip_%1x%2x4d, 5, 8, 8, src, src_stride, ref1, ref_stride, \
                              res, ref2, ref3, ref4
%else
cglobal sad_skip_%1x%2x4d, 4, 7, 8, src, src_stride, ref1, ref_stride, \
                              ref2, ref3, ref4
%endif
%elif %3 == 0  ; normal sad
%if ARCH_X86_64
cglobal sad%1x%2x4d, 5, 8, 8, src, src_stride, ref1, ref_stride, \
                              res, ref2, ref3, ref4
%else
cglobal sad%1x%2x4d, 4, 7, 8, src, src_stride, ref1, ref_stride, \
                              ref2, ref3, ref4
%endif
%else ; avg
%if ARCH_X86_64
cglobal sad%1x%2x4d_avg, 6, 10, 8, src, src_stride, ref1, ref_stride, \
                                  second_pred, res, ref2, ref3, ref4
%else
cglobal sad%1x%2x4d_avg, 5, 7, 8, src, ref4, ref1, ref_stride, \
                                  second_pred, ref2, ref3
  %define src_strideq r1mp
  %define src_strided r1mp
%endif
%endif

  %define mflag ((1 - ARCH_X86_64) & %3)
%if %4 == 1
  lea          src_strided, [2*src_strided]
  lea          ref_strided, [2*ref_strided]
%endif
  movsxdifnidn src_strideq, src_strided
  movsxdifnidn ref_strideq, ref_strided

  mov                ref2q, [ref1q+gprsize*1]
  mov                ref3q, [ref1q+gprsize*2]
  mov                ref4q, [ref1q+gprsize*3]
  mov                ref1q, [ref1q+gprsize*0]

  PROCESS_%1x2x4 1, 0, 0, 0, ref_strideq, %3, 0, 1, 2
%if %4 == 1  ; downsample number of rows by 2
%define num_rep (%2-8)/4
%else
%define num_rep (%2-4)/2
%endif
%rep num_rep
  PROCESS_%1x2x4 0, 0, 0, 0, ref_strideq, %3, 0, 1, 2
%endrep
%undef num_rep
  PROCESS_%1x2x4 0, 0, 0, 0, ref_strideq, %3, 0, 1, 2

%if %3 == 0
  %define resultq r4
  %define resultmp r4mp
%else
  %define resultq r5
  %define resultmp r5mp
%endif

%if %1 > 4
  pslldq                m5, 4
  pslldq                m7, 4
  por                   m4, m5
  por                   m6, m7
  mova                  m5, m4
  mova                  m7, m6
  punpcklqdq            m4, m6
  punpckhqdq            m5, m7
  paddd                 m4, m5
%if %4 == 1
  pslld                 m4, 1
%endif
  movifnidn             resultq, resultmp
  movu                [resultq], m4
  RET
%else
  pshufd            m6, m6, 0x08
  pshufd            m7, m7, 0x08
%if %4 == 1
  pslld                 m6, 1
  pslld                 m7, 1
%endif
  movifnidn             resultq, resultmp
  movq              [resultq+0], m6
  movq              [resultq+8], m7
  RET
%endif
%endmacro

INIT_XMM sse2
SADNXN4D 128, 128
SADNXN4D 128,  64
SADNXN4D  64, 128
SADNXN4D  64,  64
SADNXN4D  64,  32
SADNXN4D  32,  64
SADNXN4D  32,  32
SADNXN4D  32,  16
SADNXN4D  16,  32
SADNXN4D  16,  16
SADNXN4D  16,   8
SADNXN4D   8,  16
SADNXN4D   8,   8
SADNXN4D   8,   4
SADNXN4D   4,   8
SADNXN4D   4,   4
%if CONFIG_REALTIME_ONLY==0
SADNXN4D   4,  16
SADNXN4D  16,   4
SADNXN4D   8,  32
SADNXN4D  32,   8
SADNXN4D  16,  64
SADNXN4D  64,  16
%endif
%if CONFIG_REALTIME_ONLY==0
SADNXN4D 128, 128, 1
SADNXN4D 128,  64, 1
SADNXN4D  64, 128, 1
SADNXN4D  64,  64, 1
SADNXN4D  64,  32, 1
SADNXN4D  32,  64, 1
SADNXN4D  32,  32, 1
SADNXN4D  32,  16, 1
SADNXN4D  16,  32, 1
SADNXN4D  16,  16, 1
SADNXN4D  16,   8, 1
SADNXN4D   8,  16, 1
SADNXN4D   8,   8, 1
SADNXN4D   8,   4, 1
SADNXN4D   4,   8, 1
SADNXN4D   4,   4, 1
SADNXN4D   4,  16, 1
SADNXN4D  16,   4, 1
SADNXN4D   8,  32, 1
SADNXN4D  32,   8, 1
SADNXN4D  16,  64, 1
SADNXN4D  64,  16, 1
%endif
SADNXN4D 128, 128, 0, 1
SADNXN4D 128,  64, 0, 1
SADNXN4D  64, 128, 0, 1
SADNXN4D  64,  64, 0, 1
SADNXN4D  64,  32, 0, 1
SADNXN4D  32,  64, 0, 1
SADNXN4D  32,  32, 0, 1
SADNXN4D  32,  16, 0, 1
SADNXN4D  16,  32, 0, 1
SADNXN4D  16,  16, 0, 1
SADNXN4D  16,   8, 0, 1
SADNXN4D   8,  16, 0, 1
SADNXN4D   8,   8, 0, 1
SADNXN4D   4,   8, 0, 1
%if CONFIG_REALTIME_ONLY==0
SADNXN4D   4,  16, 0, 1
SADNXN4D   8,  32, 0, 1
SADNXN4D  32,   8, 0, 1
SADNXN4D  16,  64, 0, 1
SADNXN4D  64,  16, 0, 1
%endif

; Different assembly is needed when the height gets subsampled to 2
; SADNXN4D 16,  4, 0, 1
; SADNXN4D  8,  4, 0, 1
; SADNXN4D  4,  4, 0, 1
