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
#include <fcntl.h>

/*RDK Logging */
#include "rdk_debug.h"

/***** Global Variable Declaration *****/

pthread_t pws_getFrame;
pthread_mutex_t pws_videoframelock;

/***** Prototype *****/
static int pws_FormatConversion( PWS_FORMAT enpwsformat, int formatval);
static void pws_Load_DefaultStreamProp( struct pws_data *pwsdata);
static int pws_StartStream( void *vptr );
static void pws_OnParamChanged(void *userdata, uint32_t id, const struct spa_pod *param);
static void pws_OnProcess(void *userdata);
static u32 pws_GetH264PictureType( u8 *Framedata );

/***** Function Definition *****/

/** @description: Format Conversion from pwstream format to Pipewire format
 *  @param[in]: pwsdata
 *  @return: Video/Audio specific Format
 */
/* {{{ pws_FormatConversion() */
static int pws_FormatConversion( PWS_FORMAT enpwsformat, int formatval)
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

    return PWS_FAILURE;
}
/* }}} */

/** @description: Inilializing logs,video properties and memory allocation
 *  @param[in]: pwsdata
 *  @return: File Descriptor
 */
/* {{{ pws_StreamInit() */
int pws_StreamInit(struct pws_data *pwsdata)
{
    int ret = 0;

    /* RDK logger initialization */
    rdk_logger_init("/etc/debug.ini");

    if(NULL == pwsdata )
        return PWS_FAILURE;

    pwsdata->isH264FrameReady = false;

    if( NULL == pwsdata->intermediateFrameInfoH264)
    {
        pwsdata->intermediateFrameInfoH264 = (pws_frameInfo*)malloc(sizeof(pws_frameInfo));

	if( NULL == pwsdata->intermediateFrameInfoH264 )
	{
	    RDK_LOG(RDK_LOG_ERROR,"LOG.RDK.PWSTREAM","%s(%d) : Failed to allocate memory \n",__FILE__, __LINE__);
	    return PWS_FAILURE;
	}
	else
	{
	    memset(pwsdata->intermediateFrameInfoH264,0,sizeof(pws_frameInfo));
	}

	pwsdata->intermediateFrameInfoH264->frame_ptr = (u8*)malloc(sizeof(u8)*FRAME_SIZE);

        if( NULL == pwsdata->intermediateFrameInfoH264->frame_ptr )
        {
            RDK_LOG(RDK_LOG_ERROR,"LOG.RDK.PWSTREAM","%s(%d) : Failed to allocate memory \n",__FILE__, __LINE__);
            return PWS_FAILURE;
        }
        else
        {
            memset(pwsdata->intermediateFrameInfoH264->frame_ptr,0,(sizeof(u8)*FRAME_SIZE));
        }
    }

    pws_Load_DefaultStreamProp(pwsdata);

    memset (pwsdata->pws_fd, -1, 2 *sizeof(s32));

    //create pipe and return its one end point(read fd)
    if( -1 != pipe(pwsdata->pws_fd) )
    {
        if (fcntl(pwsdata->pws_fd[0], F_SETFL, O_NONBLOCK) < 0) {
            RDK_LOG(RDK_LOG_WARN,"LOG.RDK.PWSTREAM","%s(%d) : Error in setting the read end of PIPE(%d<------%d) to non blocking mode\n", __FILE__, __LINE__, pwsdata->pws_fd[0], pwsdata->pws_fd[1]);
        }
        else {
            RDK_LOG(RDK_LOG_INFO,"LOG.RDK.PWSTREAM","%s(%d) : Set the read end of PIPE(%d<------%d) to non blocking mode\n", __FILE__, __LINE__, pwsdata->pws_fd[0], pwsdata->pws_fd[1]);
        }

        if (fcntl(pwsdata->pws_fd[1], F_SETFL, O_NONBLOCK) < 0) {
            RDK_LOG(RDK_LOG_WARN,"LOG.RDK.PWSTREAM","%s(%d) : Error in setting the write end of PIPE(%d<------%d) to non blocking mode\n", __FILE__, __LINE__, pwsdata->pws_fd[0], pwsdata->pws_fd[1]);
        }
        else {
            RDK_LOG(RDK_LOG_INFO,"LOG.RDK.PWSTREAM","%s(%d) : Set the write end of PIPE(%d<------%d) to non blocking mode\n", __FILE__, __LINE__, pwsdata->pws_fd[0], pwsdata->pws_fd[1]);
        }
    }

    if(pthread_mutex_init(&pws_videoframelock, NULL) != 0)
    {
	RDK_LOG(RDK_LOG_ERROR,"LOG.RDK.PWSTREAM","%s(%d) : FrameLock Mutex Init Failed \n",__FILE__, __LINE__);
        return PWS_FAILURE;
    }

    ret = pthread_create( &pws_getFrame, NULL, pws_StartStream, (void*)pwsdata );

    if(ret != 0)
    {
	RDK_LOG(RDK_LOG_ERROR,"LOG.RDK.PWSTREAM","%s(%d) : Failed to create start_Streaming thread \n",__FILE__, __LINE__);
    }
    
    return pwsdata->pws_fd[0];

}
/* }}} */

