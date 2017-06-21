/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/* PayloadTypes.h */
/* Used by RTPencode application */
// TODO(henrik.lundin) Remove this once RTPencode is refactored.

/* RTP defined codepoints */
#define NETEQ_CODEC_PCMU_PT				0
#define NETEQ_CODEC_PCMA_PT				8
#define NETEQ_CODEC_G722_PT				9
#define NETEQ_CODEC_CN_PT				13

/* Dynamic RTP codepoints */
#define NETEQ_CODEC_ILBC_PT				102
#define NETEQ_CODEC_ISAC_PT				103
#define NETEQ_CODEC_ISACSWB_PT			104
#define NETEQ_CODEC_AVT_PT				106
#define NETEQ_CODEC_OPUS_PT             111
#define NETEQ_CODEC_RED_PT				117
#define NETEQ_CODEC_CN_WB_PT			105
#define NETEQ_CODEC_CN_SWB_PT           126
#define NETEQ_CODEC_PCM16B_PT			93
#define NETEQ_CODEC_PCM16B_WB_PT		94
#define NETEQ_CODEC_PCM16B_SWB32KHZ_PT	95
#define NETEQ_CODEC_PCM16B_SWB48KHZ_PT	96
