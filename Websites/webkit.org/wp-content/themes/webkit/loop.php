<?php

$background_image = '';
if ( $post_thumbnail_id = get_post_thumbnail_id() ) {
    $post_thumbnail_url = wp_get_attachment_url( $post_thumbnail_id );
    $background_image = " data-url=\"" . $post_thumbnail_url . "\"";
}

$classes = array('tile', 'third-tile');
$classes[] = 'third-tile';
?>
    <div <?php echo post_class(join(' ', $classes)); ?>>
        <a class="tile-link" href="<?php the_permalink(); ?>"><?php the_title(); ?></a>
        <div class="background-image">
            <div class="featured-image"<?php echo $background_image; ?>></div>
            <svg viewbox="0 0 100 100">
                <use xlink:href="#<?php echo esc_attr(get_post_icon()); ?>" />
            </svg>
            <?php if ( $featured ): ?><div class="background-vignette"></div><?php endif; ?>
        </div>
        <div class="tile-content">
            <h1><?php the_title(); ?></h1>
            <div class="summary"><?php the_excerpt(); ?></div>
            <p><a href="<?php the_permalink(); ?>" class="readmore">Read more</a></p>
        </div>
    </div>