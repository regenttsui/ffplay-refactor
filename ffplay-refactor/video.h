#ifndef VIDEO_H_
#define VIDEO_H_

#include "media_state.h"


int get_video_frame(MediaState *ms, AVFrame *frame);
int video_thread(void *arg);
int subtitle_thread(void *arg);
int video_open(MediaState *ms);
void video_audio_display(MediaState *ms);
void video_image_display(MediaState *ms);
void video_display(MediaState *ms);



#endif // !VIDEO_H_

