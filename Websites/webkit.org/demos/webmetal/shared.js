const hasWebMetal = () => {
    return 'WebMetalRenderingContext' in window;
};

const checkForWebMetal = () => {
    if (hasWebMetal())
      return true;

    document.body.className = "error";
    return false;
}
