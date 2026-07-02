;
; Copyright (c) 2016, Alliance for Open Media. All rights reserved.
;
; This source code is subject to the terms of the BSD 2 Clause License and
; the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
; was not distributed with this source code in the LICENSE file, you can
; obtain it at www.aomedia.org/license/software. If the Alliance for Open
; Media Patent License 1.0 was not distributed with this source code in the
; PATENTS file, you can obtain it at www.aomedia.org/license/patent.
;

%define private_prefix av1

%include "third_party/x86inc/x86inc.asm"

SECTION .text

%macro TRANSFORM_COLS 0
  paddw           m0,        m1
  movq            m4,        m0
  psubw           m3,        m2
  psubw           m4,        m3
  psraw           m4,        1
  movq            m5,        m4
  psubw           m5,        m1 ;b1
  psubw           m4,        m2 ;c1
  psubw           m0,        m4
  paddw           m3,        m5
                                ; m0 a0
  SWAP            1,         4  ; m1 c1
  SWAP            2,         3  ; m2 d1
  SWAP            3,         5  ; m3 b1
%endmacro

%macro TRANSPOSE_4X4 0
                                ; 00 01 02 03
                                ; 10 11 12 13
                                ; 20 21 22 23
                                ; 30 31 32 33
  punpcklwd       m0,        m1 ; 00 10 01 11  02 12 03 13
  punpcklwd       m2,        m3 ; 20 30 21 31  22 32 23 33
  mova            m1,        m0
  punpckldq       m0,        m2 ; 00 10 20 30  01 11 21 31
  punpckhdq       m1,        m2 ; 02 12 22 32  03 13 23 33
%endmacro

INIT_XMM sse2
cglobal fwht4x4, 3, 4, 8, input, output, stride
  lea             r3q,       [inputq + strideq*4]
  movq            m0,        [inputq] ;a1
  movq            m1,        [inputq + strideq*2] ;b1
  movq            m2,        [r3q] ;c1
  movq            m3,        [r3q + strideq*2] ;d1

  TRANSFORM_COLS
  TRANSPOSE_4X4
  SWAP            1,         2
  psrldq          m1,        m0, 8
  psrldq          m3,        m2, 8
  TRANSFORM_COLS
  TRANSPOSE_4X4

  psllw           m0,        2
  psllw           m1,        2

  ; sign extension
  mova            m2,             m0
  mova            m3,             m1
  punpcklwd       m0,             m0
  punpcklwd       m1,             m1
  punpckhwd       m2,             m2
  punpckhwd       m3,             m3
  psrad           m0,             16
  psrad           m1,             16
  psrad           m2,             16
  psrad           m3,             16
  mova            [outputq],      m0
  mova            [outputq + 16], m2
  mova            [outputq + 32], m1
  mova            [outputq + 48], m3

  RET
