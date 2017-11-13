/*
** FAAD2 - Freeware Advanced Audio (AAC) Decoder including SBR decoding
** Copyright (C) 2003-2005 M. Bakker, Nero AG, http://www.nero.com
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
**
** Any non-GPL usage of this software or parts of this software is strictly
** forbidden.
**
** The "appropriate copyright message" mentioned in section 2c of the GPLv2
** must read: "Code from FAAD2 is copyright (c) Nero AG, www.nero.com"
**
** Commercial non-GPL licensing of this software is possible.
** For more info contact Nero AG through Mpeg4AAClicense@nero.com.
**
** $Id: main.c,v 1.85 2008/09/22 17:55:09 menno Exp $
**/

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#define off_t __int64
#else
#include <time.h>
#endif

#include <fcntl.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

#include <neaacdec.h>
#include "audio.h"

#ifndef min
#define min(a,b) ( (a) < (b) ? (a) : (b) )
#endif

#define MAX_CHANNELS 6 /* make this higher to support files with
                          more channels */


/* FAAD file buffering routines */
typedef struct {
    long bytes_into_buffer;
    long bytes_consumed;
    long file_offset;
    unsigned char* buffer;
    int at_eof;
    FILE* infile;
    long buffer_size;
} aac_buffer;

typedef struct aacCoderInfo {
    NeAACDecHandle hDecoder;
    aac_buffer b;
    int first_pkt;
} aacCoderInfoT;

enum aacErr {
    aacErrOK = 0,
    aacParaErr,
    aacAllocErr,
    aacInitErr,
    aacDecodeErr,
    aacNoBuffer,
    aacErrMax
};


static int quiet = 0;

static void faad_fprintf(FILE* stream, const char* fmt, ...)
{
    va_list ap;

    if (!quiet) {
        va_start(ap, fmt);

        vfprintf(stream, fmt, ap);

        va_end(ap);
    }
}

static int fill_buffer(aac_buffer* b, const char* aac_data, const int aac_len)
{
    int buffer_rest = 0, request = 0;

    if (b == NULL || b->buffer == NULL) {
        faad_fprintf(stderr, "aac_buffer error: b=%p, b->buffer=%p\n", b, b->buffer);
        return 0;
    }

    buffer_rest = b->buffer_size - b->bytes_into_buffer;

    if (b->bytes_into_buffer) {
        memmove((void*)b->buffer, (void*)(b->buffer + b->bytes_consumed),
                b->bytes_into_buffer * sizeof(unsigned char));
    }

    request = buffer_rest > aac_len ? aac_len : buffer_rest;
    memcpy(b->buffer + b->bytes_into_buffer, aac_data, request);
    b->bytes_into_buffer += request;
    b->bytes_consumed = 0;

    if (b->bytes_into_buffer > 3) {
        if (memcmp(b->buffer, "TAG", 3) == 0) {
            b->bytes_into_buffer = 0;
        }
    }

    if (b->bytes_into_buffer > 11) {
        if (memcmp(b->buffer, "LYRICSBEGIN", 11) == 0) {
            b->bytes_into_buffer = 0;
        }
    }

    if (b->bytes_into_buffer > 8) {
        if (memcmp(b->buffer, "APETAGEX", 8) == 0) {
            b->bytes_into_buffer = 0;
        }
    }

    return request;
}

static void advance_buffer(aac_buffer* b, int bytes)
{
    b->file_offset += bytes;
    b->bytes_consumed = bytes;
    b->bytes_into_buffer -= bytes;

    if (b->bytes_into_buffer < 0) {
        b->bytes_into_buffer = 0;
    }
}

