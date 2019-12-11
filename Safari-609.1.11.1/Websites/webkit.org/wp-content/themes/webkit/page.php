<?php get_header(); ?>

    <?php if (have_posts()) : while (have_posts()) : the_post(); ?>

        <article class="page<?php if ( has_table_of_contents() ) echo ' with-toc';?>" id="post-<?php the_ID(); ?>">

            <h1><a href="<?php echo get_permalink() ?>" rel="bookmark" title="Permanent Link: <?php the_title(); ?>"><?php the_title(); ?></a></h1>

            <div class="bodycopy">
                <?php table_of_contents(); ?>

                <?php the_content('<p class="serif">Read the rest of this entry &gt;&gt;</p>'); ?>

                <?php link_pages('<p><strong>Pages:</strong> ', '</p>', 'number'); ?>

            </div>
        </article>

    <?php //comments_template(); ?>

    <?php endwhile; else:
        include('444.php');
    endif; ?>

<?php get_footer(); ?>