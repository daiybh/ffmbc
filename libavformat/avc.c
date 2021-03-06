/*
 * AVC helper functions for muxers
 * Copyright (c) 2006 Baptiste Coudurier <baptiste.coudurier@smartjog.com>
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation;
 * version 2 of the License.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "libavutil/intreadwrite.h"
#include "avformat.h"
#include "avio.h"
#include "avc.h"

static const uint8_t *ff_avc_find_startcode_internal(const uint8_t *p, const uint8_t *end)
{
    const uint8_t *a = p + 4 - ((intptr_t)p & 3);

    for (end -= 3; p < a && p < end; p++) {
        if (p[0] == 0 && p[1] == 0 && p[2] == 1)
            return p;
    }

    for (end -= 3; p < end; p += 4) {
        uint32_t x = *(const uint32_t*)p;
//      if ((x - 0x01000100) & (~x) & 0x80008000) // little endian
//      if ((x - 0x00010001) & (~x) & 0x00800080) // big endian
        if ((x - 0x01010101) & (~x) & 0x80808080) { // generic
            if (p[1] == 0) {
                if (p[0] == 0 && p[2] == 1)
                    return p;
                if (p[2] == 0 && p[3] == 1)
                    return p+1;
            }
            if (p[3] == 0) {
                if (p[2] == 0 && p[4] == 1)
                    return p+2;
                if (p[4] == 0 && p[5] == 1)
                    return p+3;
            }
        }
    }

    for (end += 3; p < end; p++) {
        if (p[0] == 0 && p[1] == 0 && p[2] == 1)
            return p;
    }

    return end + 3;
}

const uint8_t *ff_avc_find_startcode(const uint8_t *p, const uint8_t *end){
    const uint8_t *out= ff_avc_find_startcode_internal(p, end);
    if(p<out && out<end && !out[-1]) out--;
    return out;
}

int ff_avc_parse_nal_units(AVCodecContext *avctx, ByteIOContext *pb, const uint8_t *buf_in, int size)
{
    const uint8_t *p = buf_in;
    const uint8_t *end = p + size;
    const uint8_t *nal_start, *nal_next, *nal_end;
    int sps_size = 0, pps_size = 0;
    uint8_t *sps = NULL, *pps = NULL;

    size = 0;
    nal_start = ff_avc_find_startcode(p, end);
    while (nal_start < end) {
        while(!*(nal_start++));
        nal_next = ff_avc_find_startcode(nal_start, end);
        for (nal_end = nal_next; nal_end > nal_start; nal_end--)
            if (*nal_end)
                break;
        nal_end++;
        switch (*nal_start & 0x1f) {
        case 7: // sps
            if (!avctx->extradata && !sps) {
                sps = av_malloc(4 + nal_end - nal_start);
                if (!sps)
                    return AVERROR(ENOMEM);
                AV_WB32(sps, 1);
                memcpy(sps + 4, nal_start, nal_end - nal_start);
                sps_size = 4 + nal_end - nal_start;
                goto skip;
            }
            break;
        case 8: // pps
            if (!avctx->extradata && !pps) {
                pps = av_malloc(4 + nal_end - nal_start);
                if (!pps)
                    return AVERROR(ENOMEM);
                AV_WB32(pps, 1);
                memcpy(pps + 4, nal_start, nal_end - nal_start);
                pps_size = 4 + nal_end - nal_start;
                goto skip;
            }
            break;
        case 9:
            goto skip;
        }
        put_be32(pb, nal_end - nal_start);
        put_buffer(pb, nal_start, nal_end - nal_start);
        size += 4 + nal_end - nal_start;
    skip:
        nal_start = nal_next;
    }

    if (!avctx->extradata && sps && pps) {
        avctx->extradata = av_malloc(sps_size + pps_size);
        if (!avctx->extradata)
            return AVERROR(ENOMEM);
        memcpy(avctx->extradata, sps, sps_size);
        memcpy(avctx->extradata+sps_size, pps, pps_size);
        avctx->extradata_size = sps_size + pps_size;
    }

    return size;
}

int ff_avc_parse_nal_units_buf(AVCodecContext *avctx, const uint8_t *buf_in, uint8_t **buf, int *size)
{
    ByteIOContext *pb;
    int ret = url_open_dyn_buf(&pb);
    if(ret < 0)
        return ret;

    ff_avc_parse_nal_units(avctx, pb, buf_in, *size);

    av_freep(buf);
    *size = url_close_dyn_buf(pb, buf);
    return 0;
}

int ff_isom_write_avcc(AVCodecContext *avctx, ByteIOContext *pb)
{
    if (avctx->extradata_size > 6) {
        /* check for h264 start code */
        if (AV_RB32(avctx->extradata) == 0x00000001 ||
            AV_RB24(avctx->extradata) == 0x000001) {
            const uint8_t *p = avctx->extradata, *end = p + avctx->extradata_size, *nal_end;
            uint32_t sps_size=0, pps_size=0;
            const uint8_t *sps = 0, *pps = 0;
            int nal_type;

            /* look for sps and pps */
            p = ff_avc_find_startcode(p, end);
            while (p < end) {
                while(!*(p++));
                nal_end = ff_avc_find_startcode(p, end);
                nal_type = *p & 0x1f;
                if (nal_type == 7 && !sps) { /* SPS */
                    sps = p;
                    sps_size = nal_end - p;
                } else if (nal_type == 8 && !pps) { /* PPS */
                    pps = p;
                    pps_size = nal_end - p;
                }
                p = nal_end;
            }

            assert(sps);
            assert(pps);

            put_byte(pb, 1); /* version */
            put_byte(pb, sps[1]); /* profile */
            put_byte(pb, sps[2]); /* profile compat */
            put_byte(pb, sps[3]); /* level */
            put_byte(pb, 0xff); /* 6 bits reserved (111111) + 2 bits nal size length - 1 (11) */
            put_byte(pb, 0xe1); /* 3 bits reserved (111) + 5 bits number of sps (00001) */

            put_be16(pb, sps_size);
            put_buffer(pb, sps, sps_size);
            put_byte(pb, 1); /* number of pps */
            put_be16(pb, pps_size);
            put_buffer(pb, pps, pps_size);
        } else {
            put_buffer(pb, avctx->extradata, avctx->extradata_size);
        }
    }
    return 0;
}
