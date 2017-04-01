const hasWebGPU = () => {
    return 'WebGPURenderingContext' in window;
};

const checkForWebGPU = () => {
    if (hasWebGPU())
      return true;

    document.body.className = "error";
    return false;
}
