import { useEffect, useState } from "react";
import { useLocation } from "react-router-dom";
import classNames from "classnames";

import NavList from "../navlist/navlist";
import LogoIcon from "@/assets/logo-icon";
import SocialIcons from "@/partials/icons/social-icons";

import navbarStyles from "news-site-css/dist/navbar.module.css";
import navStyles from "news-site-css/dist/nav.module.css";

export default function Navbar({ callback }) {
    const location = useLocation();
    const [isOpen, setIsOpen] = useState(false);

    function handleChange(e) {
        setIsOpen(e.target.checked);
    }

    function handleClick() {
        setIsOpen(false);
    }

    function calculateViewportHeight() {
        // Since the navbar is supposed to appear below the menu, we can't use position: fixed, height: 100%.
        // Therefore we are using 100vh for the height. This function fixes the challenge on mobile, where
        // 100vh might include the addressbar, ect.

        // First we get the viewport height and we multiple it by 1% to get a value for a vh unit
        let vh = window.innerHeight * 0.01;
        // Then we set the value in the --vh custom property to the root of the document
        document.documentElement.style.setProperty("--vh", `${vh}px`);
    }

    useEffect(() => {
        calculateViewportHeight();
        window.addEventListener("resize", calculateViewportHeight);
        return () => {
            window.removeEventListener("resize", calculateViewportHeight);
        };
    }, []);

    return (
        <div className={navbarStyles.navbar}>
            <input type="checkbox" id={navbarStyles["navbar-toggle"]} onChange={handleChange} checked={isOpen} />
            <label htmlFor={navbarStyles["navbar-toggle"]} className={navbarStyles["navbar-label"]}>
                <span className="visually-hidden">Navbar Toggle</span>
                <div className={classNames(navbarStyles["navbar-label-icon"], "animated-icon", "hamburger-icon")} title="Hamburger Icon">
                    <span className="animated-icon-inner">
                        <span></span>
                        <span></span>
                        <span></span>
                    </span>
                </div>
            </label>
            <button className={navStyles["page-navigation-logo"]} id="home-link" onClick={callback}>
                <LogoIcon />
            </button>
            <div className={navbarStyles["navbar-active-path"]}>{location.pathname.split("/")[1]}</div>
            <div className={navbarStyles["navbar-content"]}>
                <NavList id="navbar-navlist" callback={handleClick} />
                <div className={navbarStyles["navbar-icons"]}>
                    <SocialIcons id="navbar-social-icons" />
                </div>
            </div>
        </div>
    );
}
