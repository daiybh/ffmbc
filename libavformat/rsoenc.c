/*
 * RSO muxer
 * Copyright (c) 2001 Fabrice Bellard (original AU code)
 * Copyright (c) 2010 Rafael Carre
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

#include "avformat.h"
#include "internal.h"
#include "riff.h"
#include "rso.h"

static int rso_write_header(AVFormatContext *s)
{
    ByteIOContext  *pb  = s->pb;
    AVCodecContext *enc = s->streams[0]->codec;

    if (!enc->codec_tag)
        return AVERROR_INVALIDDATA;

    if (enc->channels != 1) {
        av_log(s, AV_LOG_ERROR, "RSO only supports mono\n");
        return AVERROR_INVALIDDATA;
    }

    if (url_is_streamed(s->pb)) {
        av_log(s, AV_LOG_ERROR, "muxer does not support non seekable output\n");
        return AVERROR_INVALIDDATA;
    }

    /* XXX: find legal sample rates (if any) */
    if (enc->sample_rate >= 1u<<16) {
        av_log(s, AV_LOG_ERROR, "Sample rate must be < 65536\n");
        return AVERROR_INVALIDDATA;
    }

    if (enc->codec_id == CODEC_ID_ADPCM_IMA_WAV) {
        av_log(s, AV_LOG_ERROR, "ADPCM in RSO not implemented\n");
        return AVERROR_PATCHWELCOME;
    }

    /* format header */
    put_be16(pb, enc->codec_tag);   /* codec ID */
    put_be16(pb, 0);                /* data size, will be written at EOF */
    put_be16(pb, enc->sample_rate);
    put_be16(pb, 0x0000);           /* play mode ? (0x0000 = don't loop) */

    put_flush_packet(pb);

    return 0;
}

static int rso_write_packet(AVFormatContext *s, AVPacket *pkt)
{
    put_buffer(s->pb, pkt->data, pkt->size);
    return 0;
}

static int rso_write_trailer(AVFormatContext *s)
{
    ByteIOContext *pb = s->pb;
    int64_t file_size;
    uint16_t coded_file_size;

    file_size = url_ftell(pb);

    if (file_size < 0)
        return file_size;

    if (file_size > 0xffff + RSO_HEADER_SIZE) {
        av_log(s, AV_LOG_WARNING,
               "Output file is too big (%"PRId64" bytes >= 64kB)\n", file_size);
        coded_file_size = 0xffff;
    } else {
        coded_file_size = file_size - RSO_HEADER_SIZE;
    }

    /* update file size */
    url_fseek(pb, 2, SEEK_SET);
    put_be16(pb, coded_file_size);
    url_fseek(pb, file_size, SEEK_SET);

    put_flush_packet(pb);

    return 0;
}

AVOutputFormat ff_rso_muxer = {
    .name           =   "rso",
    .long_name      =   NULL_IF_CONFIG_SMALL("Lego Mindstorms RSO format"),
    .extensions     =   "rso",
    .priv_data_size =   0,
    .audio_codec    =   CODEC_ID_PCM_U8,
    .video_codec    =   CODEC_ID_NONE,
    .write_header   =   rso_write_header,
    .write_packet   =   rso_write_packet,
    .write_trailer  =   rso_write_trailer,
    .codec_tag      =   (const AVCodecTag* const []){ff_codec_rso_tags, 0},
};