/* MicroSoft channel definitions */
#define SPEAKER_FRONT_LEFT             0x1
#define SPEAKER_FRONT_RIGHT            0x2
#define SPEAKER_FRONT_CENTER           0x4
#define SPEAKER_LOW_FREQUENCY          0x8
#define SPEAKER_BACK_LEFT              0x10
#define SPEAKER_BACK_RIGHT             0x20
#define SPEAKER_FRONT_LEFT_OF_CENTER   0x40
#define SPEAKER_FRONT_RIGHT_OF_CENTER  0x80
#define SPEAKER_BACK_CENTER            0x100
#define SPEAKER_SIDE_LEFT              0x200
#define SPEAKER_SIDE_RIGHT             0x400
#define SPEAKER_TOP_CENTER             0x800
#define SPEAKER_TOP_FRONT_LEFT         0x1000
#define SPEAKER_TOP_FRONT_CENTER       0x2000
#define SPEAKER_TOP_FRONT_RIGHT        0x4000
#define SPEAKER_TOP_BACK_LEFT          0x8000
#define SPEAKER_TOP_BACK_CENTER        0x10000
#define SPEAKER_TOP_BACK_RIGHT         0x20000
#define SPEAKER_RESERVED               0x80000000

static long aacChannelConfig2wavexChannelMask(NeAACDecFrameInfo* hInfo)
{
    if (hInfo->channels == 6 && hInfo->num_lfe_channels) {
        return SPEAKER_FRONT_LEFT + SPEAKER_FRONT_RIGHT +
               SPEAKER_FRONT_CENTER + SPEAKER_LOW_FREQUENCY +
               SPEAKER_BACK_LEFT + SPEAKER_BACK_RIGHT;
    } else {
        return 0;
    }
}

static char* position2string(int position)
{
    switch (position) {
    case FRONT_CHANNEL_CENTER:
        return "Center front";

    case FRONT_CHANNEL_LEFT:
        return "Left front";

    case FRONT_CHANNEL_RIGHT:
        return "Right front";

    case SIDE_CHANNEL_LEFT:
        return "Left side";

    case SIDE_CHANNEL_RIGHT:
        return "Right side";

    case BACK_CHANNEL_LEFT:
        return "Left back";

    case BACK_CHANNEL_RIGHT:
        return "Right back";

    case BACK_CHANNEL_CENTER:
        return "Center back";

    case LFE_CHANNEL:
        return "LFE";

    case UNKNOWN_CHANNEL:
        return "Unknown";

    default:
        return "";
    }

    return "";
}

static void print_channel_info(NeAACDecFrameInfo* frameInfo)
{
    /* print some channel info */
    int i;
    long channelMask = aacChannelConfig2wavexChannelMask(frameInfo);

    faad_fprintf(stderr, "  ---------------------\n");

    if (frameInfo->num_lfe_channels > 0) {
        faad_fprintf(stderr, " | Config: %2d.%d Ch     |", frameInfo->channels - frameInfo->num_lfe_channels, frameInfo->num_lfe_channels);
    } else {
        faad_fprintf(stderr, " | Config: %2d Ch       |", frameInfo->channels);
    }

    if (channelMask) {
        faad_fprintf(stderr, " WARNING: channels are reordered according to\n");
    } else {
        faad_fprintf(stderr, "\n");
    }

    faad_fprintf(stderr, "  ---------------------");

    if (channelMask) {
        faad_fprintf(stderr, "  MS defaults defined in WAVE_FORMAT_EXTENSIBLE\n");
    } else {
        faad_fprintf(stderr, "\n");
    }

    faad_fprintf(stderr, " | Ch |    Position    |\n");
    faad_fprintf(stderr, "  ---------------------\n");

    for (i = 0; i < frameInfo->channels; i++) {
        faad_fprintf(stderr, " | %.2d | %-14s |\n", i, position2string((int)frameInfo->channel_position[i]));
    }

    faad_fprintf(stderr, "  ---------------------\n");
    faad_fprintf(stderr, "\n");
}
#define BYTES_PER_SAMPLE 2

