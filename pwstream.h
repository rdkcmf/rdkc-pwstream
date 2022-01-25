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
#define RDKC_FAILURE            -1
#define RDKC_SUCCESS            0

#define PWS_DEF_STREAM_NAME 		"FRAME_RENDER"
#define PWS_DEF_MEDIA_TYPE 		"Video"
#define PWS_DEF_MEDIA_CATEGORY 		"Capture"
#define PWS_DEF_MEDIA_ROLE 		"Camera"
#define PWS_DEF_MEDIA_TYPE_FORMAT   	PWS_MEDIA_TYPE_FORMAT_VIDEO
#define PWS_DEF_MEDIA_SUBTYPE_FORMAT 	PWS_MEDIA_SUBTYPE_FORMAT_H264
#define PWS_DEF_VIDEO_FORMAT 		PWS_VIDEO_FORMAT_ENCODED
#define PWS_DEF_FRAME_WIDTH		640
#define PWS_DEF_FRAME_HEIGHT 		480
#define PWS_DEF_FRAMERATE 		25

#define MAX_BUFQUEUE 30
#define FRAME_SIZE   10

/***** Enum Decclaration *****/
typedef enum pws_format
{
    PWS_FORMAT_INVALID ,
    PWS_FORMAT_MEDIA_TYPE ,
    PWS_FORMAT_MEDIA_SUBTYPE ,
    PWS_FORMAT_VIDEO ,
}PWS_FORMAT;

typedef enum pws_media_type_format
{
    PWS_MEDIA_TYPE_FORMAT_INVALID ,
    PWS_MEDIA_TYPE_FORMAT_VIDEO ,
    PWS_MEDIA_TYPE_FORMAT_AUDIO ,
}PWS_MEDIA_TYPE_FORMAT;

typedef enum pws_media_subtype_format
{
    PWS_MEDIA_SUBTYPE_FORMAT_INVALID ,
    PWS_MEDIA_SUBTYPE_FORMAT_RAW ,
    PWS_MEDIA_SUBTYPE_FORMAT_H264 ,
}PWS_MEDIA_SUBTYPE_FORMAT;

typedef enum pws_video_format
{
    PWS_VIDEO_FORMAT_INVALID ,
    PWS_VIDEO_FORMAT_ENCODED ,
}PWS_VIDEO_FORMAT;

typedef enum pws_app_type
{
    PWS_APP_TYPE_INVALID ,
    PWS_APP_TYPE_RMS ,
    PWS_APP_TYPE_CVR ,
}PWS_APP_TYPE;

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
    int width;
    int height;    
    int framerate;
    PWS_APP_TYPE enAppType;
    int pws_fd[2];
};

struct pws_data {
    struct pw_main_loop *loop;
    struct pw_stream *stream;
    struct spa_video_info format;

    struct pws_prioperties streamprop;
};

typedef struct pws_frameInfo
{
    unsigned stream_id;              // buffer id (0~3)
    unsigned stream_type;            // 0 = Video H264 frame, 1 = Audio frame
    unsigned *frame_ptr;             // same as guint8 *y_addr
    unsigned width;                  // Buffer Width
    unsigned height;                 // Buffer Height
    unsigned long long frame_size;             // FrameSize = Width * Height * 1.5 (Y + UV)
    unsigned long long frame_timestamp;        // Time stamp 8 bytes from GST
}pws_frameInfo;

/***** Prototype *****/
int pws_formatconversion( PWS_FORMAT enpwsformat, int formatval);
int pws_init_videostream(struct pws_data *pwsdata);
void pws_load_defaultstreamprop( struct pws_data *pwsdata);
int pws_start_videostreaming( void *vptr );
static void pws_on_param_changed(void *userdata, uint32_t id, const struct spa_pod *param);
static void pws_on_process(void *userdata);
int pws_ReadVideoFramedata( struct pws_data *pwsdata, pws_frameInfo *pstframeinfo);
int pws_deletebuffer(pws_frameInfo *pstframeinfo);
int pws_waitforvideoframe();
void pws_sem_postvideoframe();
int pws_terminateframe ( struct pws_data *pwsdata);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* PWSTREAM_H */