/** @description: Update video/Audio properties in streamprop structure
 *  @param[in]: pwsdata
 *  @return: None
 */
/* {{{ pws_Load_DefaultStreamProp() */
static void pws_Load_DefaultStreamProp( struct pws_data *pwsdata)
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

    if( !( PWS_MEDIA_TYPE_FORMAT_START < pwsdata->streamprop.enMtypeformat < PWS_MEDIA_TYPE_FORMAT_END ) )
        pwsdata->streamprop.enMtypeformat = PWS_DEF_MEDIA_TYPE_FORMAT;
        pwsdata->streamprop.enMtypeformat = pws_FormatConversion(PWS_FORMAT_MEDIA_TYPE,
		    					     pwsdata->streamprop.enMtypeformat);

    if( !( PWS_MEDIA_SUBTYPE_FORMAT_START < pwsdata->streamprop.enMsubtypeformat < PWS_MEDIA_SUBTYPE_FORMAT_END ) )
        pwsdata->streamprop.enMsubtypeformat = PWS_DEF_MEDIA_SUBTYPE_FORMAT;
        pwsdata->streamprop.enMsubtypeformat = pws_FormatConversion(PWS_FORMAT_MEDIA_SUBTYPE,
                                                             pwsdata->streamprop.enMsubtypeformat);

    if( !( PWS_VIDEO_FORMAT_START < pwsdata->streamprop.envideoformat < PWS_VIDEO_FORMAT_END ) )
        pwsdata->streamprop.envideoformat = PWS_DEF_VIDEO_FORMAT;
        pwsdata->streamprop.envideoformat = pws_FormatConversion(PWS_FORMAT_VIDEO,
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
        .param_changed = pws_OnParamChanged,
        .process = pws_OnProcess,
};

/** @description: Start the read of video buffer with pipewire callback
 *  @param[in]: pwsdata
 *  @return: Macro- Success/Failure
 */
/* {{{ pws_StartStream() */
static int pws_StartStream( void *vptr )
{
    const struct spa_pod *params[1];
    uint8_t buffer[1024];
    struct spa_pod_builder b = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));

    struct pws_data *pwsdata = (struct pws_data *)vptr;

    if( NULL == pwsdata )
        return PWS_FAILURE;

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

    return 0;
}
/* }}} */

/** @description: Find triggered video capture parameter
 *  @param[in]: pwsdata and Parameter
 *  @return: None
 */
/* {{{ pws_OnParamChanged() */
static void pws_OnParamChanged(void *userdata, uint32_t id, const struct spa_pod *param)
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

/** @description: Dequeue video frame from pipewire instance
 *  @param[in]: pwsdata
 *  @return: None
 */
