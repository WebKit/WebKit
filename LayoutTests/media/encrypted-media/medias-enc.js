var streamMedias = {
    "simpleClearKeyMSE" : {    video : {    initDataType : "cenc",
                                            mimetype     : 'video/mp4; codecs="avc1.64001F"',
                                            segments     : [ "../../content/encrypted/segments/VideoClearKeyCenc-seg-0.mp4" ],
                                            keys         : {    KID : "a0a1a2a3a4a5a6a7a8a9aaabacadaeaf",
                                                                Key : "b0b1b2b3b4b5b6b7b8b9babbbcbdbebf" }
                                       }
                          },
    "simpleClearKey"   :  {    video : {    initDataType : "cenc",
                                            mimeType     : 'video/mp4; codecs="avc1.64001F"',
                                            path         : "../../content/encrypted/VideoClearKeyCenc.mp4",
                                            keys         : {    KID : "d0d1d2d3d4d5d6d7d8d9dadbdcdddedf",
                                                                Key : "c0c1c2c3c4c5c6c7c8c9cacbcccdcecf" }
                                       }
                          }
                   };
