<?php
/**
 * Template Name: Nightly Start Page
 **/

$latest_nightly_build = get_nightly_build();
$latest = intval($latest_nightly_build[0]);

$branch = get_query_var('nightly_branch');
$build = intval(get_query_var('nightly_build'));

get_header();

add_filter('the_content', function ($content) {
    $branch = get_query_var('nightly_branch');
    $build = intval(get_query_var('nightly_build'));

    if (empty($Build) || empty($branch) || $build < 11976) {
        $content = preg_replace('/<p>(.*?)%1\$s<\/p>/', '<p></p>', $content);
    } else {
        $content = sprintf($content, "r$build", $branch);
    }

    return $content;
});
?>
<style>
body {
    background-color: #333333;
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
    margin-bottom: 0;
    margin-top: 0;
    color: #ffffff;
    font-weight: 100;
    font-size: 9rem;
    line-height: 9rem;
}

#nightly h1 + p {
    color: #FFD15E;
    font-size: 3rem;
    line-height: 4.2rem;
    font-weight: 200;
}

#nightly img {
    width: 15rem;
}

#nightly .bodycopy {
    text-align: center;
}

#nightly ul {
    display: -webkit-flex;
    display: flex;
    -webkit-flex-wrap: wrap;
    flex-wrap: wrap;
    -webkit-justify-content: space-between;
    justify-content: space-between;
    box-sizing: border-box;
    width: 100%;
    margin-top: 3rem;
    padding-left: 0;
}

#nightly ul li {
    display: inline-block;
    margin: 0 0 15px;
    position: relative;
    vertical-align: top;
    box-sizing: border-box;
    min-height: 100px;
    overflow: hidden;
    width: calc(33.33% - 10px);
    border-radius: 0.3rem;
    padding: 1rem;
    transition: opacity 300ms linear, background-color 500ms ease, transform 300ms ease; /* ease-out-exponential */
    opacity: 0.5;
}

#nightly ul li:hover {
    background: rgba(255, 255, 255, 0.1);
    opacity: 1;
    transform: scale(1.05);
}

#nightly ul li .icon {
    width: 5rem;
    margin: 1rem auto;
    display: block;
}

#nightly ul li a {
    text-decoration: none;
    display: block;
    position: absolute;
    top: 0;
    left: 0;
    width: 100%;
    height: 100%;
    font-size: 0;
    z-index: 1;
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

#nightly ol {
    margin-top: 0;
    text-align: left;
    color: #999999;
    font-size: 1.8rem;
    list-style: none;
    padding-left: 0;
}

#nightly h3 {
    position: relative;
    width: 100%;
    background-color: #202020;
    color: #ffffff;
    border-radius: 0.3rem;
    padding: 3rem;
    font-weight: 500;
    font-size: 3rem;
    text-align: left;
    box-sizing: border-box;
    transition: opacity 300ms ease, transform 300ms ease;
}

#nightly h3:hover {
    transform: scale(1.05);
}

#nightly h3 .icon {
    width: 7rem;
    margin: 0rem 2rem;
    display: block;
    float: left;
}

#nightly h3 > a {
    text-decoration: none;
    display: block;
    position: absolute;
    font-size: 0;
    top: 0;
    left: 0;
    width: 100%;
    height: 100%;
    font-size: 0;
    z-index: 1;
}

<?php if (WebKit_Nightly_Survey::responded()): ?>#nightly h3 { display: none; }<?php endif; ?>

#icons-sprite {
    display: none;
}

hr {
    border-color: #777777;
}

#footer-nav a {
    color: #999999;
}

.update-nag {
    background-color: #FF9D00;
    color: #000000;
    width: 100%;
    top: 0;
    left: 0;
    position: absolute;
    padding-bottom: 1.5rem;
    padding-top: 11.5rem;
}

.admin-bar .update-nag {
    padding-top: 14.5rem;
}

.update-nag a {
    color: #333333;
}

.update-nag p {
    text-align: center;
    font-size: 2rem;
}

.update-nag + #nightly {
    margin-top: 9rem;
}

@media only screen and (max-width: 690px) {
    #nightly li {
        width: calc(100% - 1px);
    }
}

