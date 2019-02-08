    <style>
    header {
        position: absolute;
    }

    .page-layer {
        overflow: hidden;
    }

    .hero {
        position: relative;
        overflow: hidden;
        max-width: 100vw;
        box-sizing: border-box;
        padding: 0 0 6rem;
        color: white;
        background-color: hsl(203.6, 100%, 12%);
        z-index: 1;
        top: -30rem;
        padding-top: 45rem;
        margin-bottom: -30rem;
    }

    .hero h1 {
        font-size: 4.8rem;
        font-weight: 500;
        letter-spacing: 0.006rem;
        text-align: center;
        line-height: 1.04167;
    }

    .hero p {
        font-size: 2.2rem;
        line-height: 1.45455;
        letter-spacing: 0.016em;
        font-weight: 300;
        text-align: center;
        margin-top: 1.65rem;
    }

    .hero a {
        color: hsl(200, 100%, 50%);
    }

    #fade {
        position: absolute;
        width: 100%;
        height: 1261px;
        background-image: radial-gradient(ellipse closest-corner at center center, hsl(198, 100%, 20%) 0%, hsla(204, 100%, 20%, 0) 100%);
        z-index: 4;
        transform: translateY(-450px);
    }

    #template {
        opacity: 0.1;
        position: absolute;
        top: 0px;
        width: 100%;
        height: 195.5rem;
        background-repeat: no-repeat;
        background-position: 50% 49.5%;
        background-size: 101.5%;
        z-index: 3;
        transform: translateY(-400px);
    }

    #compass {
        position: absolute;
        top: 0px;
        width: 100%;
        height: 1950px;
        background-repeat: no-repeat;
        background-position: 50% 50%;
        background-size: 98%;
        opacity: 0.3;
        animation-name: bgspin;
        animation-duration: 360s;
        animation-timing-function: ease-out;
        z-index: 2;
        will-change: transform;
        transform: translateY(-400px);
    }

    @keyframes bgspin {
        from {
            transform: translateY(-400px) rotate(0);
            -webkit-transform: translateY(-400px) rotate(0deg);
        }
        to {
            transform: translateY(-400px) rotate(360);
            -webkit-transform: translateY(-400px) rotate(360deg);
        }
    }

    .hero .content {
        position: relative;
        max-width: 800px;
        padding: 0 3rem;
        margin: 0 auto;
        z-index: 10;
    }

    main {
        position: relative;
        z-index: 10;
    }

    @media only screen and (max-width: 920px) {
        .hero {
            padding-top: 40rem;
            padding-bottom: 3rem;
        }
    }

    @media only screen and (max-width: 690px) {
        .hero h1 {
            font-size: 4rem;
            line-height: 1.1;
        }

        .hero p {
            font-size: 2rem;
            line-height: 1.4;
            letter-spacing: -0.016rem;
        }

        #compass {
            background-size: 150%;
        }
    }

    @media only screen and (max-width: 320px) {
        .hero h1 {
            font-size: 2.5rem;
            letter-spacing: -0.016rem;
        }
    }

    @media (prefers-reduced-motion) {
        #compass {
            animation: none;
        }
    }
    </style>

    <?php if ( is_front_page() && have_posts()): the_post(); ?>
    <div class="hero">
        <div id="template"></div>
        <div id="compass"></div>
        <div id="fade"></div>
        <div class="content">
            <?php the_content(); ?>
        </div>
    </div>
    <?php endif; ?>

<div class="page-layer">
