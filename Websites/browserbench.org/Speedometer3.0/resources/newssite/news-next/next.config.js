/** @type {import('next').NextConfig} */
const nextConfig = {
    reactStrictMode: true,
    output: "export",
    distDir: "dist",
    assetPrefix: "./",
    images: {
        unoptimized: true,
    },
};

module.exports = nextConfig;
