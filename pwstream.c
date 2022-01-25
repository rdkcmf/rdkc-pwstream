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

/***** HEADER FILE *****/
#include "pwstream.h"
#include "pipewire/pipewire.h"
#include <pthread.h>
#include <semaphore.h>

/***** Global Variable Declaration *****/

pthread_t pws_getFrame;
pthread_mutex_t pws_videoframelock;
sem_t pws_applock;

/***** Function Definition *****/

/* {{{ pws_load_defaultstreamprop() */
int pws_formatconversion( PWS_FORMAT enpwsformat, int formatval)
{
    if( PWS_FORMAT_MEDIA_TYPE == enpwsformat )
    {
        if( PWS_MEDIA_TYPE_FORMAT_VIDEO == formatval)
	    return SPA_MEDIA_TYPE_video;

	if( PWS_MEDIA_TYPE_FORMAT_AUDIO == formatval )
	    return SPA_MEDIA_TYPE_audio;
    }

    if( PWS_FORMAT_MEDIA_SUBTYPE == enpwsformat )
    {
        if( PWS_MEDIA_SUBTYPE_FORMAT_RAW == formatval )
	    return SPA_MEDIA_SUBTYPE_raw;

	if( PWS_MEDIA_SUBTYPE_FORMAT_H264 == formatval )
	    return SPA_MEDIA_SUBTYPE_h264;
    }

    if( PWS_FORMAT_VIDEO == enpwsformat   )
    {
        if( PWS_VIDEO_FORMAT_ENCODED == formatval )
	    return SPA_VIDEO_FORMAT_ENCODED;
    }

    return RDKC_FAILURE;
}
/* }}} */

/* {{{ pws_init_videostream() */
int pws_init_videostream(struct pws_data *pwsdata)
{
    int ret = 0;

    if( -1 == pipe(pwsdata->streamprop.pws_fd) )
    {
        return -1;
    }

    pws_load_defaultstreamprop(pwsdata);

    if(pthread_mutex_init(&pws_videoframelock, NULL) != 0)
    {
        printf("%s(%d): Mutex init for queue failed...\n", __FILE__, __LINE__);
        return -1;
    }

    if ( sem_init( &pws_applock, 0, 0 ) )
    {
        printf("%s(%d): Sem init failed...\n", __FILE__, __LINE__);
        return -1;
    }

    ret = pthread_create( &pws_getFrame, NULL, pws_start_videostreaming, (void*)pwsdata );

    if(ret != 0)
    {
        printf("%s(%d): Failed to create start_Streaming thread,error_code: %d\n",
                        __FILE__, __LINE__,ret);
    }
    
    return pwsdata->streamprop.pws_fd[0];

}
/* }}} */

/* {{{ pws_load_defaultstreamprop() */
void pws_load_defaultstreamprop( struct pws_data *pwsdata)
{
    if( NULL == pwsdata )
        return;

    if( NULL == pwsdata->streamprop.stream_name )
        pwsdata->streamprop.stream_name = PWS_DEF_STREAM_NAME;

    if( NULL == pwsdata->streamprop.mediatype )
        pwsdata->streamprop.mediatype = PWS_DEF_MEDIA_TYPE;

    if( NULL == pwsdata->streamprop.mediacategory )
        pwsdata->streamprop.mediacategory = PWS_DEF_MEDIA_CATEGORY;

    if( NULL == pwsdata->streamprop.mediarole )
        pwsdata->streamprop.mediarole = PWS_DEF_MEDIA_ROLE;

    if( PWS_MEDIA_TYPE_FORMAT_INVALID == pwsdata->streamprop.enMtypeformat )
        pwsdata->streamprop.enMtypeformat = PWS_DEF_MEDIA_TYPE_FORMAT;
        pwsdata->streamprop.enMtypeformat = pws_formatconversion(PWS_FORMAT_MEDIA_TYPE,
		    					     pwsdata->streamprop.enMtypeformat);

    if( PWS_MEDIA_SUBTYPE_FORMAT_INVALID == pwsdata->streamprop.enMsubtypeformat )
        pwsdata->streamprop.enMsubtypeformat = PWS_DEF_MEDIA_SUBTYPE_FORMAT;
        pwsdata->streamprop.enMsubtypeformat = pws_formatconversion(PWS_FORMAT_MEDIA_SUBTYPE,
                                                             pwsdata->streamprop.enMsubtypeformat);

    if( PWS_VIDEO_FORMAT_INVALID == pwsdata->streamprop.envideoformat )
        pwsdata->streamprop.envideoformat = PWS_DEF_VIDEO_FORMAT;
        pwsdata->streamprop.envideoformat = pws_formatconversion(PWS_FORMAT_VIDEO,
                                                             pwsdata->streamprop.envideoformat);

    if( 0 == pwsdata->streamprop.width )
        pwsdata->streamprop.width = PWS_DEF_FRAME_WIDTH;

    if( 0 == pwsdata->streamprop.height )
        pwsdata->streamprop.height = PWS_DEF_FRAME_HEIGHT;

    if( 0 == pwsdata->streamprop.framerate )
        pwsdata->streamprop.framerate = PWS_DEF_FRAMERATE;

}
/* }}} */

