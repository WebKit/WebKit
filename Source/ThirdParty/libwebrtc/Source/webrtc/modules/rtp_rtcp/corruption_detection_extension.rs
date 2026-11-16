/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

const MAX_VALUE_SIZE_BYTES: usize = 16;
const MANDATORY_PAYLOAD_BYTES: usize = 1;
const CONFIGURATION_BYTES: usize = 3;
const MAX_VALUE_FOR_STD_DEV: f64 = 40.0;

#[derive(Default)]
pub struct CorruptionDetectionExtension {
    // Sequence index in the Halton sequence.
    // Valid values: [0, 2^7-1]
    pub sequence_index: i8,

    // Whether to interpret the `sequence_index_` as the most significant bits of
    // the true sequence index.
    pub interpret_sequence_index_as_most_significant_bits: bool,

    // Standard deviation of the Gaussian filter kernel.
    // Valid values: [0, 40.0]
    pub std_dev: f64,

    // Corruption threshold for the luma layer.
    // Valid values: [0, 2^4 - 1]
    pub luma_error_threshold: i8,

    // Corruption threshold for the chroma layer.
    // Valid values: [0, 2^4 - 1]
    pub chroma_error_threshold: i8,

    // An ordered list of samples that are the result of applying the Gaussian
    // filter on the image. The coordinates of the samples and their layer are
    // determined by the Halton sequence.
    // An empty list should be interpreted as a way to keep the `sequence_index`
    // in sync.
    num_sample_values: u8,
    arr_sample_values: [u8; 13],
}

impl CorruptionDetectionExtension {
    fn parse_mandatory(&mut self, data: &[u8]) {
        self.interpret_sequence_index_as_most_significant_bits = data[0] >> 7 != 0;
        self.sequence_index = (data[0] & 0b0111_1111) as i8;
    }

    pub fn parse(&mut self, data: &[u8]) -> bool {
        if data.len() == MANDATORY_PAYLOAD_BYTES {
            self.parse_mandatory(data);
            return true;
        }
        if data.len() <= CONFIGURATION_BYTES || data.len() > MAX_VALUE_SIZE_BYTES {
            return false;
        }
        self.parse_mandatory(data);
        self.std_dev = (data[1] as f64) * MAX_VALUE_FOR_STD_DEV / 255.0;
        let channel_error_thresholds = data[2];
        self.luma_error_threshold = (channel_error_thresholds >> 4) as i8;
        self.chroma_error_threshold = (channel_error_thresholds & 0xF) as i8;
        self.num_sample_values = (data.len() - CONFIGURATION_BYTES) as u8;
        self.arr_sample_values[..(self.num_sample_values as usize)]
            .copy_from_slice(&data[CONFIGURATION_BYTES..]);
        true
    }

    pub fn sample_values(&self) -> &[u8] {
        &self.arr_sample_values[..(self.num_sample_values as usize)]
    }
}

#[cfg(test)]
mod test {
    use super::*;

    #[test]
    fn parses_mandatory_fields_from_extension() {
        let raw: &[u8] = &[0b1110_1111];
        let mut message = CorruptionDetectionExtension::default();
        assert!(message.parse(raw));
        assert_eq!(message.sequence_index, 0b0110_1111);
        assert!(message.interpret_sequence_index_as_most_significant_bits);
        assert_eq!(message.std_dev, 0.0);
        assert_eq!(message.luma_error_threshold, 0);
        assert_eq!(message.chroma_error_threshold, 0);
        assert!(message.sample_values().is_empty());
    }

    #[test]
    fn fails_to_parse_when_given_too_few_fields() {
        let raw: &[u8] = &[0b1110_1111, 8, 0];
        assert!(!CorruptionDetectionExtension::default().parse(raw));
    }

    #[test]
    fn parses_everything_from_extension_with_few_samples() {
        let sample_values: &[u8] = &[1, 2, 3];
        let raw: &[u8] = &[0b1100_0100, 220, 0b1110_1111, 1, 2, 3];
        let mut message = CorruptionDetectionExtension::default();

        assert!(message.parse(raw));
        assert_eq!(message.sample_values(), sample_values);
    }

    #[test]
    fn parses_everything_from_extension_when_upper_bits_are_used_for_sequence_index() {
        let sample_values: &[u8] = &[1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13];
        let raw: &[u8] = &[
            0b1100_0100,
            220,
            0b1110_1111,
            sample_values[0],
            sample_values[1],
            sample_values[2],
            sample_values[3],
            sample_values[4],
            sample_values[5],
            sample_values[6],
            sample_values[7],
            sample_values[8],
            sample_values[9],
            sample_values[10],
            sample_values[11],
            sample_values[12],
        ];
        let mut message = CorruptionDetectionExtension::default();

        assert!(message.parse(raw));
        assert_eq!(message.sequence_index, 0b0100_0100);
        assert!(message.interpret_sequence_index_as_most_significant_bits);
        assert_eq!(message.std_dev, 34.509803921568626); // 220 / 255.0 * 40.0
        assert_eq!(message.luma_error_threshold, 0b1110);
        assert_eq!(message.chroma_error_threshold, 0b1111);
        assert_eq!(message.sample_values(), sample_values);
    }

    #[test]
    fn parses_everything_from_extension_when_lower_bits_are_used_for_sequence_index() {
        let sample_values: &[u8] = &[1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13];
        let raw: &[u8] = &[
            0b0100_0100,
            220,
            0b1110_1111,
            sample_values[0],
            sample_values[1],
            sample_values[2],
            sample_values[3],
            sample_values[4],
            sample_values[5],
            sample_values[6],
            sample_values[7],
            sample_values[8],
            sample_values[9],
            sample_values[10],
            sample_values[11],
            sample_values[12],
        ];
        let mut message = CorruptionDetectionExtension::default();

        assert!(message.parse(raw));
        assert_eq!(message.sequence_index, 0b0100_0100);
        assert!(!message.interpret_sequence_index_as_most_significant_bits);
        assert_eq!(message.std_dev, 34.509803921568626); // 220 / 255.0 * 40.0
        assert_eq!(message.luma_error_threshold, 0b1110);
        assert_eq!(message.chroma_error_threshold, 0b1111);
        assert_eq!(message.sample_values(), sample_values);
    }

    #[test]
    fn fails_to_parse_when_too_many_samples_are_specified() {
        let raw: [u8; 17] = [42; 17];
        assert!(!CorruptionDetectionExtension::default().parse(&raw));
    }
}