</style>
<svg xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink" version="1.1" id="icons-sprite">
    <style>
        svg,symbol { color: white; }
    </style>
    <defs>
        <symbol id="cycle">
            <path fill="none" stroke="currentColor" stroke-width="2.5" d="M11.2720779,11.2720779 C4.24264069,18.3015152 4.24264069,29.6984848 11.2720779,36.7279221 C16.0613936,41.5172377 22.878121,43.0434836 28.9596165,41.3066596 M36.7279221,36.7279221 C43.7573593,29.6984848 43.7573593,18.3015152 36.7279221,11.2720779 C31.9713395,6.51549533 25.2149636,4.97753141 19.1651795,6.65818618 M32.4852814,40.9705627 L40.9705627,40.9705627 L32.4852814,32.4852814 L32.4852814,40.9705627 L32.4852814,40.9705627 L32.4852814,40.9705627 L32.4852814,40.9705627 Z M15.5147186,7.02943725 L15.5147186,15.5147186 L7.02943725,7.02943725 L15.5147186,7.02943725 L15.5147186,7.02943725 L15.5147186,7.02943725 L15.5147186,7.02943725 Z"/>
        </symbol>

        <symbol id="landing">
            <path d="M25 39.6894531C29.4337391 39.6894531 33.0279948 38.7092016 33.0279948 37.5 33.0279948 36.2907984 29.4337391 35.3105469 25 35.3105469 20.5662609 35.3105469 16.9720052 36.2907984 16.9720052 37.5 16.9720052 38.7092016 20.5662609 39.6894531 25 39.6894531zM25 42.9736328C33.464411 42.9736328 40.3261719 40.5230039 40.3261719 37.5 40.3261719 34.4769961 33.464411 32.0263672 25 32.0263672 16.535589 32.0263672 9.67382812 34.4769961 9.67382812 37.5 9.67382812 40.5230039 16.535589 42.9736328 25 42.9736328zM24.7290081 3.50148293C24.7290081 3.50148293 18.3504449 8.20808946 18.3504449 14.6816869 18.3504449 17.593729 21.0244162 23.9842129 21.0244162 23.9842129L28.4224355 23.9954394C28.4224355 23.9954394 31.1061539 17.5948742 31.1061539 14.6816869 31.1061539 8.20808946 24.7290081 3.50148293 24.7290081 3.50148293zM25.0124629 23.0160712L25.0124629 16.784522 25.0124629 16.0055783 24.8074778 16.0055783 24.8074778 16.784522 24.8074778 23.0160712 24.8074778 23.7950148 25.0124629 23.7950148 25.0124629 23.0160712M27 24L35.1994068 24 35.1994068 20.972726 31.0997034 17.0553266M22.4494068 24L14.25 24 14.25 20.972726 18.3497034 17.0553266" fill="none" stroke="currentColor" stroke-width="2.5"/>
        </symbol>

        <symbol id="patches">
            <g fill="none" stroke="currentColor" stroke-width="2.5">
                <polyline points="20.842 6 14.526 7.499 14.526 19.494 12 23.992 14.526 28.49 14.526 40.485 20.842 41.984" />
                <polyline points="28 6 34.316 7.499 34.316 19.494 36.842 23.992 34.316 28.49 34.316 40.485 28 41.984" />
            </g>
        </symbol>

        <symbol id="priority">
            <path fill="none" stroke="currentColor" stroke-width="2.5" d="M24,4.13378906 L43.8662109,43.8662109 L4.13378906,43.8662109 L24,4.13378906 Z M23.7675781,37.1438117 L24.2675781,37.1438117 L24.2675781,37.6438117 L23.7675781,37.6438117 L23.7675781,37.1438117 Z M23.7675781,31.3879523 L24.2675781,31.3879523 L24.2675781,18.1438117 L23.7675781,18.1438117 L23.7675781,31.3879523 Z"/>
        </symbol>

        <symbol id="reduce">
            <g fill="none" stroke="currentColor" stroke-width="2.5">
                <rect width="40.381" height="15.531" x="4" y="4"/>
                <path d="M36,26 L36,31.3857422" stroke-linecap="round" stroke-dasharray="1 7"/>
                <path d="M12,19 L12,31.4248387"/>
                <rect width="15.531" height="12.425" x="29" y="32" stroke-linecap="round" stroke-dasharray="1 6"/>
                <rect width="15.531" height="12.425" x="4" y="32"/>
            </g>
        </symbol>

        <symbol id="report">
            <path d="M24.1320008 44.328125C32.1512251 44.328125 38.6520901 35.226914 38.6520901 24 38.6520901 12.773086 32.1512251 3.671875 24.1320008 3.671875 16.1127765 3.671875 9.61191153 12.773086 9.61191153 24 9.61191153 35.226914 16.1127765 44.328125 24.1320008 44.328125zM13.7861328 10.5L34.4768075 10.5 13.7861328 10.5zM24.25 11L24.25 44.328125M34.1640625 37.0680804L39.9720982 42.8761161M38.5200893 22.25L44.328125 22.25M9.47991071 22.25L3.671875 22.25M13.8359375 10.9319196L8.02790179 5.12388393M34.1640625 10.9319196L39.9720982 5.12388393M13.9720982 37.0680804L8.1640625 42.8761161" fill="none" stroke="currentColor" stroke-width="2.5"/>
        </symbol>

        <symbol id="review">
            <path stroke="currentColor" stroke-width="2.5" fill="none" d="M23.9412979,30.8091391 C20.5466664,30.8091391 17.7939376,27.9866905 17.7939376,24.4982979 C17.7939376,21.0147136 20.5466664,18.1898609 23.9412979,18.1898609 C27.3383336,18.1898609 30.0910624,21.0147136 30.0910624,24.4982979 C30.0910624,27.9866905 27.3383336,30.8091391 23.9412979,30.8091391 M23.9412979,13.6821172 C14.2790996,13.6821172 7.25423179,24.4982979 7.25423179,24.4982979 C7.25423179,24.4982979 14.2790996,35.3168828 23.9412979,35.3168828 C33.6059004,35.3168828 40.6307682,24.4982979 40.6307682,24.4982979 C40.6307682,24.4982979 33.6059004,13.6821172 23.9412979,13.6821172"/>
            <path fill="currentColor" d="M23.9412979,21.7960558 C22.4867993,21.7960558 21.3063715,23.0053332 21.3063715,24.5007021 C21.3063715,25.9936668 22.4867993,27.2053483 23.9412979,27.2053483 C25.3982007,27.2053483 26.5762244,25.9936668 26.5762244,24.5007021 C26.5762244,23.0053332 25.3982007,21.7960558 23.9412979,21.7960558"/>
        </symbol>

        <symbol id="steps">
            <g fill="none" stroke="currentColor" stroke-width="2.5">
                <path fill="currentColor" d="M20 13L32 13M20 27L32 27M20 41L32 41M20 8L42 8M20 22L42 22M20 36L42 36"/>
                <circle cx="10.5" cy="9.5" r="4.5"/>
                <circle cx="10.5" cy="24.5" r="4.5"/>
                <circle cx="10.5" cy="38.5" r="4.5"/>
            </g>
        </symbol>

        <symbol id="testcase">
            <g fill="none" stroke="currentColor" stroke-width="2.5">
                <path fill="currentColor" d="M20 13L32 13M20 27L32 27M20 41L32 41M20 8L42 8M20 22L42 22M20 36L42 36"/>
                <polyline points="6 11 9 14 15 7" stroke-linecap="square"/>
                <polyline points="6 25 9 28 15 21" stroke-linecap="square"/>
                <polyline points="6 39 9 42 15 35" stroke-linecap="square"/>
            </g>
        </symbol>
    </defs>
</svg>
<?php if ( (int)$build < (int)$latest ):
    $prompt_text = "Please be sure you are running the latest WebKit Nightly build before continuing to test.";
    if (!empty($build))
        $prompt_meta = get_post_meta(get_the_ID(), 'prompt', true);
    if (!empty($prompt_meta)) $prompt_text = $prompt_meta;
?>
<div class="update-nag">
    <p><a href="<?php echo esc_url($latest_nightly_build[2]); ?>"><?php echo esc_html($prompt_text); ?></a></p>
</div>
<?php endif; ?>

    <?php if (have_posts()) : while (have_posts()) : the_post(); ?>

        <article class="page" id="nightly">

            <div class="bodycopy">
                <?php the_content(''); ?>
            </div>

        </article>

    <?php endwhile; endif; ?>

<?php get_footer(); ?>