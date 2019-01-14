#include "read_stream.h"


/* 中断回调函数，返回强制中断的请求标志 */
int decode_interrupt_cb(void * ctx)
{
	MediaState *ms = (MediaState*)ctx;
	return ms->abort_request;
}

/* 判断是否时网络流视频 */
int is_realtime(AVFormatContext * ctx)
{
	if (!strcmp(ctx->iformat->name, "rtp")
		|| !strcmp(ctx->iformat->name, "rtsp")
		|| !strcmp(ctx->iformat->name, "sdp"))
		return 1;

	if (ctx->pb && (!strncmp(ctx->url, "rtp:", 4)
		|| !strncmp(ctx->url, "udp:", 4)))
		return 1;
	return 0;
}

int stream_has_enough_packets(AVStream * st, int stream_id, PacketQueue * queue)
{
	return stream_id < 0 ||
		queue->is_abort() ||
		(st->disposition & AV_DISPOSITION_ATTACHED_PIC) ||
		queue->get_nb_packets() > MIN_FRAMES && (!queue->get_duration() || av_q2d(st->time_base) * queue->get_duration() > 1.0);
}


int is_pkt_in_play_range(AVFormatContext * ic, AVPacket * pkt)
{
	if (duration == AV_NOPTS_VALUE) 
		return 1;

	int64_t stream_ts = get_pkt_ts(pkt) - get_stream_start_time(ic, pkt->stream_index);
	double stream_ts_s = ts_as_second(stream_ts, ic, pkt->stream_index);

	double ic_ts = stream_ts_s - get_ic_start_time(ic);

	return ic_ts <= ((double)duration / 1000000);
}

/* 打开给定的具体的一个流，成功则返回0 */
int stream_component_open(MediaState *ms, int stream_index)
{
	AVFormatContext *fmt_ctx = ms->ic;
	AVCodecContext *avctx;
	AVCodec *codec;
	int sample_rate, nb_channels;
	int64_t channel_layout = 0;
	int ret = 0;

	if (stream_index < 0 || stream_index >= fmt_ctx->nb_streams)
		return -1;

	avctx = avcodec_alloc_context3(NULL);
	if (!avctx)
		return AVERROR(ENOMEM);

	
	ret = avcodec_parameters_to_context(avctx, fmt_ctx->streams[stream_index]->codecpar);
	if (ret < 0)
	{
		avcodec_free_context(&avctx);
		return ret;
	}

	
	avctx->pkt_timebase = fmt_ctx->streams[stream_index]->time_base;

	
	codec = avcodec_find_decoder(avctx->codec_id);

	
	switch (avctx->codec_type)
	{
	case AVMEDIA_TYPE_AUDIO:
		ms->last_audio_stream = stream_index;
		break;
	case AVMEDIA_TYPE_SUBTITLE:
		ms->last_subtitle_stream = stream_index;
		break;
	case AVMEDIA_TYPE_VIDEO:
		ms->last_video_stream = stream_index;
		break;
	}

	if (!codec)
	{

		av_log(NULL, AV_LOG_WARNING, "No decoder could be found for codec %s\n", avcodec_get_name(avctx->codec_id));
		ret = AVERROR(EINVAL);
		avcodec_free_context(&avctx);
		return ret;
	}

	
	if ((ret = avcodec_open2(avctx, codec, NULL)) < 0)
	{
		avcodec_free_context(&avctx);
		return ret;
	}

	ms->eof = 0;
	fmt_ctx->streams[stream_index]->discard = AVDISCARD_DEFAULT;

	
	switch (avctx->codec_type)
	{
	case AVMEDIA_TYPE_AUDIO:
		
		sample_rate = avctx->sample_rate;
		nb_channels = avctx->channels;

		
		if ((ret = audio_open(ms, channel_layout, nb_channels, sample_rate, &ms->audio_tgt)) < 0)
		{
			avcodec_free_context(&avctx);
			return ret;
		}
		ms->audio_hw_buf_size = ret;
		ms->audio_src = ms->audio_tgt;

		
		ms->audio_buf_size = 0;
		ms->audio_buf_index = 0;

		
		ms->audio_diff_avg_coef = exp(log(0.01) / AUDIO_DIFF_AVG_NB);
		ms->audio_diff_avg_count = 0;
		
		ms->audio_diff_threshold = (double)(ms->audio_hw_buf_size) / ms->audio_tgt.bytes_per_sec;

		ms->audio_stream = stream_index;
		ms->audio_st = fmt_ctx->streams[stream_index];

		ms->auddec.decoder_init(avctx, &ms->audio_pq, ms->continue_read_thread);
		if ((ms->ic->iformat->flags & (AVFMT_NOBINSEARCH | AVFMT_NOGENSEARCH | AVFMT_NO_BYTE_SEEK)) &&
			!ms->ic->iformat->read_seek)
		{
			ms->auddec.set_start_pts(ms->audio_st->start_time);
			ms->auddec.set_start_pts_tb(ms->audio_st->time_base);
		}

		if ((ret = ms->auddec.decoder_start(audio_thread, ms)) < 0)
			return ret;
		SDL_PauseAudioDevice(audio_dev, 0);
		break;
	case AVMEDIA_TYPE_VIDEO:
		ms->video_stream = stream_index;
		ms->video_st = fmt_ctx->streams[stream_index];

		ms->viddec.decoder_init(avctx, &ms->video_pq, ms->continue_read_thread);
		if ((ret = ms->viddec.decoder_start(video_thread, ms)) < 0)
			return ret;
		ms->queue_attachments_req = 1;
		break;
	case AVMEDIA_TYPE_SUBTITLE:
		ms->subtitle_stream = stream_index;
		ms->subtitle_st = fmt_ctx->streams[stream_index];

		ms->subdec.decoder_init(avctx, &ms->subtitle_pq, ms->continue_read_thread);
		if ((ret = ms->subdec.decoder_start(subtitle_thread, ms)) < 0)
			return ret;
		break;
	default:
		break;
	}

	return ret;
}

