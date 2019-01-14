#ifndef GLOBAL_H_
#define GLOBAL_H_

#ifndef __STDC_CONSTANT_MACROS
#define __STDC_CONSTANT_MACROS
#endif
#include <SDL.h>
extern "C"
{
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
};

constexpr auto MAX_QUEUE_SIZE = (15 * 1024 * 1024);
constexpr auto MIN_FRAMES = 25;
constexpr auto EXTERNAL_CLOCK_MIN_FRAMES = 2;
constexpr auto EXTERNAL_CLOCK_MAX_FRAMES = 10;
constexpr auto SDL_AUDIO_MIN_BUFFER_SIZE = 512;
constexpr auto SDL_AUDIO_MAX_CALLBACKS_PER_SEC = 30;
constexpr auto SDL_VOLUME_STEP = (0.75);
constexpr auto AUDIO_DIFF_AVG_NB = 20;
constexpr auto REFRESH_RATE = 0.01;
constexpr auto CURSOR_HIDE_DELAY = 1000000;
constexpr auto USE_ONEPASS_SUBTITLE_RENDER = 1;
constexpr auto FF_QUIT_EVENT = (SDL_USEREVENT + 2);    //自定义的退出事件

extern unsigned sws_flags;    //图像转换默认算法
extern AVPacket flush_pkt;  //刷新用的packet
extern SDL_Window *window;
extern SDL_Renderer *renderer;
extern SDL_RendererInfo renderer_info;
extern SDL_AudioDeviceID audio_dev;

extern const char *input_filename;
extern const char *window_title;
extern int default_width;
extern int default_height;
extern int screen_width;
extern int screen_height;
extern int loop;
extern int autoexit;
extern int framedrop;
extern int show_status;
extern double rdftspeed;
extern int64_t start_time;
extern int64_t duration;
extern int64_t cursor_last_shown;
extern int cursor_hidden;
extern int exit_on_keydown;
extern int exit_on_mousedown;
extern int is_full_screen;
extern int seek_by_bytes;
extern float seek_interval;
extern int64_t audio_callback_time; 


/* 通用Frame结构，包含了解码的视音频和字幕数据 */
struct Frame
{
	AVFrame *frame;       
	AVSubtitle sub;       
	int serial;            
	double pts;           
	double duration;      
	int64_t pos;          
	int width;            
	int height;
	int format;
	AVRational sar;		  
	int uploaded;
	int flip_v;           
};

/* 音频参数，用于复制SDL中与FFmpeg兼容的参数并加上符合FFmpeg的参数 */
struct AudioParams
{
	int freq;
	int channels;
	int64_t channel_layout;
	enum AVSampleFormat fmt;
	int frame_size;
	int bytes_per_sec;
};

enum ShowMode
{
	SHOW_MODE_NONE = -1,
	SHOW_MODE_VIDEO = 0,
	SHOW_MODE_WAVES,
	SHOW_MODE_RDFT,
	SHOW_MODE_NB
};

#endif // !GLOBAL_H_

