/*
 *  Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/desktop_capture/win/full_screen_win_application_handler.h"

#include <vector>

#include "modules/desktop_capture/win/test_support/test_window.h"
#include "modules/desktop_capture/win/window_capture_utils.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {

WindowInfo CreateTestWindow(const WCHAR* window_title,
                            const WCHAR* window_class) {
  return CreateTestWindow(window_title, /*height=*/240, /*width=*/320,
                          /*extended_styles=*/0, window_class);
}

class FullScreenWinApplicationHandlerTest : public ::testing::Test {
 public:
  void CreateEditorWindow(
      const WCHAR* title,
      const WCHAR* window_class = L"PPTFrameClass",
      bool fullscreen_slide_show_started_after_capture_start = true) {
    editor_window_info_ = CreateTestWindow(title, window_class);
    full_screen_ppt_handler_ = std::make_unique<FullScreenPowerPointHandler>(
        reinterpret_cast<DesktopCapturer::SourceId>(editor_window_info_.hwnd));
    full_screen_ppt_handler_->SetSlideShowCreationStateForTest(
        fullscreen_slide_show_started_after_capture_start);
  }

  HWND CreateSlideShowWindow(const WCHAR* title) {
    slide_show_window_info_ =
        CreateTestWindow(title, /*window_class=*/L"screenClass");
    ResizeTestWindowToFullScreen(slide_show_window_info_.hwnd);
    return slide_show_window_info_.hwnd;
  }

  // FindFullScreenWindow returns a non-zero value when a full screen slide show
  // is found. It returns NULL when it fails to find a corresponding full screen
  // slide show.
  HWND FindFullScreenWindow() {
    DesktopCapturer::SourceList window_list;
    EXPECT_TRUE(GetWindowList(GetWindowListFlags::kNone, &window_list));
    EXPECT_GT(window_list.size(), 0u);  // Otherwise, faulty test.

    return reinterpret_cast<HWND>(
        full_screen_ppt_handler_->FindFullScreenWindow(window_list,
                                                       /*timestamp=*/0));
  }

  // FindEditorWindow returns a non-zero value when a editor window is found.
  HWND FindEditorWindow() {
    DesktopCapturer::SourceList window_list;
    EXPECT_TRUE(GetWindowList(GetWindowListFlags::kNone, &window_list));
    EXPECT_GT(window_list.size(), 0u);  // Otherwise, faulty test.

    return reinterpret_cast<HWND>(
        full_screen_ppt_handler_->FindEditorWindow(window_list));
  }

  void TearDown() override {
    DestroyTestWindow(editor_window_info_);
    DestroyTestWindow(slide_show_window_info_);
  }

 protected:
  WindowInfo editor_window_info_;
  WindowInfo slide_show_window_info_;
  std::unique_ptr<FullScreenPowerPointHandler> full_screen_ppt_handler_;
};

TEST_F(FullScreenWinApplicationHandlerTest, FullScreenWindowFoundForEditor) {
  CreateEditorWindow(L"My - Title - PowerPoint");
  HWND slide_show =
      CreateSlideShowWindow(L"PowerPoint Slide Show - [My - Title]");

  EXPECT_EQ(FindFullScreenWindow(), slide_show);
}

TEST_F(FullScreenWinApplicationHandlerTest,
       FullScreenWindowFoundWhenEditorTitleHasExtraSpaces) {
  CreateEditorWindow(L"My  Title  - PowerPoint");
  HWND slide_show =
      CreateSlideShowWindow(L"PowerPoint Slide Show - [My  Title ]");

  EXPECT_EQ(FindFullScreenWindow(), slide_show);
}

// This can happen in older PowerPoint versions, where full screen slide show
// title did not had brackets around the document name.
TEST_F(FullScreenWinApplicationHandlerTest,
       FullScreenWindowFoundWhenSlideShowTitleHasNoBrackets) {
  CreateEditorWindow(L"My - Title - PowerPoint");
  HWND slide_show =
      CreateSlideShowWindow(L"PowerPoint Slide Show - My - Title");

  EXPECT_EQ(FindFullScreenWindow(), slide_show);
}

TEST_F(FullScreenWinApplicationHandlerTest,
       FullScreenWindowFoundWhenEditorTitleHasUnevenlySpacedDashes) {
  CreateEditorWindow(L"My -Test - Title - PowerPoint");
  HWND slide_show =
      CreateSlideShowWindow(L"PowerPoint Slide Show - [My -Test - Title]");

  EXPECT_EQ(FindFullScreenWindow(), slide_show);
}

