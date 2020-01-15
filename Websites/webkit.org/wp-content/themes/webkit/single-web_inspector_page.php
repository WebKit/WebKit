<?php get_header();
if (have_posts()) : while (have_posts()) : the_post(); ?>
    <article class="page<?php if ( has_table_of_contents() ) echo ' with-toc';?>" id="post-<?php the_ID(); ?>">

        <h1><a href="<?php echo get_post_type_archive_link('web_inspector_page'); ?>" class="landing-link">Web Inspector Reference</a><a href="<?php echo get_permalink() ?>" rel="bookmark" title="Permanent Link: <?php the_title(); ?>"><?php the_title(); ?></a></h1>

        <div class="bodycopy">
            <?php table_of_contents(); ?>

            <?php the_content('<p class="serif">Read the rest of this entry &gt;&gt;</p>'); ?>

            <?php link_pages('<p><strong>Pages:</strong> ', '</p>', 'number'); ?>

        </div>

        <div class="meta">
            <p class="written">Written <?php the_date(); ?> by <?php the_author() ?></p>
            <p class="updated">Last updated <?php the_modified_date(); ?> by <?php the_modified_author(); ?></p>
        </div>
    </article>
<?php 
endwhile;
else:
    include('444.php');
endif; 
get_footer(); ?>