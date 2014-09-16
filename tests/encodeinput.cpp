/*
 *  encodeinput.cpp - encode test input
 *
 *  Copyright (C) 2011-2014 Intel Corporation
 *    Author: Cong Zhong<congx.zhong@intel.com>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public License
 *  as published by the Free Software Foundation; either version 2.1
 *  of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free
 *  Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301 USA
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include "encodeinput.h"

using namespace YamiMediaCodec;

EncodeStreamInput::EncodeStreamInput()
    : m_fp(NULL)
    , m_width(0)
    , m_height(0)
    , m_frameSize(0)
    , m_buffer(NULL)
    , m_readToEOS(false)
{
}

bool EncodeStreamInput::init(const char* inputFileName, const int width, const int height)
{
    m_width = width;
    m_height = height;
    m_frameSize = m_width * m_height * 3 / 2;

    m_fp = fopen(inputFileName, "r");
    if (!m_fp) {
        fprintf(stderr, "fail to open input file: %s", inputFileName);
        return false;
    }

    m_buffer = static_cast<uint8_t*>(malloc(m_frameSize));
    return true;
}

bool EncodeStreamInput::getOneFrameInput(VideoEncRawBuffer &inputBuffer)
{
    if (m_readToEOS)
        return false;

    int ret = fread(m_buffer, sizeof(uint8_t), m_frameSize, m_fp);

    if (ret <= 0) {
        m_readToEOS = true;
        return false;
    } else if (ret < m_frameSize) {
        fprintf (stderr, "data is not enough to read, maybe resolution is wrong\n");
        return false;
    } else {
        inputBuffer.data = m_buffer;
        inputBuffer.size = m_frameSize;
    }

    return true;
}

EncodeStreamInput::~EncodeStreamInput()
{
    if(m_fp)
        fclose(m_fp);

    if(m_buffer)
        free(m_buffer);
}

EncodeStreamOutput::EncodeStreamOutput():m_fp(NULL)
{
}

EncodeStreamOutput::~EncodeStreamOutput()
{
    if (m_fp)
        fclose(m_fp);
}

EncodeStreamOutput* EncodeStreamOutput::create(const char* outputFileName, int width , int height)
{
    EncodeStreamOutput * output = NULL;
    if(outputFileName==NULL)
        return NULL;
    const char *ext = strrchr(outputFileName,'.');
    if(ext==NULL)
        return NULL;
    ext++;//h264;264;jsv;avc;26l;jvt;ivf
    if(strcasecmp(ext,"h264")==0 ||
        strcasecmp(ext,"264")==0 ||
        strcasecmp(ext,"jsv")==0 ||
        strcasecmp(ext,"avc")==0 ||
        strcasecmp(ext,"26l")==0 ||
        strcasecmp(ext,"jvt")==0 ) {
            output = new EncodeStreamOutputH264();
    }
    else if((strcasecmp(ext,"ivf")==0) ||
            (strcasecmp(ext,"vp8")==0)) {
            output = new EncodeStreamOutputVP8();
    }
    else
        return NULL;

    if(!output->init(outputFileName, width, height)) {
        delete output;
        return NULL;
    }
    return output;
}

bool EncodeStreamOutput::init(const char* outputFileName, int width , int height)
{
    m_fp = fopen(outputFileName, "w+");
    if (!m_fp) {
        fprintf(stderr, "fail to open output file: %s\n", outputFileName);
        return false;
    }
    return true;
}

bool EncodeStreamOutput::write(void* data, int size)
{
    return fwrite(data, 1, size, m_fp) == size;
}

const char* EncodeStreamOutputH264::getMimeType()
{
    return "video/h264";
}

const char* EncodeStreamOutputVP8::getMimeType()
{
    return "video/x-vnd.on2.vp8";
}

void setUint32(uint8_t* header, uint32_t value)
{
    uint32_t* h = (uint32_t*)header;
    *h = value;
}

void setUint16(uint8_t* header, uint16_t value)
{
    uint16_t* h = (uint16_t*)header;
    *h = value;
}

static void get_ivf_file_header(uint8_t *header, int width, int height,int count)
{
    setUint32(header, 'FIKD');
    setUint16(header+4, 0);                     /* version */
    setUint16(header+6,  32);                   /* headersize */
    setUint32(header+8,  '08PV');               /* headersize */
    setUint16(header+12, width);                /* width */
    setUint16(header+14, height);               /* height */
    setUint32(header+16, 1);                    /* rate */
    setUint32(header+20, 1);                    /* scale */
    setUint32(header+24, count);                /* length */
    setUint32(header+28, 0);                    /* unused */
}

bool EncodeStreamOutputVP8::init(const char* outputFileName, int width , int height)
{
    if (!EncodeStreamOutput::init(outputFileName, width, height))
        return false;
    uint8_t header[32];
    get_ivf_file_header(header, width, height, m_frameCount);
    return EncodeStreamOutput::write(header, sizeof(header));
}

bool EncodeStreamOutputVP8::write(void* data, int size)
{
    uint8_t header[12];
    memset(header, 0, sizeof(header));
    setUint32(header, size);
    if (!EncodeStreamOutput::write(&header, sizeof(header)))
        return false;
    if (!EncodeStreamOutput::write(data, size))
        return false;
    m_frameCount++;
    return true;
}
EncodeStreamOutputVP8::EncodeStreamOutputVP8():m_frameCount(0){}

EncodeStreamOutputVP8::~EncodeStreamOutputVP8()
{
    if (m_fp && !fseek(m_fp, 24,SEEK_SET)) {
        EncodeStreamOutput::write(&m_frameCount, sizeof(m_frameCount));
    }
}

bool createOutputBuffer(VideoEncOutputBuffer* outputBuffer, int maxOutSize)
{
    outputBuffer->data = static_cast<uint8_t*>(malloc(maxOutSize));
    if (!outputBuffer->data)
        return false;
    outputBuffer->bufferSize = maxOutSize;
    outputBuffer->format = OUTPUT_EVERYTHING;
    return true;
}