static const struct pw_stream_events pws_stream_events = {
        PW_VERSION_STREAM_EVENTS,
        .param_changed = pws_on_param_changed,
        .process = pws_on_process,
};

/* {{{ pws_start_videostreaming() */
int pws_start_videostreaming( void *vptr )
{
    const struct spa_pod *params[1];
    uint8_t buffer[1024];
    struct spa_pod_builder b = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));

    struct pws_data *pwsdata = (struct pws_data *)vptr;

    if( NULL == pwsdata )
        return RDKC_FAILURE;

    printf(" mtype<%s> mcategory<%s> mediarole<%s> Mtypeformat<%d> MsubtypeFMT<%d> Videoformat<%d> Width<%d> height<%d> framerate<%d>",
                    pwsdata->streamprop.mediatype,
                    pwsdata->streamprop.mediacategory,
                    pwsdata->streamprop.mediarole,
                    pwsdata->streamprop.enMtypeformat,
                    pwsdata->streamprop.enMsubtypeformat,
                    pwsdata->streamprop.envideoformat,
                    pwsdata->streamprop.width,
                    pwsdata->streamprop.height,
                    pwsdata->streamprop.framerate );

    pw_init(NULL, NULL);

    pwsdata->loop = pw_main_loop_new(NULL);

    pwsdata->stream = pw_stream_new_simple(
                          pw_main_loop_get_loop(pwsdata->loop),
                          pwsdata->streamprop.stream_name,
                          pw_properties_new(
                              PW_KEY_MEDIA_TYPE, pwsdata->streamprop.mediatype,
                              PW_KEY_MEDIA_CATEGORY, pwsdata->streamprop.mediacategory,
                              PW_KEY_MEDIA_ROLE, pwsdata->streamprop.mediarole,
                              NULL),
                          &pws_stream_events,
                          pwsdata);

    params[0] = spa_pod_builder_add_object(&b,
                    SPA_TYPE_OBJECT_Format,   SPA_PARAM_EnumFormat,
                    SPA_FORMAT_mediaType,     SPA_POD_Id(pwsdata->streamprop.enMtypeformat),
                    SPA_FORMAT_mediaSubtype,  SPA_POD_Id(pwsdata->streamprop.enMsubtypeformat),
                    SPA_FORMAT_VIDEO_format,  SPA_POD_Id(pwsdata->streamprop.envideoformat),
                    SPA_FORMAT_VIDEO_size,    SPA_POD_Rectangle(
			                          &SPA_RECTANGLE(
					              pwsdata->streamprop.width, 
						      pwsdata->streamprop.height)),
		    SPA_FORMAT_VIDEO_framerate, SPA_POD_CHOICE_RANGE_Fraction(
                                                              &SPA_FRACTION(25, 1),
                                                              &SPA_FRACTION(0, 1),
                                                              &SPA_FRACTION(1000, 1)));


    pw_stream_connect(pwsdata->stream,
                      PW_DIRECTION_INPUT,
                      PW_ID_ANY,
                      PW_STREAM_FLAG_AUTOCONNECT |
                      PW_STREAM_FLAG_MAP_BUFFERS,
                      params, 1);

    pw_main_loop_run(pwsdata->loop);

    pw_stream_destroy(pwsdata->stream);
    pw_main_loop_destroy(pwsdata->loop);

    return 0;
}
/* }}} */

/* {{{ pws_on_param_changed() */
static void pws_on_param_changed(void *userdata, uint32_t id, const struct spa_pod *param)
{
    struct pws_data *pwsdata = userdata;
    struct pw_stream *stream = pwsdata->stream;
    uint8_t params_buffer[1024];
    struct spa_pod_builder b = SPA_POD_BUILDER_INIT(params_buffer, sizeof(params_buffer));
    const struct spa_pod *params[5];

    if (param == NULL || id != SPA_PARAM_Format)
        return;

    if (spa_format_parse(param,
                         &pwsdata->format.media_type,
                         &pwsdata->format.media_subtype) < 0 )
        return;

    if (pwsdata->format.media_type != SPA_MEDIA_TYPE_video )
        return;

    if( pwsdata->format.media_subtype == SPA_MEDIA_SUBTYPE_raw )
    {
        if( spa_format_video_raw_parse(param, &pwsdata->format.info.raw) >= 0 )
        {
            printf("got video format:\n");
            printf("  format: %d (%s)\n", pwsdata->format.info.raw.format,
                                          spa_debug_type_find_name(spa_type_video_format,
                                                                   pwsdata->format.info.raw.format));
            printf("  size: %dx%d\n", pwsdata->format.info.raw.size.width,
                                      pwsdata->format.info.raw.size.height);
            printf("  framerate: %d/%d\n", pwsdata->format.info.raw.framerate.num,
                                           pwsdata->format.info.raw.framerate.denom);
        }
    }

    if( pwsdata->format.media_subtype == SPA_MEDIA_SUBTYPE_h264 )
    {
        if( spa_format_video_h264_parse(param, &pwsdata->format.info.h264) >= 0 )
        {
            printf("got video format: H264\n");
            printf("  size: %dx%d\n", pwsdata->format.info.h264.size.width,
                                      pwsdata->format.info.h264.size.height);
            printf("  framerate: %d/%d\n", pwsdata->format.info.h264.framerate.num,
                                           pwsdata->format.info.h264.framerate.denom);
        }
    }
   
    /* a SPA_TYPE_OBJECT_ParamBuffers object defines the acceptable size,
     * number, stride etc of the buffers */

    params[0] = spa_pod_builder_add_object(&b,
                SPA_TYPE_OBJECT_ParamBuffers, SPA_PARAM_Buffers,
                SPA_PARAM_BUFFERS_dataType, SPA_POD_CHOICE_FLAGS_Int((1<<SPA_DATA_MemPtr)));

    pw_stream_update_params(stream, params, 1);

}
/* }}} */


