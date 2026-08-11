<?php
/*
Plugin Name: Strong Passwords
Description: Enforce strong passwords
Version:     1.0
Author:      Jonathan Davis
Author URI:  http://webkit.org
*/

function enforce_strong_passwords () {
    wp_add_inline_script('user-profile','
        jQuery(document).ready(function($) {
            $weakRow = $(".pw-weak").remove();
            $pass1 = $("#pass1");
            $pass1Text = $("#pass1-text");
            $submitButtons = $("input[type=submit]");
            
            function allowOrDisableSubmit() {
                var passStrength = $("#pass-strength-result")[0];

                if ( passStrength.className ) {
                    if ( $( passStrength ).is( ".short, .bad, .good" ) ) {
                        $submitButtons.prop( "disabled", true );
                    } else {
                        $submitButtons.prop( "disabled", false );
                    }
                }
            }

            currentPass = $pass1.val();
            $pass1.on( "input pwupdate", function () {
                if ( $pass1.val() === currentPass ) {
                    return;
                }
                allowOrDisableSubmit();
            } );
        });
    ','after');
}

add_action( 'admin_enqueue_scripts', 'enforce_strong_passwords' );
add_action( 'login_enqueue_scripts', 'enforce_strong_passwords' );