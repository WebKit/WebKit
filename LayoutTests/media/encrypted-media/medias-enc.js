const streamMedias = {
    "simpleClearKeyMSE" : {    video : {    initDataType : "cenc",
                                            mimeType     : 'video/mp4; codecs="avc1.64001F"',
                                            segments     : [ "../../content/encrypted/segments/VideoClearKeyCenc-seg-0.mp4" ],
                                            keys         : {    "d0d1d2d3d4d5d6d7d8d9dadbdcdddedf" : "c0c1c2c3c4c5c6c7c8c9cacbcccdcecf" }
                                       }
                          },
    "simpleClearKey"   :  {    video : {    initDataType : "cenc",
                                            mimeType     : 'video/mp4; codecs="avc1.64001F"',
                                            path         : "../../content/encrypted/VideoClearKeyCenc.mp4",
                                            keys         : {    "d0d1d2d3d4d5d6d7d8d9dadbdcdddedf" : "c0c1c2c3c4c5c6c7c8c9cacbcccdcecf" }
                                       }
                          }
                   };
