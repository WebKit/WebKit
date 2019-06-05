<?php
/**
 * WebKitPostTileWidget class
 * A WordPress widget to show a post tile on the home page
 **/

defined('WPINC') || header('HTTP/1.1 403') & exit; // Prevent direct access

if ( ! class_exists('WP_Widget') ) return;

class WebKitPostTileWidget extends WP_Widget {

    public function __construct() {
        parent::WP_Widget(false,
            __('Post Tile'),
            array('description' => __('Post tile for the home page'))
        );
    }

    public function load( array $options = array() ) {
        return Front_Page_Posts::WP_Query();
    }

    public function widget( array $args, array $options ) {
        $Query = $this->load($options);

        // Get the next post, if available
        if ( ! $Query->have_posts() ) return;

        // Queue the post data
        $Query->the_post();

        $featured = ( 'on' == $options['featured'] );
        if ( ! $featured ) {
            // Prevent Safari Technology Preview release note posts from showing up when not a featured post.
            while ( in_category('safari-technology-preview') && $Query->have_posts() ) {
                $Query->the_post();
                continue;
            }
        }

        if ( ! empty($args) )
            extract($args, EXTR_SKIP);

        $title = $before_title . ( ! empty($options['title']) ? $options['title'] : get_the_title() ) . $after_title;
        $summary = ! empty($options['summary']) ? $options['summary'] : get_the_excerpt();
        $link = ! empty($options['link']) ? $options['link'] : __('Read more');

        $image = '';
        if ( $post_thumbnail_id = get_post_thumbnail_id() ) {
            $post_thumbnail_url = wp_get_attachment_url( $post_thumbnail_id );
            $image = " data-url=\"" . $post_thumbnail_url . "\"";
        }

        $classes = array('tile');
        if ( $featured ) {
            $classes[] = 'featured-tile';
            $classes[] = 'two-thirds-tile';
        } else {
            $classes[] = 'third-tile';
        }

        if ( isset($options['vignette']) && 'dark' == $options['vignette'] )
            $classes[] = 'tag-dark';

        ?>
            <div <?php echo post_class(join(' ', $classes)); ?>>
                <a class="tile-link" href="<?php the_permalink(); ?>"><?php echo $title; ?></a>
                <div class="background-image">
                    <svg viewbox="0 0 100 100">
                        <use xlink:href="#<?php echo esc_attr(get_post_icon()); ?>" aria-label="<?php echo esc_attr(str_replace('-',' ', get_post_icon())); ?> icon" />
                    </svg>
                    <div class="featured-image"<?php echo $image; ?>></div>
                    <?php if ( $featured ): ?><div class="background-vignette"></div><?php endif; ?>
                </div>
                <div class="tile-content">
                    <h1><?php echo $title; ?></h1>
                    <div class="summary"><?php echo $summary; ?></div>
                    <p><a href="<?php the_permalink(); ?>" class="readmore"><?php echo $link; ?></a></p>
                </div>
            </div>
        <?php
    }

    public function form( array $options ) {
        if ( empty( $options['link'] ) ) $options['link'] = __('Read more');
        ?>
        <p><label for="<?php echo $this->get_field_id('title'); ?>"><?php _e('Title'); ?></label>
        <input type="text" name="<?php echo $this->get_field_name('title'); ?>" id="<?php echo $this->get_field_id('title'); ?>" class="widefat" value="<?php echo $options['title']; ?>" placeholder="Current Post Title"></p>

        <p><label for="<?php echo $this->get_field_id('summary'); ?>"><?php _e('Summary'); ?></label>
        <input type="text" name="<?php echo $this->get_field_name('summary'); ?>" id="<?php echo $this->get_field_id('summary'); ?>" class="widefat" value="<?php echo $options['summary']; ?>" placeholder="Current Post Excerpt"></p>

        <p><label for="<?php echo $this->get_field_id('link'); ?>"><?php _e('Call to Action'); ?></label>
        <input type="text" name="<?php echo $this->get_field_name('link'); ?>" id="<?php echo $this->get_field_id('link'); ?>" class="widefat" value="<?php echo $options['link']; ?>"></p>

        <p><label for="<?php echo $this->get_field_id('featured'); ?>">
            <input type="hidden" name="<?php echo $this->get_field_name('featured'); ?>" value="off">
            <input type="checkbox" name="<?php echo $this->get_field_name('featured'); ?>" id="<?php echo $this->get_field_id('featured'); ?>" class="widefat" value="on" <?php echo ( 'on' == $options['featured'] ) ? 'checked' : ''; ?>>
            <?php _e('Featured'); ?></label>
        </p>
        <?php
    }

} // END class WebKitPostTileWidget

register_widget('WebKitPostTileWidget');