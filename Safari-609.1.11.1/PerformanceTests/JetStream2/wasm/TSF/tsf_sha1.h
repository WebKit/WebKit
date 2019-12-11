/*
 *  tsf_sha1.h
 *
 *  Copyright (C) 1998, 2009
 *  Paul E. Jones <paulej@packetizer.com>
 *  All Rights Reserved
 *
 *****************************************************************************
 *  $Id: sha1.h 12 2009-06-22 19:34:25Z paulej $
 *****************************************************************************
 *
 *  Description:
 *      This class implements the Secure Hashing Standard as defined
 *      in FIPS PUB 180-1 published April 17, 1995.
 *
 *      Many of the variable names in the SHA1Context, especially the
 *      single character names, were used because those were the names
 *      used in the publication.
 *
 *      Please read the file sha1.c for more information.
 *
 */

#ifndef TSF_SHA1_H_
#define TSF_SHA1_H_

#include "tsf_types.h"

/* 
 *  This structure will hold context information for the hashing
 *  operation
 */
typedef struct tsf_SHA1Context
{
    uint32_t Message_Digest[5]; /* Message Digest (output)          */

    uint32_t Length_Low;        /* Message length in bits           */
    uint32_t Length_High;       /* Message length in bits           */

    uint8_t Message_Block[64]; /* 512-bit message blocks      */
    int32_t Message_Block_Index;    /* Index into message block array   */

    int32_t Computed;               /* Is the digest computed?          */
    int32_t Corrupted;              /* Is the message digest corruped?  */
} tsf_SHA1Context;

/*
 *  Function Prototypes
 */
void tsf_SHA1Reset(tsf_SHA1Context *);
int32_t tsf_SHA1Result(tsf_SHA1Context *);
void tsf_SHA1Input( tsf_SHA1Context *,
                    const uint8_t *,
                    uint32_t);

#endif
