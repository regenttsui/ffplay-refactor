#ifndef MEDIA_STATE_H_
#define MEDIA_STATE_H_

#include "decoder.h"
#include "clock.h"
#include "sdl_utility.h"

constexpr auto SAMPLE_ARRAY_SIZE = (8 * 65536);

//总控数据结构，把其他核心数据结构整合在一起
struct MediaState
{
	int queue_picture(AVFrame *src_frame, double pts, double duration, int64_t pos, int serial);
	int get_master_sync_type();
	double get_master_clock();
	void sync_clock_to_slave(Clock *c, Clock *slave);

	SDL_Thread *read_tid;  
	int abort_request;      
	int force_refresh;      
	int paused;             
	int last_paused;        
	int queue_attachments_req;  
	int seek_req;           
	int seek_flags;         
	int64_t seek_pos;       
	int64_t seek_rel;       
	int read_pause_return;
	AVFormatContext *ic;    
	int realtime;           

	Clock audclk;           
	Clock vidclk;           
	Clock extclk;           

	FrameQueue pict_fq;       
	FrameQueue sub_fq;       
	FrameQueue samp_fq;       

	Decoder auddec;         
	Decoder viddec;         
	Decoder subdec;         

	int av_sync_type;       
	double audio_clock;     
	int audio_clock_serial; 
	double audio_diff_cum; 
	double audio_diff_avg_coef;
	double audio_diff_threshold;
	int audio_diff_avg_count;
	int audio_stream;       
	AVStream *audio_st;     
	PacketQueue audio_pq;     
	int audio_hw_buf_size;   
	uint8_t *audio_buf;     
	uint8_t *audio_buf1;    
	unsigned int audio_buf_size; 
	unsigned int audio_buf1_size;
	int audio_buf_index;    
	int audio_write_buf_size;   
	int audio_volume;       
	int muted;              
	struct AudioParams audio_src;   
	struct AudioParams audio_tgt;   
	struct SwrContext *swr_ctx;     
	int frame_drops_early;
	int frame_drops_late;
	
	int show_mode;
	int16_t sample_array[SAMPLE_ARRAY_SIZE];    
	int sample_array_index; 
	int last_i_start;
	int xpos;
	double last_vis_time;
	SDL_Texture *vis_texture;   
	SDL_Texture *sub_texture;   
	SDL_Texture *vid_texture;   

	int subtitle_stream;        
	AVStream *subtitle_st;      
	PacketQueue subtitle_pq;      

	double frame_timer;         
	double frame_last_returned_time;    
	double frame_last_filter_delay;     
	int video_stream;       
	AVStream *video_st;     
	PacketQueue video_pq;     
	double max_frame_duration;      
	struct SwsContext *img_convert_ctx; 
	struct SwsContext *sub_convert_ctx; 
	int eof;             

	char *filename;     
	int width, height, xleft, ytop;     
	int step;           
	
	int last_video_stream, last_audio_stream, last_subtitle_stream;

	SDL_cond *continue_read_thread; 
};


#endif // !MEDIA_STATE_H_