/* 关闭一个具体的流 */
void stream_component_close(MediaState * ms, int stream_index)
{
	AVFormatContext *ic = ms->ic;
	AVCodecParameters *codecpar;

	if (stream_index < 0 || stream_index >= ic->nb_streams)
		return;
	codecpar = ic->streams[stream_index]->codecpar;

	switch (codecpar->codec_type) {
	case AVMEDIA_TYPE_AUDIO:
		ms->auddec.decoder_abort(&ms->samp_fq);
		SDL_CloseAudioDevice(audio_dev);
		ms->auddec.decoder_destroy();
		swr_free(&ms->swr_ctx);
		av_freep(&ms->audio_buf1);
		ms->audio_buf1_size = 0;
		ms->audio_buf = NULL;
		break;
	case AVMEDIA_TYPE_VIDEO:
		ms->viddec.decoder_abort(&ms->pict_fq);
		ms->viddec.decoder_destroy();
		break;
	case AVMEDIA_TYPE_SUBTITLE:
		ms->subdec.decoder_abort(&ms->sub_fq);
		ms->subdec.decoder_destroy();
		break;
	default:
		break;
	}

	ic->streams[stream_index]->discard = AVDISCARD_ALL;
	switch (codecpar->codec_type) {
	case AVMEDIA_TYPE_AUDIO:
		ms->audio_st = NULL;
		ms->audio_stream = -1;
		break;
	case AVMEDIA_TYPE_VIDEO:
		ms->video_st = NULL;
		ms->video_stream = -1;
		break;
	case AVMEDIA_TYPE_SUBTITLE:
		ms->subtitle_st = NULL;
		ms->subtitle_stream = -1;
		break;
	default:
		break;
	}
}

