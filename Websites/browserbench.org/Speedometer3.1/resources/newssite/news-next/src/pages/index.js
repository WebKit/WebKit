import React from "react";
import { HashRouter as Router, Routes, Route } from "react-router-dom";
import Page from "@/partials/page/page";
import Head from "next/head";
import { DataContextProvider } from "@/context/data-context";

export default function App() {
    return (
        <>
            <Head>
                <title>The Daily Broadcast</title>
                <meta name="description" content="A news site developed with Next.js." key="description" />
            </Head>
            <DataContextProvider>
                <Router>
                    <Routes>
                        <Route path="/business" element={<Page id="business" key="business" />} />
                        <Route path="/health" element={<Page id="health" key="health" />} />
                        <Route path="/opinion" element={<Page id="opinion" key="opinion" />} />
                        <Route path="/politics" element={<Page id="politics" key="politics" />} />
                        <Route path="/us" element={<Page id="us" key="us" />} />
                        <Route path="/world" element={<Page id="world" key="world" />} />
                        <Route path="/home" element={<Page id="home" key="home" />} />
                        <Route path="/" element={<Page id="home" key="home" />} />
                    </Routes>
                </Router>
            </DataContextProvider>
        </>
    );
}
