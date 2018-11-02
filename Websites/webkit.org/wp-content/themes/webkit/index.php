<?php get_header(); ?>

        <?php if ( have_posts() ) 
            include_post_icons(); ?>
            <div id="posts" class="tiles">
            <?php

            $count = 1;
            while ( have_posts() ) : the_post();
                get_template_part( 'loop', get_post_format() );
                $offset = $count++ % 3;
            endwhile;

            if ( $offset > 0 ) {
                for ($offset; $offset < 3; $offset++)
                    echo "<div class=\"tile third-tile spacer\"></div>";
            }

            ?>
            </div>

        <?php
            the_posts_pagination( array(
                'prev_text'          => __( 'Previous page' ),
                'next_text'          => __( 'Next page' ),
                'before_page_number' => '<span class="meta-nav screen-reader-text">' . __( 'Page' ) . ' </span>',
            ) );

        else :
            get_template_part( 'loop', 'none' );

        endif;
        ?>

<?php get_footer(); ?>