/* 读取线程，从本地或网络读取文件 */
int read_thread(void * arg)
{
	MediaState *ms = (MediaState*)arg;
	AVFormatContext *fmt_ctx = NULL;
	int err, i, ret;
	int st_index[AVMEDIA_TYPE_NB];			   //储存流的索引的数组
	AVPacket pkt1, *pkt = &pkt1;
	int pkt_in_play_range = 0;
	SDL_mutex *wait_mutex = SDL_CreateMutex(); //创建互斥量
	int infinite_buffer = -1;
	SDL_Event event;						   //失败时使用的事件

	if (!wait_mutex)
	{
		av_log(NULL, AV_LOG_FATAL, "SDL_CreateMutex(): %s\n", SDL_GetError());
		event.type = FF_QUIT_EVENT;
		event.user.data1 = ms;
		SDL_PushEvent(&event);
		return -1;
	}

	//设置MediaState的一些初始值
	memset(st_index, -1, sizeof(st_index));
	ms->last_video_stream = ms->video_stream = -1;
	ms->last_audio_stream = ms->audio_stream = -1;
	ms->last_subtitle_stream = ms->subtitle_stream = -1;
	ms->eof = 0;

	fmt_ctx = avformat_alloc_context();
	if (!fmt_ctx)
	{
		av_log(NULL, AV_LOG_FATAL, "Could not allocate context.\n");
		event.type = FF_QUIT_EVENT;
		event.user.data1 = ms;
		SDL_PushEvent(&event);
		SDL_DestroyMutex(wait_mutex);
		return -1;
	}

	fmt_ctx->interrupt_callback.callback = decode_interrupt_cb;
	fmt_ctx->interrupt_callback.opaque = ms;

	err = avformat_open_input(&fmt_ctx, ms->filename, NULL, NULL);
	if (err < 0)
	{
		if (fmt_ctx && !ms->ic)
			avformat_close_input(&fmt_ctx);

		event.type = FF_QUIT_EVENT;
		event.user.data1 = ms;
		SDL_PushEvent(&event);
		SDL_DestroyMutex(wait_mutex);
		return -1;
	}

	ms->ic = fmt_ctx;

	err = avformat_find_stream_info(fmt_ctx, NULL);
	if (err < 0)
	{
		av_log(NULL, AV_LOG_WARNING,
			"%s: could not find codec parameters\n", ms->filename);
		if (fmt_ctx && !ms->ic)
			avformat_close_input(&fmt_ctx);

		event.type = FF_QUIT_EVENT;
		event.user.data1 = ms;
		SDL_PushEvent(&event);
		SDL_DestroyMutex(wait_mutex);
		return -1;
	}

	
	if (fmt_ctx->pb)
		fmt_ctx->pb->eof_reached = 0; 

	
	if (seek_by_bytes < 0)
		seek_by_bytes = !!(fmt_ctx->iformat->flags & AVFMT_TS_DISCONT) && strcmp("ogg", fmt_ctx->iformat->name);

	
	ms->max_frame_duration = (fmt_ctx->iformat->flags & AVFMT_TS_DISCONT) ? 10.0 : 3600.0;

	
	ms->realtime = is_realtime(fmt_ctx);

	
	if (show_status)
		av_dump_format(fmt_ctx, 0, ms->filename, 0);

	
	st_index[AVMEDIA_TYPE_VIDEO] = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_VIDEO, st_index[AVMEDIA_TYPE_VIDEO], -1, NULL, 0);
	st_index[AVMEDIA_TYPE_AUDIO] = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_AUDIO, st_index[AVMEDIA_TYPE_AUDIO],
		st_index[AVMEDIA_TYPE_VIDEO], NULL, 0);
	st_index[AVMEDIA_TYPE_SUBTITLE] = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_SUBTITLE, st_index[AVMEDIA_TYPE_SUBTITLE],
		(st_index[AVMEDIA_TYPE_AUDIO] >= 0 ? st_index[AVMEDIA_TYPE_AUDIO] : st_index[AVMEDIA_TYPE_VIDEO]), NULL, 0);

	
	ms->show_mode = SHOW_MODE_NONE;

	
	if (st_index[AVMEDIA_TYPE_VIDEO] >= 0)
	{
		AVStream *st = fmt_ctx->streams[st_index[AVMEDIA_TYPE_VIDEO]];
		AVCodecParameters *codecpar = st->codecpar;
		
		AVRational sar = av_guess_sample_aspect_ratio(fmt_ctx, st, NULL);
		
		if (codecpar->width)
			set_default_window_size(codecpar->width, codecpar->height, sar);
	}

	/* 打开对应的三个流，里面创建了解码线程 */
	if (st_index[AVMEDIA_TYPE_AUDIO] >= 0)
		stream_component_open(ms, st_index[AVMEDIA_TYPE_AUDIO]);

	ret = -1;
	if (st_index[AVMEDIA_TYPE_VIDEO] >= 0)
		ret = stream_component_open(ms, st_index[AVMEDIA_TYPE_VIDEO]);

	//如果视频打开成功，就显示视频画面，否则，显示音频对应的频谱图
	if (ms->show_mode == SHOW_MODE_NONE)
		ms->show_mode = ret >= 0 ? SHOW_MODE_VIDEO : SHOW_MODE_RDFT;

	if (st_index[AVMEDIA_TYPE_SUBTITLE] >= 0)
		stream_component_open(ms, st_index[AVMEDIA_TYPE_SUBTITLE]);

	//打开失败
	if (ms->video_stream < 0 && ms->audio_stream < 0)
	{
		av_log(NULL, AV_LOG_FATAL, "Failed to open file '%s' or configure filtergraph\n",
			ms->filename);
		if (fmt_ctx && !ms->ic)
			avformat_close_input(&fmt_ctx);

		event.type = FF_QUIT_EVENT;
		event.user.data1 = ms;
		SDL_PushEvent(&event);
		SDL_DestroyMutex(wait_mutex);
		return -1;
	}

	//在文件是实时协议时，用于后面控制缓冲区大小
	if (ms->realtime)
		infinite_buffer = 1;

	while (true)
	{
		//退出
		if (ms->abort_request)
			break;

		//暂停/播放，如果标志位相对上一次有改变，则进行响应
		if (ms->paused != ms->last_paused)
		{
			ms->last_paused = ms->paused; //在改变前先将当前标志位保存
			//暂停和播放均调用内置函数即可
			if (ms->paused)
				ms->read_pause_return = av_read_pause(fmt_ctx);
			else
				av_read_play(fmt_ctx);
		}

		if (ms->seek_req) 
		{
			int64_t seek_target = ms->seek_pos;
			int64_t seek_min = ms->seek_rel > 0 ? seek_target - ms->seek_rel + 2 : INT64_MIN;
			int64_t seek_max = ms->seek_rel < 0 ? seek_target - ms->seek_rel - 2 : INT64_MAX;

			ret = avformat_seek_file(ms->ic, -1, seek_min, seek_target, seek_max, ms->seek_flags);

			if (ret < 0)
			{
				av_log(NULL, AV_LOG_ERROR, "%s: error while seeking\n", ms->ic->url);
			}
			else
			{
				if (ms->audio_stream >= 0)
				{
					ms->audio_pq.packet_queue_flush();
					ms->audio_pq.packet_queue_put(&flush_pkt);
				}
				if (ms->subtitle_stream >= 0)
				{
					ms->subtitle_pq.packet_queue_flush();
					ms->subtitle_pq.packet_queue_put(&flush_pkt);
				}
				if (ms->video_stream >= 0)
				{
					ms->video_pq.packet_queue_flush();
					ms->video_pq.packet_queue_put(&flush_pkt);
				}
				if (ms->seek_flags & AVSEEK_FLAG_BYTE)
				{
					ms->extclk.set_clock(NAN, 0);
				}
				else
				{
					ms->extclk.set_clock(seek_target / (double)AV_TIME_BASE, 0);
				}
			}
			ms->seek_req = 0;  
			ms->queue_attachments_req = 1;
			ms->eof = 0;

			
			if (ms->paused)
				step_to_next_frame(ms);
		}

		
		if (ms->queue_attachments_req)
		{
			if (ms->video_st && ms->video_st->disposition & AV_DISPOSITION_ATTACHED_PIC)
			{
				AVPacket copy = { 0 };
				if ((ret = av_packet_ref(&copy, &ms->video_st->attached_pic)) < 0)
				{
					if (fmt_ctx && !ms->ic)
						avformat_close_input(&fmt_ctx);

					if (ret != 0)
					{
						SDL_Event event;

						event.type = FF_QUIT_EVENT;
						event.user.data1 = ms;
						SDL_PushEvent(&event);
					}
					SDL_DestroyMutex(wait_mutex);
					return -1;
				}
				ms->video_pq.packet_queue_put(&copy);
				ms->video_pq.packet_queue_put_nullpacket(ms->video_stream);
			}
			ms->queue_attachments_req = 0;
		}

		if (infinite_buffer < 1 &&
			(ms->audio_pq.get_size() + ms->video_pq.get_size() + ms->subtitle_pq.get_size() > MAX_QUEUE_SIZE
				|| (stream_has_enough_packets(ms->audio_st, ms->audio_stream, &ms->audio_pq) &&
					stream_has_enough_packets(ms->video_st, ms->video_stream, &ms->video_pq) &&
					stream_has_enough_packets(ms->subtitle_st, ms->subtitle_stream, &ms->subtitle_pq))))
		{
			
			SDL_LockMutex(wait_mutex);
			SDL_CondWaitTimeout(ms->continue_read_thread, wait_mutex, 10);
			SDL_UnlockMutex(wait_mutex);
			continue;
		}
		

		if (!ms->paused &&
			(!ms->audio_st || (ms->auddec.is_finished() == ms->audio_pq.get_serial() && ms->samp_fq.frame_queue_nb_remaining() == 0)) &&
			(!ms->video_st || (ms->viddec.is_finished() == ms->video_pq.get_serial() && ms->pict_fq.frame_queue_nb_remaining() == 0)))
		{
			
			if (allow_loop())
			{
				stream_seek(ms, start_time != AV_NOPTS_VALUE ? start_time : 0, 0, 0);
			}
			 
			else if (autoexit)
			{
				ret = AVERROR_EOF;
				if (fmt_ctx && !ms->ic)
					avformat_close_input(&fmt_ctx);

				if (ret != 0)
				{
					SDL_Event event;

					event.type = FF_QUIT_EVENT;
					event.user.data1 = ms;
					SDL_PushEvent(&event);
				}
				SDL_DestroyMutex(wait_mutex);
				return -1;
			}
		}

		
		ret = av_read_frame(fmt_ctx, pkt);

		
		if (ret < 0)
		{
			
			if ((ret == AVERROR_EOF || avio_feof(fmt_ctx->pb)) && !ms->eof)
			{
				if (ms->video_stream >= 0)
					ms->video_pq.packet_queue_put_nullpacket(ms->video_stream);
				if (ms->audio_stream >= 0)
					ms->audio_pq.packet_queue_put_nullpacket(ms->audio_stream);
				if (ms->subtitle_stream >= 0)
					ms->subtitle_pq.packet_queue_put_nullpacket(ms->subtitle_stream);
				ms->eof = 1;
			}
			
			if (fmt_ctx->pb && fmt_ctx->pb->error)
				break;
			
			SDL_LockMutex(wait_mutex);
			SDL_CondWaitTimeout(ms->continue_read_thread, wait_mutex, 10);
			SDL_UnlockMutex(wait_mutex);
			continue;
		}
		else
		{
			ms->eof = 0;
		}

		pkt_in_play_range = is_pkt_in_play_range(fmt_ctx, pkt);

		
		if (pkt->stream_index == ms->audio_stream && pkt_in_play_range)
		{
			ms->audio_pq.packet_queue_put(pkt);
		}
		else if (pkt->stream_index == ms->video_stream && pkt_in_play_range
			&& !(ms->video_st->disposition & AV_DISPOSITION_ATTACHED_PIC))
		{
			ms->video_pq.packet_queue_put(pkt);
		}
		else if (pkt->stream_index == ms->subtitle_stream && pkt_in_play_range)
		{
			ms->subtitle_pq.packet_queue_put(pkt);
		}
		else
		{
			av_packet_unref(pkt);
		}
	}

	ret = 0;
	if (fmt_ctx && !ms->ic)
		avformat_close_input(&fmt_ctx);

	if (ret != 0)
	{
		SDL_Event event;

		event.type = FF_QUIT_EVENT;
		event.user.data1 = ms;
		SDL_PushEvent(&event);
	}
	SDL_DestroyMutex(wait_mutex);
	return 0;
}