/* {{{ pws_OnProcess() */
static void pws_OnProcess(void *userdata)
{
    struct pws_data *pwsdata = NULL;
    struct pw_buffer *b;
    struct spa_buffer *buf;
    struct timeb timer_msec;

    if(NULL == userdata )
        return PWS_FAILURE;

    pwsdata = (struct pws_data *)userdata;

    if( NULL == pwsdata->intermediateFrameInfoH264 )
        return PWS_FAILURE;

    if ( pthread_mutex_lock( &pws_videoframelock ) != 0 )
        return PWS_FAILURE;

    if ((b = pw_stream_dequeue_buffer(pwsdata->stream)) == NULL)
    {
	RDK_LOG(RDK_LOG_ERROR,"LOG.RDK.PWSTREAM","%s(%d) : Out of Buffers \n",__FILE__, __LINE__);
        return PWS_FAILURE;
    }

    buf = b->buffer;

    if (buf->datas[0].data == NULL)
        return PWS_FAILURE;


    /* Updating H264 frame details in intermediateFrameInfoH264 */
    if(SPA_MEDIA_SUBTYPE_h264 == pwsdata->streamprop.enMsubtypeformat )
    {
        if (!ftime(&timer_msec))
        {
           pwsdata->intermediateFrameInfoH264->frame_timestamp = ((long long int) timer_msec.time) * 1000ll +
                                                			(long long int) timer_msec.millitm;
        }

        pwsdata->intermediateFrameInfoH264->frame_size = buf->datas[0].chunk->size;

        pwsdata->intermediateFrameInfoH264->frame_ptr = (u8*)realloc(pwsdata->intermediateFrameInfoH264->frame_ptr,
		    						 pwsdata->intermediateFrameInfoH264->frame_size);

	if(NULL != pwsdata->intermediateFrameInfoH264->frame_ptr)
	{
	    memset(pwsdata->intermediateFrameInfoH264->frame_ptr,0,pwsdata->intermediateFrameInfoH264->frame_size);

	    memcpy(pwsdata->intermediateFrameInfoH264->frame_ptr,
			    buf->datas[0].data,
			    pwsdata->intermediateFrameInfoH264->frame_size);
	}

	pwsdata->intermediateFrameInfoH264->stream_type = 1;

	pwsdata->intermediateFrameInfoH264->width = pwsdata->streamprop.width;
	pwsdata->intermediateFrameInfoH264->height = pwsdata->streamprop.height;

	pwsdata->intermediateFrameInfoH264->pic_type = pws_GetH264PictureType( pwsdata->intermediateFrameInfoH264->frame_ptr );
    }

    if( ( true == pwsdata->isH264FrameReady) && ( -1 != pwsdata->pws_fd[0] ) )
    {
        char rbuf[1]={0};
	read(pwsdata->pws_fd[0], rbuf, 1);
	pwsdata->isH264FrameReady = false;
    }

    if(pwsdata->pws_fd[1] != -1) 
    {
        char wbuf[1] = {0};
	pwsdata->isH264FrameReady = true;
        write(pwsdata->pws_fd[1], wbuf, sizeof(wbuf));
    }

    pw_stream_queue_buffer(pwsdata->stream, b);

    pthread_mutex_unlock( &pws_videoframelock );
}
/* }}} */

/** @description: Get video frame from pwstream instance
 *  @param[in]: pwsdata and application frame info
 *  @return: Macro - Success/Failure
 */
