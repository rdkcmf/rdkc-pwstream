/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2022 RDK Management
 *
 * Licensed under the Apache License, Version 2.0 (the License);
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef PWSTREAM_H
#define PWSTREAM_H

#ifdef __cplusplus
extern "C" {
#endif

/***** HEADER FILE *****/
#include "spa/param/video/format-utils.h"
#include "spa/debug/types.h"
#include "spa/param/video/type-info.h"
#include "spa/param/video/format.h"
#include <sys/timeb.h>

/***** MACROS *****/
typedef unsigned char           u8;     /**< UNSIGNED  8-bit data type */
typedef unsigned short          u16;     /**< UNSIGNED 16-bit data type */
typedef unsigned int            u32;     /**< UNSIGNED 32-bit data type */
typedef unsigned long long      u64;     /**< UNSIGNED 64-bit data type */
typedef signed char             s8;    /**<   SIGNED  8-bit data type */
typedef signed short            s16;    /**<   SIGNED 16-bit data type */
typedef signed int              s32;    /**<   SIGNED 32-bit data type */
typedef signed long long        s64;    /**<   SIGNED 64-bit data type */

#define PWS_DEF_STREAM_NAME 		"FRAME_RENDER"
#define PWS_DEF_MEDIA_TYPE 		"Video"
#define PWS_DEF_MEDIA_CATEGORY 		"Capture"
#define PWS_DEF_MEDIA_ROLE 		"Camera"
#define PWS_DEF_MEDIA_TYPE_FORMAT	PWS_MEDIA_TYPE_FORMAT_VIDEO
#define PWS_DEF_MEDIA_SUBTYPE_FORMAT	PWS_MEDIA_SUBTYPE_FORMAT_H264
#define PWS_DEF_VIDEO_FORMAT 		PWS_VIDEO_FORMAT_ENCODED
#define PWS_DEF_FRAME_WIDTH		640
#define PWS_DEF_FRAME_HEIGHT 		480
#define PWS_DEF_FRAMERATE 		25

#define FRAME_SIZE   10

/***** Enum Decclaration *****/
typedef enum pws_error
{
    PWS_FAILURE = -1 ,
    PWS_SUCCESS = 0 ,
    PWS_FRAME_NOT_READY = 1 ,
    PWS_INVALID_PARAM ,
    PWS_OPERATION_NOT_SUPPORTED ,
    PWS_UNKNOWN ,    
}PWS_ERROR;

typedef enum pws_format
{
    PWS_FORMAT_INVALID ,
    PWS_FORMAT_MEDIA_TYPE ,
    PWS_FORMAT_MEDIA_SUBTYPE ,
    PWS_FORMAT_VIDEO ,
}PWS_FORMAT;

typedef enum pws_media_type_format
{
    PWS_MEDIA_TYPE_FORMAT_START ,
    PWS_MEDIA_TYPE_FORMAT_VIDEO ,
    PWS_MEDIA_TYPE_FORMAT_AUDIO ,
    PWS_MEDIA_TYPE_FORMAT_END ,
}PWS_MEDIA_TYPE_FORMAT;

typedef enum pws_media_subtype_format
{
    PWS_MEDIA_SUBTYPE_FORMAT_START ,
    PWS_MEDIA_SUBTYPE_FORMAT_RAW ,
    PWS_MEDIA_SUBTYPE_FORMAT_H264 ,
    PWS_MEDIA_SUBTYPE_FORMAT_END ,
}PWS_MEDIA_SUBTYPE_FORMAT;

typedef enum pws_video_format
{
    PWS_VIDEO_FORMAT_START ,
    PWS_VIDEO_FORMAT_ENCODED ,
    PWS_VIDEO_FORMAT_END ,
}PWS_VIDEO_FORMAT;

typedef enum pws_pic_type
{
    PWS_PIC_TYPE_INVALID ,
    PWS_PIC_TYPE_IDR_FRAME ,
    PWS_PIC_TYPE_I_FRAME ,
    PWS_PIC_TYPE_P_FRAME
}PWS_PIC_TYPE;

/***** Structure Declaration *****/

struct pws_prioperties
{
    const char *stream_name;
    const char *mediatype;
    const char *mediacategory;
    const char *mediarole;
    PWS_MEDIA_TYPE_FORMAT enMtypeformat;
    PWS_MEDIA_SUBTYPE_FORMAT enMsubtypeformat;
    PWS_VIDEO_FORMAT envideoformat;
    u32 width;
    u32 height;    
    u32 framerate;
};

typedef struct pws_frameInfo
{
    s16 stream_id;              // buffer id (0~3)
    u16 stream_type;            // 0 = Video H264 frame, 1 = Audio frame
    u32 pic_type;		// 1 = IDR Frame 2 = I Frame 3 = P Frame
    u8 *frame_ptr;             // same as guint8 *y_addr
    u32 width;                  // Buffer Width
    u32 height;                 // Buffer Height
    u32 frame_size;             // FrameSize = Width * Height * 1.5 (Y + UV)
    u32 frame_timestamp;        // Time stamp 8 bytes from GST
}pws_frameInfo;

struct pws_data {
    struct pw_main_loop *loop;
    struct pw_stream *stream;
    struct spa_video_info format;

    struct pws_prioperties streamprop;

    s32 pws_fd[2];

    pws_frameInfo *intermediateFrameInfoH264;
    bool isH264FrameReady;
};

/***** Prototype *****/
int pws_StreamInit(struct pws_data *pwsdata);
int pws_ReadFrame( struct pws_data *pwsdata, pws_frameInfo *pstframeinfo);
int pws_StreamClose( struct pws_data *pwsdata, pws_frameInfo *pstframeinfo );

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* PWSTREAM_H */