/* 关闭流 */
void stream_close(MediaState * ms)
{
	/* 销毁读取线程 */
	ms->abort_request = 1;
	SDL_WaitThread(ms->read_tid, NULL);

	/* 关闭每一个流 */
	if (ms->audio_stream >= 0)
		stream_component_close(ms, ms->audio_stream);
	if (ms->video_stream >= 0)
		stream_component_close(ms, ms->video_stream);
	if (ms->subtitle_stream >= 0)
		stream_component_close(ms, ms->subtitle_stream);

	avformat_close_input(&ms->ic);

	/* 显式销毁所有packet queue和frame queue */
	ms->video_pq.packet_queue_destroy();
	ms->audio_pq.packet_queue_destroy();
	ms->subtitle_pq.packet_queue_destroy();
	ms->pict_fq.frame_queue_destroy();
	ms->samp_fq.frame_queue_destroy();
	ms->sub_fq.frame_queue_destroy();

	SDL_DestroyCond(ms->continue_read_thread);
	sws_freeContext(ms->img_convert_ctx);
	sws_freeContext(ms->sub_convert_ctx);
	av_free(ms->filename);
	if (ms->vis_texture)
		SDL_DestroyTexture(ms->vis_texture);
	if (ms->vid_texture)
		SDL_DestroyTexture(ms->vid_texture);
	if (ms->sub_texture)
		SDL_DestroyTexture(ms->sub_texture);
	av_free(ms);
}


