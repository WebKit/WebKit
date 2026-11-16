const { merge } = require("webpack-merge");
const common = require("./webpack.common.js");

const MiniCssExtractPlugin = require("mini-css-extract-plugin");
const CopyPlugin = require("copy-webpack-plugin");
const webpack = require("webpack");

module.exports = merge(common, {
    mode: "production",
    plugins: [
        new MiniCssExtractPlugin({
            filename: "big-dom.css",
            chunkFilename: "[id].css",
        }),
        new CopyPlugin({
            patterns: [
                {
                    from: "./src/assets/logo.png",
                    to: "logo.png",
                },
            ],
        }),
        new webpack.IgnorePlugin({ resourceRegExp: /canvas/ }),
    ],
    module: {
        rules: [
            {
                test: /\.css$/i,
                use: [
                    MiniCssExtractPlugin.loader,
                    "css-loader",
                    {
                        loader: "postcss-loader",
                        options: {
                            postcssOptions: {
                                plugins: [require("postcss-import"), require("postcss-varfallback"), require("postcss-dropunusedvars"), require("cssnano")],
                            },
                        },
                    },
                ],
            },
        ],
    },
    target: "node",
});
