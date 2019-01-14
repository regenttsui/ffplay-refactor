#include "global.h"


AVPacket flush_pkt;
SDL_Window *window;
SDL_Renderer *renderer;
SDL_RendererInfo renderer_info = { 0 };
SDL_AudioDeviceID audio_dev;

const char *input_filename;
const char *window_title;
int default_width = 640;
int default_height = 480;
int screen_width = 0;
int screen_height = 0;
int loop = 1;
int autoexit = 1;
int framedrop = -1;
int show_status = 1;
double rdftspeed = 0.02;
unsigned sws_flags = SWS_BICUBIC;    //Í¼Ïñ×ª»»Ä¬ÈÏËã·¨
int64_t start_time = AV_NOPTS_VALUE;
int64_t duration = AV_NOPTS_VALUE;
int64_t cursor_last_shown;
int cursor_hidden = 0;
int exit_on_keydown = 0;
int exit_on_mousedown = 0;
int is_full_screen = 0;
int seek_by_bytes = -1;
float seek_interval = 10;
int64_t audio_callback_time; 