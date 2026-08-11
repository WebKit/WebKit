<?php
/**
 * Template Name: Nightly Survey
 **/

$Survey = new WebKit_Nightly_Survey();
$Survey->process();

get_header();
?>
    <style>
        body {
            background: #333333;
        }

        main {
            background: none;
        }

        header {
            background-color: rgba(0,0,0,0.1);
        }

        #nightly {
            margin: 6rem auto;
            color: #ffffff;
        }

        #nightly h1 {
            text-align: center;
            margin-bottom: 3rem;
            margin-top: 0;
            color: #ffffff;
            font-weight: 100;
            font-size: 9rem;
            line-height: 9rem;
        }

        #nightly .bodycopy > p:first-child {
            color: #FFD15E;
            font-size: 3rem;
            line-height: 50vh;
            font-weight: 200;
            text-align: center;
        }


        #nightly .bodycopy {
            max-width: 64rem;
        }

        #nightly ul {
            width: 100%;
            margin-top: 3rem;
            padding-left: 0;
        }

        #nightly ul li {
            display: block;
            margin: 0 3rem;
            position: relative;
            vertical-align: top;
            box-sizing: border-box;
            overflow: hidden;
            min-height: 62px;
            perspective: 500px;
        }

        #nightly input[type=text] {
            display: inline-block;
            vertical-align: baseline;
            margin-left: 1rem;
            margin-bottom: 0;
            backface-visibility: hidden;
            transform: rotateX( 90deg );
            transition: transform 500ms ease;
            visibility: hidden;
        }

        #nightly input[type=radio] {
            margin-top: 0;
            vertical-align: middle;
            transition: opacity 500ms ease;
        }

        #nightly input[type=radio]:checked ~ input[type=text] {
            transform: rotateX( 0deg );
            visibility: visible;
        }

        #nightly input[type=radio]:checked,
        #nightly input[type=radio]:checked + span {
            opacity: 1;
            color: #ffffff;
        }

        #nightly input[type=radio] + span {
            color: #878787;
            transition: color 500ms ease;
        }

        #nightly input[type=submit] {
            padding: 1rem 3rem;
        }

        #nightly label {
            box-sizing: border-box;
            display: inline-block;
            border-radius: 0.3rem;
            padding: 1rem 3rem 1rem 5rem;
            transition: background-color 500ms ease;
            cursor: pointer;
            text-indent: -2.4rem;
        }

        #nightly ul label:hover {
            background: rgba(255, 255, 255, 0.1);
            opacity: 1;
        }

        #nightly code {
            background-color: rgba(242, 242, 242, 0.1);
            border-color: rgba(230, 230, 230, 0.1);
            color: #999999;
        }

        #nightly h5 {
            text-align: center;
            font-weight: normal;
            font-size: 1.8rem;
        }

        #nightly .pagination .page-numbers {
            background-color: rgba(255, 255, 255, 0.1);
            color: #ffffff;
        }

        #nightly .pagination .page-numbers:hover {
            background-color: rgba(255, 255, 255, 1);
            color: #08c;
        }

        hr {
            border-color: #777777;
        }

        #footer-nav a {
            color: #999999;
        }

    </style>

    <?php if (have_posts()) : while (have_posts()) : the_post(); ?>

        <article class="page" id="nightly">

            <h1><?php the_title(); ?></h1>

            <div class="bodycopy">
                <?php if (WebKit_Nightly_Survey::responded()): ?>
                    <?php the_content(''); ?>
                    <nav class="navigation pagination">
                        <a href="/nightly/start" class="page-numbers next-post">Return<span>to the WebKit Start Page</span></a>
                    </nav>
                <?php else: ?>
                <form name="webkit-nightly-survey" action="" method="POST">
                <?php
                echo WebKit_Nightly_Survey::form_nonce();

                $Survey = WebKit_Nightly_Survey::survey();
                foreach ($Survey as $id => $SurveyQuestion) {
                    echo "<h3>$SurveyQuestion->question</h3>";

                    echo "<ul>";
                    foreach ($SurveyQuestion->responses as $value => $response) {
                        echo '<li><label><input type="radio" name="questions[' . $id . ']" value="' . $value . '" required> <span>' . $response . '</span>';
                        if ($response == "Other:") {
                            echo '<input type="text" name="other[' . $id . ']" maxlength="144">';
                        }
                        echo '</label></li>';
                    }
                    echo "</ul>";
                }
                ?>
                <p class="alignright"><input type="submit" name="Submit" value="Submit" class="submit-button"></p>
                </form>
                <?php endif;?>
            </div>

        </article>

    <?php endwhile; endif; ?>

<?php get_footer(); ?>