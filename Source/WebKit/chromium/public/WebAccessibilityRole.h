/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WebAccessibilityRole_h
#define WebAccessibilityRole_h

#include "platform/WebCommon.h"

namespace WebKit {

// These values must match WebCore::AccessibilityRole values
enum WebAccessibilityRole {
    WebAccessibilityRoleAnnotation = 1,
    WebAccessibilityRoleApplication,
    WebAccessibilityRoleApplicationAlert,
    WebAccessibilityRoleApplicationAlertDialog,
    WebAccessibilityRoleApplicationDialog,
    WebAccessibilityRoleApplicationLog,
    WebAccessibilityRoleApplicationMarquee,
    WebAccessibilityRoleApplicationStatus,
    WebAccessibilityRoleApplicationTimer, 
    WebAccessibilityRoleBrowser,
    WebAccessibilityRoleBusyIndicator,
    WebAccessibilityRoleButton,
    WebAccessibilityRoleCell, 
    WebAccessibilityRoleCheckBox,
    WebAccessibilityRoleColorWell,
    WebAccessibilityRoleColumn,
    WebAccessibilityRoleColumnHeader,
    WebAccessibilityRoleComboBox,
    WebAccessibilityRoleDefinitionListTerm,
    WebAccessibilityRoleDefinitionListDefinition,
    WebAccessibilityRoleDirectory,
    WebAccessibilityRoleDisclosureTriangle,
    WebAccessibilityRoleDiv,
    WebAccessibilityRoleDocument,
    WebAccessibilityRoleDocumentArticle,
    WebAccessibilityRoleDocumentMath,
    WebAccessibilityRoleDocumentNote,
    WebAccessibilityRoleDocumentRegion,
    WebAccessibilityRoleDrawer,
    WebAccessibilityRoleEditableText,
    WebAccessibilityRoleFooter,
    WebAccessibilityRoleForm,
    WebAccessibilityRoleGrid,
    WebAccessibilityRoleGroup,
    WebAccessibilityRoleGrowArea,
    WebAccessibilityRoleHeading,
    WebAccessibilityRoleHelpTag,
    WebAccessibilityRoleIgnored,
    WebAccessibilityRoleImage,
    WebAccessibilityRoleImageMap,
    WebAccessibilityRoleImageMapLink,
    WebAccessibilityRoleIncrementor,
    WebAccessibilityRoleLabel,
    WebAccessibilityRoleLandmarkApplication,
    WebAccessibilityRoleLandmarkBanner,
    WebAccessibilityRoleLandmarkComplementary,
    WebAccessibilityRoleLandmarkContentInfo,
    WebAccessibilityRoleLandmarkMain,
    WebAccessibilityRoleLandmarkNavigation,
    WebAccessibilityRoleLandmarkSearch,
    WebAccessibilityRoleLink,
    WebAccessibilityRoleList,
    WebAccessibilityRoleListBox,
    WebAccessibilityRoleListBoxOption,
    WebAccessibilityRoleListItem,
    WebAccessibilityRoleListMarker,
    WebAccessibilityRoleMatte,
    WebAccessibilityRoleMenu,
    WebAccessibilityRoleMenuBar,
    WebAccessibilityRoleMenuButton,
    WebAccessibilityRoleMenuItem,
    WebAccessibilityRoleMenuListPopup,
    WebAccessibilityRoleMenuListOption,
    WebAccessibilityRoleOutline,
    WebAccessibilityRoleParagraph,
    WebAccessibilityRolePopUpButton,
    WebAccessibilityRolePresentational,
    WebAccessibilityRoleProgressIndicator,
    WebAccessibilityRoleRadioButton,
    WebAccessibilityRoleRadioGroup,
    WebAccessibilityRoleRowHeader,
    WebAccessibilityRoleRow,
    WebAccessibilityRoleRuler,
    WebAccessibilityRoleRulerMarker,
    WebAccessibilityRoleScrollArea,
    WebAccessibilityRoleScrollBar,
    WebAccessibilityRoleSheet,
    WebAccessibilityRoleSlider,
    WebAccessibilityRoleSliderThumb,
    WebAccessibilityRoleSpinButton,
    WebAccessibilityRoleSpinButtonPart,
    WebAccessibilityRoleSplitGroup,
    WebAccessibilityRoleSplitter,
    WebAccessibilityRoleStaticText,
    WebAccessibilityRoleSystemWide,
    WebAccessibilityRoleTabGroup,
    WebAccessibilityRoleTabList,
    WebAccessibilityRoleTabPanel,
    WebAccessibilityRoleTab,
    WebAccessibilityRoleTable,
    WebAccessibilityRoleTableHeaderContainer,
    WebAccessibilityRoleTextArea,
    WebAccessibilityRoleTreeRole,
    WebAccessibilityRoleTreeGrid,
    WebAccessibilityRoleTreeItemRole,
    WebAccessibilityRoleTextField,
    WebAccessibilityRoleToolbar,
    WebAccessibilityRoleUnknown,
    WebAccessibilityRoleUserInterfaceTooltip,
    WebAccessibilityRoleValueIndicator,
    WebAccessibilityRoleWebArea,
    WebAccessibilityRoleWebCoreLink,
    WebAccessibilityRoleWindow,
};

} // namespace WebKit

#endif
