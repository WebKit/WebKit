<?php
/**
 * WebKitPostTileWidget class
 * A WordPress widget to show an icon tile on the home page
 **/

defined('WPINC') || header('HTTP/1.1 403') & exit; // Prevent direct access

if ( ! class_exists('WP_Widget') ) return;

class WebKitIconTileWidget extends WP_Widget {

    function __construct() {
        parent::WP_Widget(false,
            __('Icon Tile'),
            array('description' => __('Icon tile for the home page'))
        );
    }

    function widget($args, $options) {
        if ( ! empty($args) )
            extract($args, EXTR_SKIP);

        $classes = array('third-tile', 'tile', 'icon-tile');
        $classes[] = (string)$options['color'];
        $classes[] = (string)$options['icon'];
        $text = (string)$options['text'];
        $subtext = (string)$options['subtext'];
        $url = (string)$options['url'];

        ?>
        <div class="<?php echo esc_attr(join(' ', $classes)); ?>">
            <a class="tile-link" href="<?php echo esc_url($url); ?>"><?php echo nl2br(esc_html($text)); ?></a>
            <div class="icon"></div>
            <h2><?php echo nl2br(esc_html($text)); ?></h2>
            <?php if (!empty($subtext)): ?><p><?php echo nl2br(esc_html($subtext)); ?></p><?php endif; ?>
        </div>
        <?php
    }

    function form($options) {
        ?>
        <p><label for="<?php echo $this->get_field_id('color'); ?>"><?php _e('Color'); ?></label>
        <select name="<?php echo $this->get_field_name('color'); ?>">
        <?php echo html_select_options(array(
            'gray-tile' => 'Gray',
            'blue-tile' => 'Blue',
            'goldenrod-tile' => 'Goldenrod',
            'amber-tile' => 'Amber'
        ), $options['color'], true); ?>
        </select></p>

        <p><label for="<?php echo $this->get_field_id('icon'); ?>"><?php _e('Icon'); ?></label>
        <select name="<?php echo $this->get_field_name('icon'); ?>">
        <?php echo html_select_options(array(
            'compass' => 'Compass',
            'download' => 'Download',
            'developer' => 'Developer'
        ), $options['icon'], true); ?>
        </select></p>

        <p><label for="<?php echo $this->get_field_id('text'); ?>"><?php _e('Text'); ?></label>
        <textarea type="text" name="<?php echo $this->get_field_name('text'); ?>" id="<?php echo $this->get_field_id('text'); ?>" class="widefat"><?php echo $options['text']; ?></textarea></p>

        <p><label for="<?php echo $this->get_field_id('subtext'); ?>"><?php _e('Sub Text'); ?></label>
        <textarea type="text" name="<?php echo $this->get_field_name('subtext'); ?>" id="<?php echo $this->get_field_id('subtext'); ?>" class="widefat"><?php echo $options['subtext']; ?></textarea></p>

        <p><label for="<?php echo $this->get_field_id('url'); ?>"><?php _e('Link URL'); ?></label>
        <input type="text" name="<?php echo $this->get_field_name('url'); ?>" id="<?php echo $this->get_field_id('url'); ?>" class="widefat" value="<?php echo $options['url']; ?>"></p>

        <?php
    }

} // END class WebKitIconTileWidget

register_widget('WebKitIconTileWidget');