/* {{{ pws_terminateframe() */
int pws_terminateframe ( struct pws_data *pwsdata)
{
    pthread_mutex_destroy(&pws_videoframelock);

    return 0;
}
/* }}} */

/* {{{ pws_on_process() */
static void pws_on_process(void *userdata)
{
    struct pws_data *pwsdata = NULL;

    if(NULL == userdata )
        return;

    pwsdata = (struct pws_data *)userdata;

    if( PWS_APP_TYPE_RMS == pwsdata->streamprop.enAppType)
    {
        if (pwsdata->streamprop.pws_fd != -1)
        {
            char wbuf[1] = {0};
            write(pwsdata->streamprop.pws_fd[1], wbuf, sizeof(wbuf));
        }
    }

    if( PWS_APP_TYPE_CVR == pwsdata->streamprop.enAppType)
    {
        pws_sem_postvideoframe();
    }

}
/* }}} */

/* {{{ pws_ReaVideodFramedata() */
int pws_ReadVideoFramedata( struct pws_data *pwsdata, pws_frameInfo *pstframeinfo)
{
    struct pw_buffer *b;
    struct spa_buffer *buf;
    struct timeb timer_msec;

    if( ( NULL == pwsdata ) || ( NULL == pstframeinfo ) )
    {
        return RDKC_FAILURE;
    }

    if ( pthread_mutex_lock( &pws_videoframelock ) != 0 )
    {
        return RDKC_FAILURE;
    }

    if ((b = pw_stream_dequeue_buffer(pwsdata->stream)) == NULL)
    {
        pw_log_warn("out of buffers: %m");
        return RDKC_FAILURE;
    }

    buf = b->buffer;

    if (buf->datas[0].data == NULL)
        return RDKC_FAILURE;

    if (!ftime(&timer_msec))
    {
       pstframeinfo->frame_timestamp = ((long long int) timer_msec.time) * 1000ll +
                                            (long long int) timer_msec.millitm;
    }

    pstframeinfo->frame_size = buf->datas[0].chunk->size;
    pstframeinfo->frame_ptr = malloc( pstframeinfo->frame_size );

    memcpy(pstframeinfo->frame_ptr,
	   buf->datas[0].data,
	   pstframeinfo->frame_size );

    pstframeinfo->stream_id = 4;
    pstframeinfo->stream_type = 1;
    pstframeinfo->width = pwsdata->streamprop.width;
    pstframeinfo->height = pwsdata->streamprop.height;

    if( PWS_APP_TYPE_RMS == pwsdata->streamprop.enAppType)
    {
        // read one byte from pipe
        if (pwsdata->streamprop.pws_fd[0] != -1) 
	{
            char rbuf[1] ={0};
            read(pwsdata->streamprop.pws_fd[0], rbuf, 1);
        }
    }

    pw_stream_queue_buffer(pwsdata->stream, b);

    pthread_mutex_unlock( &pws_videoframelock );

    return RDKC_SUCCESS;
}
/* }}} */

/* {{{ pws_deletebuffer() */
int pws_deletebuffer(pws_frameInfo *pstframeinfo)
{
    if( NULL != pstframeinfo )
    {
	free(pstframeinfo->frame_ptr);
	pstframeinfo->frame_ptr=NULL;

	return RDKC_FAILURE;
    }

    return RDKC_SUCCESS;
}
/* }}} */

/* {{{ pws_sem_waitforvideoframe() */
int pws_waitforvideoframe()
{
    if( sem_wait( &pws_applock ) )
    {
        return RDKC_FAILURE;
    }

    return RDKC_SUCCESS;
}
/* }}} */

/* {{{ pws_sem_postvideoframe() */
void pws_sem_postvideoframe()
{
      sem_post( &pws_applock );
}
/* }}} */

