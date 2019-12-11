<?php
/**
 * WebKitPageTileWidget class
 * A WordPress widget to show a post tile on the home page
 **/

defined('WPINC') || header('HTTP/1.1 403') & exit; // Prevent direct access

if ( ! class_exists('WP_Widget') ) return;

class WebKitPageTileWidget extends WebKitPostTileWidget {

    public function __construct() {
        parent::WP_Widget(false,
            __('Page Tile'),
            array('description' => __('Page tile for the home page'))
        );
    }

    public function load( array $options = array() ) {
        return new WP_Query(array(
            'post_type' => 'page',
            'page_id' => $options['page']
        ));
    }

    function form( array $options ) {
        if ( empty( $options['link'] ) ) $options['link'] = __('Read more');
        ?>
        <p><label for="<?php echo $this->get_field_id('page'); ?>"><?php _e( 'Page' ); ?></label>
        <?php echo wp_dropdown_pages( array( 'name' => $this->get_field_name('page'), 'echo' => 0, 'show_option_none' => __( '&mdash; Select &mdash;' ), 'option_none_value' => '0', 'selected' => $options['page'] ) ); ?></p>

        <p><label for="<?php echo $this->get_field_id('title'); ?>"><?php _e('Title'); ?></label>
        <input type="text" name="<?php echo $this->get_field_name('title'); ?>" id="<?php echo $this->get_field_id('title'); ?>" class="widefat" value="<?php echo $options['title']; ?>" placeholder="Current Page Title"></p>

        <p><label for="<?php echo $this->get_field_id('summary'); ?>"><?php _e('Summary'); ?></label>
        <input type="text" name="<?php echo $this->get_field_name('summary'); ?>" id="<?php echo $this->get_field_id('summary'); ?>" class="widefat" value="<?php echo esc_attr($options['summary']); ?>" placeholder="Current Page Excerpt"></p>

        <p><label for="<?php echo $this->get_field_id('link'); ?>"><?php _e('Call to Action'); ?></label>
        <input type="text" name="<?php echo $this->get_field_name('link'); ?>" id="<?php echo $this->get_field_id('link'); ?>" class="widefat" value="<?php echo $options['link']; ?>"></p>

        <p><label for="<?php echo $this->get_field_id('vignette'); ?>"><?php _e( 'Vignette' ); ?></label>
        <select  name="<?php echo $this->get_field_name('vignette'); ?>" id="<?php echo $this->get_field_id('vignette'); ?>">
            <option value="light">Light</option>
            <option value="dark"<?php if ( 'dark' == $options['link'] ) echo " checked=\"checked\""; ?>>Dark</option>
        </select></p>

        <p><label for="<?php echo $this->get_field_id('featured'); ?>">
            <input type="hidden" name="<?php echo $this->get_field_name('featured'); ?>" value="off">
            <input type="checkbox" name="<?php echo $this->get_field_name('featured'); ?>" id="<?php echo $this->get_field_id('featured'); ?>" class="widefat" value="on" <?php echo ( 'on' == $options['featured'] ) ? 'checked' : ''; ?>>
            <?php _e('Featured'); ?></label>
        </p>
        <?php
    }

} // END class WebKitPageTileWidget

register_widget('WebKitPageTileWidget');
