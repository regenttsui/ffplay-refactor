#ifndef AUDIO_H_
#define AUDIO_H_

#ifndef __STDC_CONSTANT_MACROS
#define __STDC_CONSTANT_MACROS
#endif
extern "C"
{
#include <libswresample/swresample.h>
};
#include "media_state.h"


int audio_thread(void *arg);
int audio_decode_frame(MediaState *ms);
void update_sample_display(MediaState *ms, short *samples, int samples_size);
void sdl_audio_callback(void *opaque, Uint8 *stream, int len);
int audio_open(void *opaque, int64_t wanted_channel_layout, int wanted_nb_channels,
	int wanted_sample_rate, struct AudioParams *audio_hw_params);


#endif // !AUDIO_H_

