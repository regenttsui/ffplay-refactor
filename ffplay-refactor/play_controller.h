#ifndef PLAY_CONTROLLER_H
#define PLAY_CONTROLLER_H

#include "media_state.h"
#include "video.h"
#include "read_stream.h"

double vp_duration(MediaState *ms, Frame *vp, Frame *nextvp);
double compute_target_delay(double delay, MediaState *ms);
void update_video_pts(MediaState *ms, double pts, int64_t pos, int serial);


void stream_seek(MediaState *ms, int64_t pos, int64_t rel, int seek_by_bytes);
void stream_toggle_pause(MediaState *ms);
void toggle_pause(MediaState *ms);
void toggle_mute(MediaState *ms);
void update_volume(MediaState *ms, int sign, double step);
void step_to_next_frame(MediaState *ms);
void toggle_full_screen(MediaState *ms);
void toggle_audio_display(MediaState *ms);


void video_refresh(void *opaque, double *remaining_time);
void refresh_loop_wait_event(MediaState *ms, SDL_Event *event);
void event_loop(MediaState *cur_stream);



#endif // !PLAY_CONTROLLER_H

