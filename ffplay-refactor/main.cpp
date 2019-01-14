#include "read_stream.h"
#include "play_controller.h"

int main(int argc, char *argv[])
{
	MediaState *ms;

	av_log_set_flags(AV_LOG_SKIP_REPEATED);

	//avdevice_register_all();
	avformat_network_init();

	av_init_packet(&flush_pkt);
	flush_pkt.data = (uint8_t *)&flush_pkt;

	input_filename = "test.mp4";

	/* 初始化SDL子系统 */
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER))
	{
		av_log(NULL, AV_LOG_FATAL, "Could not initialize SDL - %s\n", SDL_GetError());
		av_log(NULL, AV_LOG_FATAL, "(Did you set the DISPLAY variable?)\n");
		exit(1);
	}

	//设置事件的处理状态，以下两种事件将自动从事件队列中删除
	SDL_EventState(SDL_SYSWMEVENT, SDL_IGNORE);
	SDL_EventState(SDL_USEREVENT, SDL_IGNORE);

	/* 以默认宽高建立窗口 */
	window = SDL_CreateWindow("OnePlayer", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
		default_width, default_height, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
	//设置缩放的算法级别
	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
	//创建渲染器
	if (window) 
	{
		renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
		if (!renderer) 
		{
			av_log(NULL, AV_LOG_WARNING, "Failed to initialize a hardware accelerated renderer: %s\n", SDL_GetError());
			renderer = SDL_CreateRenderer(window, -1, 0);
		}
		if (renderer) 
		{
			if (!SDL_GetRendererInfo(renderer, &renderer_info))
				av_log(NULL, AV_LOG_VERBOSE, "Initialized %s renderer.\n", renderer_info.name);
		}
	}
	if (!window || !renderer || !renderer_info.num_texture_formats) 
	{
		av_log(NULL, AV_LOG_FATAL, "Failed to create window or renderer: %s", SDL_GetError());
		do_exit(NULL);
	}


	// 打开流
	ms = stream_open(input_filename);
	if (!ms) 
	{
		av_log(NULL, AV_LOG_FATAL, "Failed to initialize MediaState!\n");
		do_exit(NULL);
	}

	//进入事件循环
	event_loop(ms);

	return 0;
}


