#!/bin/sh
#
# automatic regression test for libavformat
#
#
#set -x

set -e

. $(dirname $0)/regression-funcs.sh

eval do_$test=y

do_lavf()
{
    file=${outfile}$4
    [ -z $4 ] && file=${outfile}lavf.$1
    do_ffmpeg $file -t 1 -f image2 -vcodec pgmyuv -i $raw_src -f s16le -i $pcm_src -qscale 10 $ENC_OPTS $2
    do_ffmpeg_crc $file -i $target_path/$file $3
}

do_streamed_images()
{
    file=${outfile}${1}pipe.$1
    do_ffmpeg $file -t 1 -f image2 -vcodec pgmyuv -i $raw_src -qscale 10 $BITEXACT -f image2pipe
    do_ffmpeg_crc $file -f image2pipe -i $target_path/$file
}

do_image_formats()
{
    outfile="$datadir/images/$1/"
    mkdir -p "$outfile"
    file=${outfile}%02d.$1
    $echov $ffmpeg -t 0.5 -y -f image2 -vcodec pgmyuv -i $raw_src $2 $3 -qscale 10 $BITEXACT $target_path/$file
    $ffmpeg -t 0.5 -y -f image2 -vcodec pgmyuv -i $raw_src $2 $3 -qscale 10 $BITEXACT $target_path/$file
    do_md5sum ${outfile}02.$1 >> $logfile
    do_ffmpeg_crc $file $3 -i $target_path/$file
    wc -c ${outfile}02.$1 >> $logfile
}

do_audio_only()
{
    file=${outfile}lavf.$1
    do_ffmpeg $file -t 1 $2 -f s16le -i $pcm_src $BITEXACT $3
    do_ffmpeg_crc $file -i $target_path/$file
}

rm -f "$logfile"
rm -f "$benchfile"

if [ -n "$do_avi" ] ; then
do_lavf avi
fi

if [ -n "$do_asf" ] ; then
do_lavf asf "-acodec mp2" "-r 25"
fi

if [ -n "$do_rm" ] ; then
file=${outfile}lavf.rm
do_ffmpeg $file -t 1 -f image2 -vcodec pgmyuv -i $raw_src -f s16le -i $pcm_src -qscale 10 $ENC_OPTS -acodec ac3_fixed
# broken
#do_ffmpeg_crc $file -i $target_path/$file
fi

if [ -n "$do_mpg" ] ; then
do_lavf mpg
fi

if [ -n "$do_mxf" ] ; then
do_lavf mxf '-ar 48000 -bf 2 -timecode 02:56:14:13'
do_lavf mxf_d10 "-ar 48000 -ac 2 -r 25 -target imx30 -f mxf_d10"
do_lavf mxf '-ar 48000 -r 30000/1001 -timecode 02:56:14;13' '' 'lavf_ntsc_tc.mxf'
fi

if [ -n "$do_ts" ] ; then
do_lavf ts
fi

if [ -n "$do_swf" ] ; then
do_lavf swf -an
fi

if [ -n "$do_ffm" ] ; then
do_lavf ffm
fi

if [ -n "$do_flv_fmt" ] ; then
do_lavf flv -an
fi

if [ -n "$do_mov" ] ; then
do_lavf mov "-acodec pcm_alaw"
do_lavf mov "-acodec pcm_s24le -timecode 11:02:53:20" "" "lavf_tc.mov"
do_lavf mov "-target imx50" "" "lavf_imx50.mov"
fi

if [ -n "$do_dv_fmt" ] ; then
do_lavf dv "-ar 48000 -r 25 -vf scale=720:576 -ac 2"
fi

if [ -n "$do_gxf" ] ; then
do_lavf gxf "-ar 48000 -r 25 -vf scale=720:576 -ac 1"
fi

if [ -n "$do_nut" ] ; then
do_lavf nut "-acodec mp2"
fi

