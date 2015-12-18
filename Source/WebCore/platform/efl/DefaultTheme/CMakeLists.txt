set(Resources_DIR "${WEBCORE_DIR}/Resources")
macro(GENERATE_THEME _target_name _name _option)
    set(DefaultTheme_DIR "${WEBCORE_DIR}/platform/efl/DefaultTheme")

    add_custom_command(
        OUTPUT ${THEME_BINARY_DIR}/${_name}
        COMMAND ${EDJE_CC_EXECUTABLE} ${_option} default.edc ${THEME_BINARY_DIR}/${_name}
        DEPENDS
            default.edc
            widget/button/button.edc
            widget/button/img_button_focus.png
            widget/button/img_button_hover.png
            widget/button/img_button_normal.png
            widget/button/img_button_press.png
            widget/check/check.edc
            widget/check/img_check_off_focus.png
            widget/check/img_check_off_hover.png
            widget/check/img_check_off.png
            widget/check/img_check_on_focus.png
            widget/check/img_check_on_hover.png
            widget/check/img_check_on.png
            widget/combo/combo.edc
            widget/combo/combo_focus_button.png
            widget/combo/combo_focus_button_transparent.png
            widget/combo/combo_focus.png
            widget/combo/combo_focus_transparent.png
            widget/combo/combo_hover_button.png
            widget/combo/combo_hover_button_transparent.png
            widget/combo/combo_hover.png
            widget/combo/combo_hover_transparent.png
            widget/combo/combo_normal_button.png
            widget/combo/combo_normal_button_transparent.png
            widget/combo/combo_normal.png
            widget/combo/combo_normal_transparent.png
            widget/combo/combo_press_button.png
            widget/combo/combo_press_button_transparent.png
            widget/combo/combo_press.png
            widget/combo/combo_press_transparent.png
            widget/combo/icon.png
            widget/entry/entry.edc
            widget/entry/img_focused.png
            widget/entry/img_hovered.png
            widget/entry/img_normal.png
            widget/progressbar/bt_base.png
            widget/progressbar/progressbar.edc
            widget/progressbar/shelf_inset.png
            widget/radio/img_radio_off_focus.png
            widget/radio/img_radio_off_hover.png
            widget/radio/img_radio_off.png
            widget/radio/img_radio_on_focus.png
            widget/radio/img_radio_on_hover.png
            widget/radio/img_radio_on.png
            widget/radio/radio.edc
            widget/scrollbar/scrollbar.edc
            widget/scrollbar/scrollbar_bg_horizontal.png
            widget/scrollbar/scrollbar_bg_vertical.png
            widget/scrollbar/scrollbar_thumb_horizontal.png
            widget/scrollbar/scrollbar_thumb_vertical.png
            widget/search/cancel/cancel_normal_button2.png
            widget/search/cancel/cancel_normal_button.png
            widget/search/cancel/search_cancel.edc
            widget/search/decoration/decoration_normal_button.png
            widget/search/decoration/search_decoration.edc
            widget/search/field/field_focused.png
            widget/search/field/field_hovered.png
            widget/search/field/field_normal.png
            widget/search/field/search_field.edc
            widget/slider/slider.edc
            widget/slider/slider_fill_h.png
            widget/slider/slider_fill_v.png
            widget/slider/slider_h.png
            widget/slider/slider_thumb_h.png
            widget/slider/slider_thumb_press_h.png
            widget/slider/slider_thumb_press_v.png
            widget/slider/slider_thumb_v.png
            widget/slider/slider_v.png
            widget/spinner/sp_bg.png
            widget/spinner/sp_down_default.png
            widget/spinner/sp_down_hover.png
            widget/spinner/sp_down_pressed.png
            widget/spinner/spinner.edc
            widget/spinner/sp_up_default.png
            widget/spinner/sp_up_hover.png
            widget/spinner/sp_up_pressed.png
        WORKING_DIRECTORY ${DefaultTheme_DIR}
        VERBATIM
    )

    add_custom_target(${_target_name} ALL
        DEPENDS ${THEME_BINARY_DIR}/${_name}
    )
endmacro()

set(DefaultTheme_DEFINITION "")

GENERATE_THEME(DefaultTheme "default.edj" "${DefaultTheme_DEFINITION}")
install(FILES "${THEME_BINARY_DIR}/default.edj" DESTINATION ${DATA_INSTALL_DIR}/themes)

if (ENABLE_API_TESTS)
    GENERATE_THEME(BigButtonTheme "big_button_theme.edj" "-DBIG_BUTTON_THEME_FOR_TESTING")
endif ()