TEST_F(FullScreenWinApplicationHandlerTest,
       FullScreenWindowFoundWhenEditorTitleHasMatchingBrackets) {
  CreateEditorWindow(L"[My - Title] - PowerPoint");
  HWND slide_show =
      CreateSlideShowWindow(L"PowerPoint Slide Show - [[My - Title]]");

  EXPECT_EQ(FindFullScreenWindow(), slide_show);
}

TEST_F(FullScreenWinApplicationHandlerTest,
       FullScreenWindowFoundWhenEditorTitleHasAFrontRightBracket) {
  CreateEditorWindow(L"[My - Title - PowerPoint");
  HWND slide_show =
      CreateSlideShowWindow(L"PowerPoint Slide Show - [[My - Title]");

  EXPECT_EQ(FindFullScreenWindow(), slide_show);
}

TEST_F(FullScreenWinApplicationHandlerTest,
       FullScreenWindowFoundWhenEditorTitleHasABackRightBracket) {
  CreateEditorWindow(L"My - Title[ - PowerPoint");
  HWND slide_show =
      CreateSlideShowWindow(L"PowerPoint Slide Show - [My - Title[]");

  EXPECT_EQ(FindFullScreenWindow(), slide_show);
}

TEST_F(FullScreenWinApplicationHandlerTest,
       FullScreenWindowFoundWhenEditorTitleHasAFrontLeftBracket) {
  CreateEditorWindow(L"]My - Title - PowerPoint");
  HWND slide_show =
      CreateSlideShowWindow(L"PowerPoint Slide Show - []My - Title]");

  EXPECT_EQ(FindFullScreenWindow(), slide_show);
}

TEST_F(FullScreenWinApplicationHandlerTest,
       FullScreenWindowFoundWhenEditorTitleHasABackLeftBracket) {
  CreateEditorWindow(L"My - Title] - PowerPoint");
  HWND slide_show =
      CreateSlideShowWindow(L"PowerPoint Slide Show - [My - Title]]");

  EXPECT_EQ(FindFullScreenWindow(), slide_show);
}

TEST_F(FullScreenWinApplicationHandlerTest,
       FullScreenWindowFoundWhenEditorTitleHasMismatchingBrackets) {
  CreateEditorWindow(L"]My - Title[ - PowerPoint");
  HWND slide_show =
      CreateSlideShowWindow(L"PowerPoint Slide Show - []My - Title[]");

  EXPECT_EQ(FindFullScreenWindow(), slide_show);
}

TEST_F(FullScreenWinApplicationHandlerTest,
       FullScreenWindowFoundWhenEditorTitleHasOnlyLeftBrackets) {
  CreateEditorWindow(L"[[My - Title[ - PowerPoint");
  HWND slide_show =
      CreateSlideShowWindow(L"PowerPoint Slide Show - [[[My - Title[]");

  EXPECT_EQ(FindFullScreenWindow(), slide_show);
}

TEST_F(FullScreenWinApplicationHandlerTest,
       FullScreenWindowFoundWhenEditorTitleHasOnlyRightBrackets) {
  CreateEditorWindow(L"]My - Title]] - PowerPoint");
  HWND slide_show =
      CreateSlideShowWindow(L"PowerPoint Slide Show - []My - Title]]]");

  EXPECT_EQ(FindFullScreenWindow(), slide_show);
}

TEST_F(FullScreenWinApplicationHandlerTest,
       FullScreenWindowFoundWhenEditorTitleHasNestedBrackets) {
  CreateEditorWindow(L"[[My - Title]] - PowerPoint");
  HWND slide_show =
      CreateSlideShowWindow(L"PowerPoint Slide Show - [[[My - Title]]]");

  EXPECT_EQ(FindFullScreenWindow(), slide_show);
}

TEST_F(FullScreenWinApplicationHandlerTest,
       FullScreenWindowFoundWhenEditorTitleHasNestedBracketsInFront) {
  CreateEditorWindow(L"[[My] - Title] - PowerPoint");
  HWND slide_show =
      CreateSlideShowWindow(L"PowerPoint Slide Show - [[[My] - Title]]");

  EXPECT_EQ(FindFullScreenWindow(), slide_show);
}

TEST_F(FullScreenWinApplicationHandlerTest,
       FullScreenWindowFoundWhenEditorTitleHasNestedBracketsInBack) {
  CreateEditorWindow(L"[My - [Title]] - PowerPoint");
  HWND slide_show =
      CreateSlideShowWindow(L"PowerPoint Slide Show - [[My - [Title]]]");

  EXPECT_EQ(FindFullScreenWindow(), slide_show);
}