if [ -n "$do_mkv" ] ; then
do_lavf mkv
fi


# streamed images
# mjpeg
#file=${outfile}lavf.mjpeg
#do_ffmpeg $file -t 1 -qscale 10 -f image2 -vcodec pgmyuv -i $raw_src
#do_ffmpeg_crc $file -i $target_path/$file

if [ -n "$do_pbmpipe" ] ; then
do_streamed_images pbm
fi

if [ -n "$do_pgmpipe" ] ; then
do_streamed_images pgm
fi

if [ -n "$do_ppmpipe" ] ; then
do_streamed_images ppm
fi

if [ -n "$do_gif" ] ; then
file=${outfile}lavf.gif
do_ffmpeg $file -t 1 -f image2 -vcodec pgmyuv -i $raw_src $BITEXACT -pix_fmt rgb24
do_ffmpeg_crc $file -i $target_path/$file -pix_fmt rgb24
fi

if [ -n "$do_yuv4mpeg" ] ; then
file=${outfile}lavf.y4m
do_ffmpeg $file -t 1 -f image2 -vcodec pgmyuv -i $raw_src
#do_ffmpeg_crc $file -i $target_path/$file
fi

# image formats

if [ -n "$do_pgm" ] ; then
do_image_formats pgm
fi

if [ -n "$do_ppm" ] ; then
do_image_formats ppm
fi

if [ -n "$do_png" ] ; then
do_image_formats png
fi

if [ -n "$do_bmp" ] ; then
do_image_formats bmp
fi

if [ -n "$do_tga" ] ; then
do_image_formats tga
fi

if [ -n "$do_tiff" ] ; then
do_image_formats tiff "-pix_fmt rgb24"
fi

if [ -n "$do_sgi" ] ; then
do_image_formats sgi
fi

if [ -n "$do_jpg" ] ; then
do_image_formats jpg "-flags +bitexact -dct fastint -idct simple -pix_fmt yuvj420p" "-f image2"
fi

if [ -n "$do_pcx" ] ; then
do_image_formats pcx
fi

# audio only

if [ -n "$do_wav" ] ; then
do_audio_only wav
fi

if [ -n "$do_alaw" ] ; then
do_audio_only al
fi

if [ -n "$do_mulaw" ] ; then
do_audio_only ul
fi

if [ -n "$do_au" ] ; then
do_audio_only au
fi

if [ -n "$do_mmf" ] ; then
do_audio_only mmf
fi

if [ -n "$do_aiff" ] ; then
do_audio_only aif
fi

if [ -n "$do_voc" ] ; then
do_audio_only voc
fi

if [ -n "$do_voc_s16" ] ; then
do_audio_only s16.voc "-ac 2" "-acodec pcm_s16le"
fi

if [ -n "$do_ogg" ] ; then
do_audio_only ogg
fi

if [ -n "$do_rso" ] ; then
do_audio_only rso
fi

# pix_fmt conversions

if [ -n "$do_pixfmt" ] ; then
outfile="$datadir/pixfmt/"
mkdir -p "$outfile"
conversions="yuv420p yuv422p yuv444p yuyv422 yuv410p yuv411p yuvj420p \
             yuvj422p yuvj444p rgb24 bgr24 rgb32 rgb565 rgb555 gray monow \
             monob yuv440p yuvj440p"
for pix_fmt in $conversions ; do
    file=${outfile}${pix_fmt}.yuv
    do_ffmpeg_nocheck $file -r 1 -t 1 -f image2 -vcodec pgmyuv -i $raw_src $BITEXACT \
                            -f rawvideo -vf scale=352:288 -pix_fmt $pix_fmt $target_path/$raw_dst
    do_ffmpeg $file -f rawvideo -s 352x288 -pix_fmt $pix_fmt -i $target_path/$raw_dst \
                    -f rawvideo -s 352x288 -pix_fmt yuv444p $BITEXACT
done
fi