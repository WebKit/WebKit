    <style>
    body.home {
        background-color: white;
    }

    .home .site-logo {
        opacity: 0;
        transition: opacity 0.5s ease-out;
    }

    .home .page-layer {
        background-color: #f7f7f7;
        border-top: 1px solid #e7e7e7;
        position: relative;
        z-index: 1;
    }

    .admin-bar header {
        top: 32px;
    }

    .home header {
        -webkit-backdrop-filter: none;
        border: none;
        background: none;
        position: fixed;

    }

    .hero a {
        text-decoration: none;
        z-index: 5;
    }

    .hero {
        height: 800px;
        margin-bottom: 3rem;
        overflow: hidden;
        background: none;
        box-sizing: border-box;

        position: fixed;
        top: 0;
        left: 0;
        width: 100vw;

        background-image: radial-gradient(65vw at 50% 40%, white 50%, rgba(255, 255, 255, 0.00) 100%), linear-gradient(180deg, rgb(255, 255, 255) 10%, rgb(232, 232, 232) 32%);
        background-position: 0% 0%;
    }

    .admin-bar .hero.spacing {
        margin-top: -32px;
    }

    .hero.spacing {
        position: relative;
        height: auto;
        visibility: hidden;
    }

    .hero .logo {
        text-align: left;
        margin: 3rem auto 0;
        padding-left: 12rem;
        padding-bottom: 3px;
        box-sizing: border-box;

        line-height: 10rem;
        font-size: 6rem;
        font-weight: 200;
        color: #444;

        background-repeat: no-repeat;
        background-size: 10rem;
        background-position: left 1rem;
    }

    .admin-bar .hero .logo {
        margin-top: 5rem;
    }

    .hero .tagline {
        text-align: left;
        display: block;
        font-size: 3rem;
        font-weight: 200;
        line-height: 1.125;
        margin-top: -2.5rem;
        margin-bottom: 2rem;
        color: #555;
    }

    .intro .column {
        position: relative;
        font-size: 2.4rem;
        line-height: 1.35417;
        font-weight: 200;
        box-sizing: border-box;
        width: 42.24561404%;
        display: inline-block;
        vertical-align: text-top;
    }

    .intro .column h2 {
        font-size: 3.2rem;
        line-height: 1.125;
        font-weight: 200;
        letter-spacing: 0em;
        display: block;
        margin-top: 3rem;
        margin-bottom: 1.8rem;
    }

    .intro .column:first-child {
        margin-right: 14.63157894%;
    }


    .home .floating {
        border-bottom: 1px solid #e7e7e7;
        background: rgba(255,255,255,0.9);
    }

    @supports ( -webkit-backdrop-filter: blur(10px) ) {
        .home .floating {
            backdrop-filter: blur(10px);
            -webkit-backdrop-filter: blur(10px);
            background: rgba(255,255,255,0.8);
        }
    }

    .home .site-logo.fade {
        opacity: 1;
    }

    .particles {
        position: relative;
        top: 0;
        left: 0;
        width: 100%;
        height: 100%;
        -webkit-transform-origin: center center;
          -ms-transform-origin: center center;
              transform-origin: center center;
    }

    .particle {
        position: absolute;
        background: white;
        opacity: .7;
        border-radius: 50%;
        z-index: -2;
    }
    
    @media only screen and (max-width: 782px) {
        
        .admin-bar header {
            top: 46px;
        }
    
        .admin-bar .hero.spacing {
            margin-top: -46px;
        }
    
        .hero {
            position: absolute;
        }
        
        .intro .column {
            width: 100%;
        }
        
    }
    
    @media only screen and (max-width: 600px) {
    
        .hero .logo,
        .hero .tagline,
        .hero .particle {
            display: none;
        }
    
        .hero {
           padding-top: 8rem;
        }
        
        .admin-bar .hero {
            padding-top: 13rem;
        }
        
/*        .admin-bar .hero.spacing {
            margin-top: calc(13rem - 46px);
        }*/
    
        .home header {
            padding-top: 1rem;
            position: absolute;
        }

        .home .site-logo {
            opacity: 1;
            margin-top: 0;
        }

        .home header {
            border-bottom: 1px solid #e7e7e7;
            background: rgba(255,255,255,0.9);
        }

        @supports ( -webkit-backdrop-filter: blur(10px) ) {
            .home header {
                backdrop-filter: blur(10px);
                -webkit-backdrop-filter: blur(10px);
                background: rgba(255,255,255,0.8);
            }
        }
    
        .home .pagination .next-button {
            width: 100%;
        }
    
    }
    
    @media only screen and (max-height: 415px) {
        .hero .logo,
        .hero .tagline,
        .hero .particle {
            display: none;
        }
    
        .home .site-logo {
            opacity: 1;
            margin-top: 0;
        }
        .home .hero {
            margin: 7rem 0 0;
        }
    
        .home.admin-bar .hero {
            margin-top: 12rem;
        }
    
        header,
        .home header {
            padding-top: 1rem;
            position: absolute;
        }
        .home header {
            border-bottom: 1px solid #e7e7e7;
            background: rgba(255,255,255,0.9);
        }

        @supports ( -webkit-backdrop-filter: blur(10px) ) {
            .home header {
                backdrop-filter: blur(10px);
                -webkit-backdrop-filter: blur(10px);
                background: rgba(255,255,255,0.8);
            }
        }
    }
        
    <?php 
        
        function rand_outside_circle() {
            srand();
            $circle_x = 50;
            $circle_y = 40;
            $circle_r = 32;
            $x = rand(0, 100); 
            $y = rand(0, 100);
            
            // Avoid starting or ending inside the white fill gradient
            if ( pow(($x - $circle_x), 2) + pow(($y - $circle_y), 2) <= pow($circle_r, 2) )
                return rand_outside_circle();
            
            return array($x . "vw", $y . "vh");
        }
        
        for ($i = 1; $i < 101; $i++): 
            $size = rand(5, 80) . "px";
            list($Xorigin, $Yorigin) = rand_outside_circle();
            list($Xend, $Yend) = rand_outside_circle();
            $from_opacity = rand(3, 8) / 10;
            $to_opacity = rand(3, 8) / 10;
        ?>
        .particle:nth-child(<?php echo $i; ?>){
            height: <?php echo $size; ?>; width: <?php echo $size; ?>;
            transform: translate(<?php echo $Xorigin; ?>,  <?php echo $Yorigin; ?>);
            opacity: <?php echo $from_opacity; ?>;
            animation: move-<?php echo $i; ?> 60s ease-out;
            animation-direction: alternate;
            animation-fill-mode: forwards;
            -webkit-animation-fill-mode: forwards;
        }

        @keyframes move-<?php echo $i; ?> {
            100% {
                transform: translate(<?php echo $Xend; ?>, <?php echo $Yend; ?>);
                opacity: <?php echo $to_opacity; ?>;
            }
        }
    <?php endfor; ?>
    </style>
    <div id="hero" class="hero">
        <?php for ($i = 0; $i < 101; $i++)
            echo "<div class=\"particle\"></div>"; ?>
        <div class="intro page-width">
        <a href="/"><h1 class="logo">WebKit <span class="tagline">Open Source Web Browser Engine</span></h1></a>
       
        <?php if ( is_front_page() && have_posts()): the_post(); ?><div class="intro-copy"><?php 
            $content = get_the_content();
            $columns = explode('<br />', $content);
            foreach ( $columns as $column )
                echo "<div class=\"column\">$column</div>";
        ?></div><?php endif; ?>
        </div>
    </div>
    <div class="hero spacing">
        <div class="intro page-width">
        <a href="/"><h1 class="logo">WebKit <span class="tagline">Open Source Web Browser Engine</span></h1></a>
       
        <?php if ( ! empty($columns) ): ?><div class="intro-copy"><?php 
            foreach ( $columns as $column )
                echo "<div class=\"column\">$column</div>";
        ?></div><?php endif; ?>
        </div>
    </div>
    <script>
    var latestScrollY = NaN, scrollV = 0, updating = false, layer = false,
        header = document.getElementById("header"),
        hero = document.getElementById("hero"),
        scrollBoundary = 353;
        
    header.animating = false;
    logo.animating = false;
    
    function onScroll() {
        if (isNaN(latestScrollY)) 
            latestScrollY = window.scrollY;
        
    	if (!updating)
    		requestAnimationFrame(update);
    	updating = true;
    }

    function update() {
        updating = false;
        
        if (document.body.offsetWidth <= 508) return;
        
        if (latestScrollY != window.scrollY) {
            scrollV = window.scrollY - latestScrollY;
            latestScrollY = window.scrollY;
            
            if (!layer) {
                layer = document.getElementsByClassName("page-layer").item(0);
            }
            
            scrollBoundary = layer.offsetTop - header.offsetHeight;
        }
                
        if (latestScrollY >= scrollBoundary) {
            addAnimationClass(header, "floating");
            if (!logo.classList.contains('fade'))
                setAnimateSpeed(logo, scrollV);
            addAnimationClass(logo, "fade");
        } else if (latestScrollY < scrollBoundary) {
            removeAnimationClass(header, "floating");
            if (logo.classList.contains('fade'))
                setAnimateSpeed(logo, 10);
            removeAnimationClass(logo, "fade");
        }
        
    }

    function setAnimateSpeed(element, velocity) {
        if (element.animating) return;
        var duration = Math.abs(3 / (velocity * 1.618));
        element.style["-webkit-transition-duration"] = duration + "s";
    }
    
    function addAnimationClass(element, classname) {
        if (element.classList.contains(classname)) return;
        if (element.animating) return;
        
        element.classList.add(classname.trim());
        animatingElement(element);
    }
    
    function animatingElement(element) {
        element.animating = true;
        element.addEventListener("transitionend", function () {
            element.animating = false;
        });            
    }
    
    function removeAnimationClass(element, classname) {
        if (!element.classList.contains(classname)) return;
        if (element.animating) return;

        element.classList.remove(classname.trim());
        animatingElement(element);
    }

    window.addEventListener("scroll", onScroll, false);
    window.addEventListener("resize", onScroll, false);
    </script>
        
<div class="page-layer">