TEST_F(FullScreenWinApplicationHandlerTest,
       FullScreenWindowFoundWhenEditorWindowHasNoPowerPointInTitle) {
  CreateEditorWindow(L"My - Title - Power");
  HWND slide_show =
      CreateSlideShowWindow(L"PowerPoint Slide Show - [My - Title]");

  EXPECT_EQ(FindFullScreenWindow(), slide_show);
}

TEST_F(FullScreenWinApplicationHandlerTest,
       FullScreenWindowFoundWhenSlideShowWindowHasNoPowerPointInTitle) {
  CreateEditorWindow(L"My - Title - PowerPoint");
  HWND slide_show = CreateSlideShowWindow(L"Slide Show - [My - Title]");

  EXPECT_EQ(FindFullScreenWindow(), slide_show);
}

TEST_F(FullScreenWinApplicationHandlerTest,
       FullScreenWindowNotFoundWhenEditorWindowHasWrongWindowClass) {
  // Create editor window with class name "WrongClass" instead of
  // "PPTFrameClass".
  CreateEditorWindow(L"My - Title - PowerPoint",
                     /*window_class=*/L"WrongClass");
  HWND slide_show =
      CreateSlideShowWindow(L"PowerPoint Slide Show - [My - Title]");

  EXPECT_NE(FindFullScreenWindow(), slide_show);
}

TEST_F(FullScreenWinApplicationHandlerTest,
       FullScreenWindowNotFoundWhenFullScreenPPTHandlerUsesSlideShowWindow) {
  CreateEditorWindow(L"My - Title - PowerPoint");

  // Create FullScreenPowerPointHandler using the slide show WindowInfo.
  HWND slide_show =
      CreateSlideShowWindow(L"PowerPoint Slide Show - [My - Title]");
  full_screen_ppt_handler_.reset(new FullScreenPowerPointHandler(
      reinterpret_cast<DesktopCapturer::SourceId>(slide_show)));

  EXPECT_NE(FindFullScreenWindow(), slide_show);
}

TEST_F(FullScreenWinApplicationHandlerTest,
       CorrectFullScreenWindowFoundWhenMultipleSlideShowsHaveSimilarTitles) {
  CreateEditorWindow(L"My - Title - PowerPoint");
  HWND correct_slide_show =
      CreateSlideShowWindow(L"PowerPoint Slide Show - [My - Title]");
  std::vector<HWND> wrong_slide_shows = {
      CreateSlideShowWindow(L"PowerPoint Slide Show - [My -  Title]"),
      CreateSlideShowWindow(L"PowerPoint Slide Show - [My Title]"),
      CreateSlideShowWindow(L"PowerPoint Slide Show - [My Title -]")};

  ASSERT_THAT(wrong_slide_shows,
              testing::Not(testing::Contains(correct_slide_show)));
  EXPECT_EQ(FindFullScreenWindow(), correct_slide_show);
}

// TODO(crbug.com/409473386): Add DestroyTestWindow to clean the tests.
TEST_F(FullScreenWinApplicationHandlerTest,
       FullScreenWindowsFoundWhenMultipleEditorsAndSlideShowsExist) {
  std::vector<WindowInfo> editors = {
      CreateTestWindow(L"My - Title - PowerPoint",
                       /*window_class=*/L"PPTFrameClass"),
      CreateTestWindow(L"[[My Title] - PowerPoint",
                       /*window_class=*/L"PPTFrameClass"),
      CreateTestWindow(L"My  Ttile - PowerPoint",
                       /*window_class=*/L"PPTFrameClass")};

  std::vector<std::unique_ptr<FullScreenPowerPointHandler>> handlers;
  for (auto& editor : editors) {
    auto handler = std::make_unique<FullScreenPowerPointHandler>(
        reinterpret_cast<DesktopCapturer::SourceId>(editor.hwnd));
    handler->SetSlideShowCreationStateForTest(
        /*fullscreen_slide_show_started_after_capture_start=*/true);
    handlers.push_back(std::move(handler));
  }

  std::vector<HWND> slide_shows = {
      CreateSlideShowWindow(L"PowerPoint Slide Show - [My - Title]"),
      CreateSlideShowWindow(L"PowerPoint Slide Show - [[[My Title]]"),
      CreateSlideShowWindow(L"PowerPoint Slide Show - [My  Ttile]")};

  DesktopCapturer::SourceList window_list;
  EXPECT_TRUE(GetWindowList(GetWindowListFlags::kNone, &window_list));
  EXPECT_GT(window_list.size(), 0u);

  for (size_t count = 0; count < handlers.size(); count++) {
    EXPECT_EQ(reinterpret_cast<HWND>(
                  handlers[count]->FindFullScreenWindow(window_list,
                                                        /*timestamp=*/0)),
              slide_shows[count]);
  }
}