/* {{{ pws_ReadFrame() */
int pws_ReadFrame( struct pws_data *pwsdata, pws_frameInfo *pstframeinfo)
{
    if( ( NULL == pwsdata ) || ( NULL == pstframeinfo ) )
        return PWS_FAILURE;

    if( !pwsdata->isH264FrameReady )
    {
        RDK_LOG(RDK_LOG_ERROR,"LOG.RDK.PWSTREAM","%s(%d) Frame Not Ready \n",__FILE__, __LINE__);
	return PWS_FRAME_NOT_READY;
    }

    if ( pthread_mutex_lock( &pws_videoframelock ) != 0 )
        return PWS_FAILURE;

    pstframeinfo->stream_id = pwsdata->intermediateFrameInfoH264->stream_id;

    pstframeinfo->stream_type = pwsdata->intermediateFrameInfoH264->stream_type;

    pstframeinfo->pic_type = pwsdata->intermediateFrameInfoH264->pic_type;

    pstframeinfo->width = pwsdata->intermediateFrameInfoH264->width;

    pstframeinfo->height = pwsdata->intermediateFrameInfoH264->height;

    pstframeinfo->frame_size = pwsdata->intermediateFrameInfoH264->frame_size;

    pstframeinfo->frame_timestamp = pwsdata->intermediateFrameInfoH264->frame_timestamp;

    if( NULL == pstframeinfo->frame_ptr )
    {
       pstframeinfo->frame_ptr  = (u8*)malloc(pstframeinfo->frame_size);
    }
    else
    {
        pstframeinfo->frame_ptr  = (u8*)realloc( pstframeinfo->frame_ptr, pstframeinfo->frame_size);
    }

    memset( pstframeinfo->frame_ptr,0,pstframeinfo->frame_size);

    memcpy( pstframeinfo->frame_ptr,
            pwsdata->intermediateFrameInfoH264->frame_ptr,
	    pstframeinfo->frame_size);


    // read one byte from pipe
    if (pwsdata->pws_fd[0] != -1)
    {
        char rbuf[1] ={0};
        read(pwsdata->pws_fd[0], rbuf, 1);
    }

    pwsdata->isH264FrameReady = false;

    pthread_mutex_unlock( &pws_videoframelock );

    return PWS_SUCCESS;
}
/* }}} */

/** @description: To release Frame buffer
 *  @param[in]: pwsdata and application frame info
 *  @return: Macro - Success/Failure
 */
/* {{{ pws_StreamClose() */
int pws_StreamClose( struct pws_data *pwsdata, pws_frameInfo *pstframeinfo )
{
    if ( pthread_mutex_lock( &pws_videoframelock ) != 0 )
        return PWS_FAILURE;

    if( NULL != pwsdata->intermediateFrameInfoH264 )
    {
        if( NULL != pwsdata->intermediateFrameInfoH264->frame_ptr )
	{
	    free( pwsdata->intermediateFrameInfoH264->frame_ptr );
	    pwsdata->intermediateFrameInfoH264->frame_ptr = NULL;
	}

	free( pwsdata->intermediateFrameInfoH264 );
        pwsdata->intermediateFrameInfoH264 = NULL;
    }


    if( NULL != pstframeinfo )
    {
	free(pstframeinfo->frame_ptr);
	pstframeinfo->frame_ptr = NULL;
    }

    if( ( true == pwsdata->isH264FrameReady ) && ( pwsdata->pws_fd[0] != -1 ) ) 
    {
	char rbuf[1]={0};
	read(pwsdata->pws_fd[0], rbuf, 1);
	pwsdata->isH264FrameReady = false;
    }

    // cleanup the pipe fd
    close( pwsdata->pws_fd[0] );
    pwsdata->pws_fd[0] =-1;

    close( pwsdata->pws_fd[1] );
    pwsdata->pws_fd[1] =-1;

    pw_main_loop_quit(pwsdata->loop);

    pw_stream_destroy(pwsdata->stream);
    pw_main_loop_destroy(pwsdata->loop);
    pw_deinit();

    pthread_mutex_unlock( &pws_videoframelock );

    return PWS_SUCCESS;
}
/* }}} */

/** @description: Fetch Picture Type from frame data
 *  @param[in]: Frame data
 *  @return: Enum - Picture Type
 */
/* {{{ pws_GetH264PictureType() */
static u32 pws_GetH264PictureType( u8 *Framedata )
{
    PWS_PIC_TYPE enPicType = PWS_PIC_TYPE_INVALID;

    if( NULL != Framedata )
    {
#ifdef PLATFORM_RPI
        if( ( 0X27 == Framedata[4] ) || ( 0X28 == Framedata[4] ) )
	    enPicType = PWS_PIC_TYPE_IDR_FRAME;
	else if( 0X25 == Framedata[4] )
            enPicType = PWS_PIC_TYPE_I_FRAME;
        else if( 0X21 == Framedata[4] )
	    enPicType = PWS_PIC_TYPE_P_FRAME;
#endif
    }
    
    return enPicType;
}
/* }}} */