int aacCoderInit(void** decoder)
{
    aacCoderInfoT* aac = NULL;
    NeAACDecHandle hDecoder;
    unsigned long samplerate;
    unsigned char channels;

    faad_fprintf(stderr, "[%s] enter\n", __func__);

    hDecoder = NeAACDecOpen();

    if (NeAACDecInit(hDecoder, NULL, 0, &samplerate, &channels) < 0) {
        /* If some error initializing occured, skip the file */
        faad_fprintf(stderr, "Error initializing decoder library.\n");
        NeAACDecClose(hDecoder);
        return -aacInitErr;
    }

    if (!hDecoder) {
        faad_fprintf(stderr, "Error NeAACDecOpen\n");
        return -aacInitErr;
    }

    aac = (aacCoderInfoT*)malloc(sizeof(aacCoderInfoT));

    if (!aac) {
        faad_fprintf(stderr, "Error malloc\n");
        NeAACDecClose(hDecoder);
        return -aacAllocErr;
    }

    memset(aac, 0, sizeof(aacCoderInfoT));

    if (!(aac->b.buffer = (unsigned char*)malloc(FAAD_MIN_STREAMSIZE * MAX_CHANNELS))) {
        faad_fprintf(stderr, "Memory allocation b.buffer error\n");
        NeAACDecClose(hDecoder);
        free(aac);
        return -aacAllocErr;
    }

    aac->b.buffer_size = FAAD_MIN_STREAMSIZE * MAX_CHANNELS;
    memset(aac->b.buffer, 0, FAAD_MIN_STREAMSIZE * MAX_CHANNELS);

    aac->hDecoder = hDecoder;
    aac->first_pkt = 1;
    *decoder = aac;
    faad_fprintf(stderr, "[%s] exit\n", __func__);
    return aacErrOK;
}

int aacCoderConfig(void* decoder, int srate, int outputFormat, int object_type, int downMatrix, int old_format)
{
    NeAACDecConfigurationPtr config;
    NeAACDecHandle hDecoder = NULL;
    aacCoderInfoT* aac = (aacCoderInfoT*)decoder;

    faad_fprintf(stderr, "[%s] enter\n", __func__);

    if (!aac) {
        faad_fprintf(stderr, "input parameter decoder is NULL\n");
        return -aacParaErr;
    }

    hDecoder = aac->hDecoder;

    if (!hDecoder) {
        faad_fprintf(stderr, "aac->hDecoder is NULL, maybe not init\n");
        return -aacParaErr;
    }

    /* Set the default object type and samplerate */
    /* This is useful for RAW AAC files */
    config = NeAACDecGetCurrentConfiguration(hDecoder);

    if (srate) {
        config->defSampleRate = srate;
    }

    config->defObjectType = object_type;
    config->outputFormat = outputFormat;
    config->downMatrix = downMatrix;
    config->useOldADTSFormat = old_format;
    //config->dontUpSampleImplicitSBR = 1;
    NeAACDecSetConfiguration(hDecoder, config);
    faad_fprintf(stderr, "[%s] exit\n", __func__);
    return aacErrOK;
}

int aacCoderConfig2(void* hDecoder, int srate)
{
    return aacCoderConfig(hDecoder, srate, FAAD_FMT_16BIT, LC, 0, 0);
}

int aacCoderUnInit(void* decoder)
{
    aacCoderInfoT* aacInfo = (aacCoderInfoT*)decoder;

    faad_fprintf(stderr, "[%s] enter\n", __func__);

    if (aacInfo == NULL) {
        faad_fprintf(stderr, "aaacInfo is NULL\n");
        return -aacParaErr;
    }

    if (aacInfo->hDecoder) {
        NeAACDecClose(aacInfo->hDecoder);
    }

    if (aacInfo->b.buffer) {
        free(aacInfo->b.buffer);
    }

    free(decoder);

    faad_fprintf(stderr, "[%s] exit\n", __func__);

    return aacErrOK;
}