/* 打开流。主要功能是分配全局总控数据结构，初始化相关参数，启动文件解析线程。*/
MediaState *stream_open(const char * filename)
{
	MediaState *ms;
	ms = (MediaState *)av_mallocz(sizeof(MediaState));
	if (!ms)
		return NULL;

	ms->filename = av_strdup(filename); //拷贝文件名字符串
	if (!ms->filename)
	{
		stream_close(ms);
		return NULL;
	}


	ms->video_pq.packet_queue_init();
	ms->subtitle_pq.packet_queue_init();
	ms->audio_pq.packet_queue_init();
	if (!ms->video_pq.is_construct() || !ms->subtitle_pq.is_construct() || !ms->audio_pq.is_construct())
	{
		stream_close(ms);
		return NULL;
	}

	/* 初始化3个frame队列 */
	ms->pict_fq.frame_queue_init(&ms->video_pq, VIDEO_PICTURE_QUEUE_SIZE, 1);
	ms->sub_fq.frame_queue_init(&ms->subtitle_pq, SUBPICTURE_QUEUE_SIZE, 0);
	ms->samp_fq.frame_queue_init(&ms->audio_pq, SAMPLE_QUEUE_SIZE, 1);

	/* 初始化几个时钟 */
	int video_serial = ms->video_pq.get_serial(), audio_serial = ms->audio_pq.get_serial();
	ms->vidclk = Clock(&video_serial);
	ms->audclk = Clock(&audio_serial);
	ms->extclk = Clock(ms->extclk.get_serial());
	ms->audio_clock_serial = -1;

	/* 初始音量值 */
	int startup_volume = 100;
	startup_volume = av_clip(startup_volume, 0, 100);
	startup_volume = av_clip(SDL_MIX_MAXVOLUME * startup_volume / 100, 0, SDL_MIX_MAXVOLUME);
	ms->audio_volume = startup_volume;		  //设置初始音量
	ms->muted = 0;							  //不静音
	ms->av_sync_type = AV_SYNC_AUDIO_MASTER;  //默认视频同步于音频

	/* 给读取线程创建条件变量 */
	if (!(ms->continue_read_thread = SDL_CreateCond()))
	{
		av_log(NULL, AV_LOG_FATAL, "SDL_CreateCond(): %s\n", SDL_GetError());
		stream_close(ms);
		return NULL;
	}
	/* 创建读取线程，传递MediaState给read_thread并执行 */
	ms->read_tid = SDL_CreateThread(read_thread, "read_thread", ms);
	if (!ms->read_tid)
	{
		av_log(NULL, AV_LOG_FATAL, "SDL_CreateThread(): %s\n", SDL_GetError());
		stream_close(ms);
		return NULL;
	}

	return ms;
}

/* 退出时执行的函数 */
void do_exit(MediaState * ms)
{
	if (ms)
	{
		stream_close(ms);
	}
	if (renderer)
		SDL_DestroyRenderer(renderer);
	if (window)
		SDL_DestroyWindow(window);

	avformat_network_deinit();
	if (show_status)
		printf("\n");
	SDL_Quit();
	av_log(NULL, AV_LOG_QUIET, "%s", "");
	exit(0);
}