TEST_F(FullScreenWinApplicationHandlerTest,
       FullScreenWindowNotFoundWhenTwoEditorsWithSameTitleExist) {
  const WCHAR* editor_title = L"My - Title - PowerPoint";
  CreateEditorWindow(editor_title);
  WindowInfo second_editor_window_info =
      CreateTestWindow(editor_title, /*window_class=*/L"PPTFrameClass");
  EXPECT_NE(editor_window_info_.hwnd, second_editor_window_info.hwnd);
  HWND slide_show =
      CreateSlideShowWindow(L"PowerPoint Slide Show - [My - Title]");

  EXPECT_NE(FindFullScreenWindow(), slide_show);
  EXPECT_EQ(FindFullScreenWindow(), reinterpret_cast<HWND>(0));

  DestroyTestWindow(second_editor_window_info);
}

TEST_F(FullScreenWinApplicationHandlerTest,
       FullScreenWindowNotFoundWhenSlideShowWasCreatedBefore) {
  const WCHAR* editor_title = L"My - Title - PowerPoint";
  CreateEditorWindow(
      editor_title, /*window_class=*/L"PPTFrameClass",
      /*fullscreen_slide_show_started_after_capture_start=*/false);
  HWND slide_show =
      CreateSlideShowWindow(L"PowerPoint Slide Show - [My - Title]");

  EXPECT_NE(FindFullScreenWindow(), slide_show);
  EXPECT_EQ(FindFullScreenWindow(), reinterpret_cast<HWND>(0));
}

TEST_F(FullScreenWinApplicationHandlerTest, EditorWindowFound) {
  CreateEditorWindow(L"My - Title - PowerPoint");

  // Create FullScreenPowerPointHandler using the slide show WindowInfo.
  CreateSlideShowWindow(L"PowerPoint Slide Show - [My - Title]");
  full_screen_ppt_handler_->SetHeuristicForFindingEditor(
      /*use_heuristic=*/true);

  EXPECT_EQ(FindEditorWindow(), editor_window_info_.hwnd);
}

TEST_F(FullScreenWinApplicationHandlerTest,
       EditorWindowFoundWhenFullScreenPPTHandlerUsesSlideShowWindow) {
  CreateEditorWindow(L"My - Title - PowerPoint");

  // Create FullScreenPowerPointHandler using the slide show WindowInfo.
  HWND slide_show =
      CreateSlideShowWindow(L"PowerPoint Slide Show - [My - Title]");
  full_screen_ppt_handler_.reset(new FullScreenPowerPointHandler(
      reinterpret_cast<DesktopCapturer::SourceId>(slide_show)));
  full_screen_ppt_handler_->SetHeuristicForFindingEditor(
      /*use_heuristic=*/true);

  EXPECT_EQ(FindEditorWindow(), editor_window_info_.hwnd);
}

TEST_F(FullScreenWinApplicationHandlerTest,
       EditorWindowNotFoundWithHeuristicOff) {
  CreateEditorWindow(L"My - Title - PowerPoint");

  // Create FullScreenPowerPointHandler using the slide show WindowInfo.
  CreateSlideShowWindow(L"PowerPoint Slide Show - [My - Title]");
  full_screen_ppt_handler_->SetHeuristicForFindingEditor(
      /*use_heuristic=*/false);

  EXPECT_NE(FindEditorWindow(), editor_window_info_.hwnd);
}

TEST_F(FullScreenWinApplicationHandlerTest,
       EditorWindowNotFoundWithHeuristicOffAndPPTHandlerUsesSlideShowWindow) {
  CreateEditorWindow(L"My - Title - PowerPoint");

  // Create FullScreenPowerPointHandler using the slide show WindowInfo.
  HWND slide_show =
      CreateSlideShowWindow(L"PowerPoint Slide Show - [My - Title]");
  full_screen_ppt_handler_.reset(new FullScreenPowerPointHandler(
      reinterpret_cast<DesktopCapturer::SourceId>(slide_show)));
  full_screen_ppt_handler_->SetHeuristicForFindingEditor(
      /*use_heuristic=*/false);

  EXPECT_NE(FindEditorWindow(), editor_window_info_.hwnd);
}

}  // namespace webrtc
