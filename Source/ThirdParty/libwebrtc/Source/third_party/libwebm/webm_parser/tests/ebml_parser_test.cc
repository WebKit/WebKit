// Copyright (c) 2016 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#include "src/ebml_parser.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "test_utils/element_parser_test.h"
#include "webm/id.h"

using webm::Ebml;
using webm::EbmlParser;
using webm::ElementParserTest;
using webm::Id;

namespace {

class EbmlParserTest : public ElementParserTest<EbmlParser, Id::kEbml> {};

TEST_F(EbmlParserTest, DefaultParse) {
  EXPECT_CALL(callback_, OnEbml(metadata_, Ebml{})).Times(1);

  ParseAndVerify();
}

TEST_F(EbmlParserTest, DefaultValues) {
  SetReaderData({
      0x42, 0x86,  // ID = 0x4286 (EBMLVersion).
      0x80,  // Size = 0.

      0x42, 0xF7,  // ID = 0x42F7 (EBMLReadVersion).
      0x80,  // Size = 0.

      0x42, 0xF2,  // ID = 0x42F2 (EBMLMaxIDLength).
      0x40, 0x00,  // Size = 0.

      0x42, 0xF3,  // ID = 0x42F3 (EBMLMaxSizeLength).
      0x80,  // Size = 0.

      0xEC,  // ID = 0xEC (Void).
      0x40, 0x00,  // Size = 0.

      0x42, 0x82,  // ID = 0x4282 (DocType).
      0x40, 0x00,  // Size = 0.

      0x42, 0x87,  // ID = 0x4287 (DocTypeVersion).
      0x80,  // Size = 0.

      0x42, 0x85,  // ID = 0x4285 (DocTypeReadVersion).
      0x80,  // Size = 0.

      0xEC,  // ID = 0xEC (Void).
      0x82,  // Size = 2.
      0x01, 0x02,  // Body.
  });

  Ebml ebml;
  ebml.ebml_version.Set(1, true);
  ebml.ebml_read_version.Set(1, true);
  ebml.ebml_max_id_length.Set(4, true);
  ebml.ebml_max_size_length.Set(8, true);
  ebml.doc_type.Set("matroska", true);
  ebml.doc_type_version.Set(1, true);
  ebml.doc_type_read_version.Set(1, true);

  EXPECT_CALL(callback_, OnEbml(metadata_, ebml)).Times(1);

  ParseAndVerify();
}

TEST_F(EbmlParserTest, CustomValues) {
  SetReaderData({
      0x42, 0x86,  // ID = 0x4286 (EBMLVersion).
      0x81,  // Size = 1.
      0x02,  // Body (value = 2).

      0x42, 0xF7,  // ID = 0x42F7 (EBMLReadVersion).
      0x81,  // Size = 1.
      0x04,  // Body (value = 4).

      0x42, 0xF2,  // ID = 0x42F2 (EBMLMaxIDLength).
      0x40, 0x02,  // Size = 2.
      0x00, 0x02,  // Body (value = 2).

      0x42, 0xF3,  // ID = 0x42F3 (EBMLMaxSizeLength).
      0x81,  // Size = 1.
      0x04,  // Body (value = 4).

      0xEC,  // ID = 0xEC (Void).
      0x40, 0x00,  // Size = 0.

      0x42, 0x82,  // ID = 0x4282 (DocType).
      0x40, 0x02,  // Size = 2.
      0x48, 0x69,  // Body (value = "Hi").

      0x42, 0x87,  // ID = 0x4287 (DocTypeVersion).
      0x81,  // Size = 1.
      0xFF,  // Body (value = 255).

      0x42, 0x85,  // ID = 0x4285 (DocTypeReadVersion).
      0x81,  // Size = 1.
      0x02,  // Body (value = 2).

      0xEC,  // ID = 0xEC (Void).
      0x82,  // Size = 2.
      0x01, 0x02,  // Body.
  });

  Ebml ebml;
  ebml.ebml_version.Set(2, true);
  ebml.ebml_read_version.Set(4, true);
  ebml.ebml_max_id_length.Set(2, true);
  ebml.ebml_max_size_length.Set(4, true);
  ebml.doc_type.Set("Hi", true);
  ebml.doc_type_version.Set(255, true);
  ebml.doc_type_read_version.Set(2, true);

  EXPECT_CALL(callback_, OnEbml(metadata_, ebml)).Times(1);

  ParseAndVerify();
}

}  // namespace
