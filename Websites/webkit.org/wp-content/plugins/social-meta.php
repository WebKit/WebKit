<?php
/*
Plugin Name: Social Meta
Description: Adds schema.org, Twitter Card, Open Graph meta for posts and pages
Version:     1.0
Author:      Jonathan Davis
Author URI:  http://webkit.org
*/

add_action('wp_head', function() { ?>
    <?php
        $image_src = wp_get_attachment_image_src( get_post_thumbnail_id($post_id), 'large' );
        $image_url = $image_src[0];
    ?>

    <!-- Schema.org markup -->
    <meta itemprop="name" content="<?php the_title(); ?>">
    <meta itemprop="description" content="<?php the_excerpt(); ?>">
    <?php if ( $image_url ): ?>
    <meta itemprop="image" content="<?php echo $image_url; ?>">
    <?php endif; ?>

    <!-- Twitter Card data -->
    <meta name="twitter:card" content="summary_large_image">
    <meta name="twitter:site" content="@webkit">
    <meta name="twitter:title" content="<?php the_title(); ?>">
    <meta name="twitter:description" content="<?php the_excerpt(); ?>">
    <?php if ( '' !== ( $twitter_handle = get_the_author_meta('twitter') ) ): ?>
    <meta name="twitter:creator" content="@<?php echo esc_html($twitter_handle); ?>">
    <?php endif; ?>
    <?php if ( $image_url ): // Twitter summary card with large image must be at least 280x150px ?>
    <meta name="twitter:image:src" content="<?php echo $image_url; ?>">
    <?php endif; ?>

    <!-- Open Graph data -->
    <meta property="og:title" content="<?php the_title(); ?>" />
    <meta property="og:type" content="article" />
    <meta property="og:url" content="<?php the_permalink(); ?>" />
    <?php if ( $image_url ): ?>
    <meta itemprop="og:image" content="<?php echo $image_url; ?>">
    <?php endif; ?>
    <meta property="og:description" content="<?php the_excerpt(); ?>" />
    <meta property="og:site_name" content="<?php bloginfo('title'); ?>" />
    <meta property="article:published_time" content="<?php the_time('c'); ?>" />
    <meta property="article:modified_time" content="<?php the_modified_date('c'); ?>" />
    <?php
        $categories = wp_get_object_terms( get_the_ID(), 'category', array( 'fields' => 'names' ) );
        $tags = wp_get_object_terms( get_the_ID(), 'post_tag', array( 'fields' => 'names' ) );

        if ( ! empty($categories) ):
            $section = array_shift($categories);      // The first category is used as the section
            $tags = array_merge($categories, $tags);  // The rest are prepended to the tag list
    ?>
    <meta property="article:section" content="<?php echo esc_attr($section); ?>" />
    <?php
        endif;

        if ( ! empty($tags) ): foreach( $tags as $tag ):
    ?>
    <meta property="article:tag" content="<?php echo esc_attr($tag); ?>" />
    <?php
        endforeach; endif;
    ?>

<?php
});