int aacCoderDecoding(void* decoder, const char* aacData, unsigned short aacLen, char* pcmData, unsigned int pcmLen)
{
    aacCoderInfoT* aacInfo = (aacCoderInfoT*)decoder;
    NeAACDecHandle hDecoder = NULL;
    NeAACDecFrameInfo frameInfo;
    aac_buffer* b = NULL;
    int ret = 0;
    unsigned int de_pcm_len = 0;
    void* sample_buffer = NULL;

//    faad_fprintf(stderr, "[%s] enter\n", __func__);

    if (aacInfo == NULL || aacData == NULL || pcmData == NULL || aacLen == 0 || pcmLen == 0) {
        faad_fprintf(stderr, "paras is error: aacInfo=%p, aacData=%p, pcmData=%p, aacLen=%d, pcmLen=%d\n",
                     aacInfo, aacData, pcmData, aacLen, pcmLen);
        return -aacParaErr;
    }

    b = &aacInfo->b;
    hDecoder = aacInfo->hDecoder;

    if (!hDecoder) {
        faad_fprintf(stderr, "aac->hDecoder is NULL, maybe not init\n");
        return -aacParaErr;
    }

    do {
        ret = fill_buffer(b, aacData, aacLen);

        if (ret > 0) {
            aacLen -= ret;
            aacData += ret;
        }

        sample_buffer = NeAACDecDecode(hDecoder, &frameInfo,
                                       b->buffer, b->bytes_into_buffer);
        /* update buffer indices */
        advance_buffer(b, frameInfo.bytesconsumed);

        if (frameInfo.error > 0) {
            faad_fprintf(stderr, "Error: %s\n",
                         NeAACDecGetErrorMessage(frameInfo.error));
            return -1;
        } else if ((frameInfo.error == 0) && (frameInfo.samples > 0)) {
            if (aacInfo->first_pkt) {
                print_channel_info(&frameInfo);
                aacInfo->first_pkt = 0;
            }

            ret = frameInfo.samples * BYTES_PER_SAMPLE;

            if (ret > (pcmLen - de_pcm_len)) {
                ret = pcmLen - de_pcm_len;
            }

            if (de_pcm_len < pcmLen) {
                memcpy(&pcmData[de_pcm_len], sample_buffer, ret);
                de_pcm_len += ret;
            } else if (de_pcm_len >= pcmLen) {
                faad_fprintf(stderr, "Warning: no buffer space, loss %d bytes\n", frameInfo.samples * BYTES_PER_SAMPLE);
            }
        }

    } while (aacLen);

 //   faad_fprintf(stderr, "[%s] exit\n", __func__);
    return de_pcm_len;
}
/*
 * Audio decoding.
 */
#define OUT_WAV_FILE 1
#define INBUF_SIZE  (1024)
int  main(int argc, const char* argv[])
{
    void* decoder = NULL;
    const char* outfilename, *filename;
    int ret, len;
    FILE* f;
#if OUT_WAV_FILE
    audio_file* aufile;
#else
    FILE* outfile;
#endif
    char inbuf[INBUF_SIZE];
    char outbuf[INBUF_SIZE * 10];
    unsigned int pcmLen = INBUF_SIZE * 10;

    if (argc < 3) {
        fprintf(stderr, "Usage: %s outPcmFilePath inAacFilePath\n", argv[0]);
        return -1;
    }

    outfilename = argv[1];
    filename = argv[2];

    printf("Decode audio file %s to %s\n", filename, outfilename);

    f = fopen(filename, "rb");

    if (!f) {
        fprintf(stderr, "Could not open %s\n", filename);
        exit(1);
    }

#if OUT_WAV_FILE
    aufile = open_audio_file((char*)outfilename, 44100, 2, FAAD_FMT_16BIT, OUTPUT_WAV, 0);

    if (!aufile) {
        exit(1);
    }

#else
    outfile = fopen(outfilename, "wb");

    if (!outfile) {
        exit(1);
    }

#endif

    //init aac decoder
    if ((ret = aacCoderInit(&decoder) != 0)) {
        fprintf(stderr, "init aac coder error:ret=%d\n", ret);
        exit(1);
    }

    aacCoderConfig2(decoder, 44100);
    /* decode until eof */
    len = fread(inbuf, 1, INBUF_SIZE, f);
    int i = 0;

    while (len > 0) {
        len = aacCoderDecoding(decoder, inbuf, len, outbuf, pcmLen);

        if (len > 0) {
#if OUT_WAV_FILE
            write_audio_file(aufile, outbuf, len / 2, 0);
#else
            fwrite(outbuf, 1, len, outfile);
#endif
        } else if (len < 0) {
            break;
        }

        len = fread(inbuf, 1, INBUF_SIZE, f);

        if (0) {
            if (i++ > 2000) {
                break;
            }
        }
    }

#if OUT_WAV_FILE
    close_audio_file(aufile);
#else
    fclose(outfile);
#endif
    fclose(f);

    //UnInit aac decoder
    aacCoderUnInit(decoder);

    return 0;
}

