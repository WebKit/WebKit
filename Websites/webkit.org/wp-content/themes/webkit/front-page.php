<?php get_header(); ?>
<div id="posts" class="tiles">
    <?php

include_post_icons();
Front_Page_Posts::object(); // Initialize Front Page Posts query

if ( ! dynamic_sidebar('Home Tiles') ):
    $Query = Front_Page_Posts::WP_Query();
    while ( $Query->have_posts() ):
        $Query->the_post();
        get_template_part('loop');
    endwhile;
endif;

?>
</div>

<div class="pagination">
<a href="<?php echo get_permalink( get_option( 'page_for_posts' ) ) ?>" class="page-numbers next-button"><?php _e('More Blog Posts')?></a>
</div>

<?php get_footer(); ?>