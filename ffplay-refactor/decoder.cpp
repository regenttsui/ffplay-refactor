#include "decoder.h"

Decoder::~Decoder()
{
	av_packet_unref(&pkt);   
	avcodec_free_context(&avctx);
}

/* 启动解码 */
int Decoder::decoder_start(int(*fn)(void *), void * arg)
{
	pkt_queue->packet_queue_start();					//启动packet queue
	decoder_tid = SDL_CreateThread(fn, "decoder", arg);	//创建解码线程
	if (!decoder_tid)
	{
		av_log(NULL, AV_LOG_ERROR, "SDL_CreateThread(): %s\n", SDL_GetError());
		return AVERROR(ENOMEM);
	}
	return 0;
}

/* 中止解码 */
void Decoder::decoder_abort(FrameQueue * fq)
{
	/*分别终止packet和frame队列*/
	pkt_queue->packet_queue_abort();
	fq->frame_queue_signal();
	SDL_WaitThread(decoder_tid, NULL);  //销毁解码线程
	decoder_tid = NULL;
	pkt_queue->packet_queue_flush();	//清空packet队列
}

void Decoder::decoder_init(AVCodecContext * ctx, PacketQueue * pq, SDL_cond * empty_q_cond, int64_t st_pts, int serial)
{
	avctx = ctx;
	pkt_queue = pq;
	empty_queue_cond = empty_q_cond;
	start_pts = st_pts;
	pkt_serial = serial;
}

void Decoder::decoder_destroy()
{
	av_packet_unref(&pkt);  
	avcodec_free_context(&avctx);
}

/* 音视频、字幕解码共用的函数 */
int Decoder::decoder_decode_frame(AVFrame * frame, AVSubtitle * sub)
{
	int ret = AVERROR(EAGAIN);

	while (true)
	{
		AVPacket packet;
		if (pkt_queue->get_serial() == pkt_serial)
		{
			do {
				//强制退出
				if (pkt_queue->is_abort())
					return -1;

				//根据类型是音频还是视频区分处理
				switch (avctx->codec_type) 
				{
				case AVMEDIA_TYPE_VIDEO:
					ret = avcodec_receive_frame(avctx, frame);
					if (ret >= 0) 
					{
							frame->pts = frame->pkt_dts;
					}
					break;
				case AVMEDIA_TYPE_AUDIO:
					ret = avcodec_receive_frame(avctx, frame);
					if (ret >= 0) 
					{
						AVRational tb = { 1, frame->sample_rate };
						if (frame->pts != AV_NOPTS_VALUE)
							frame->pts = av_rescale_q(frame->pts, avctx->pkt_timebase, tb);
						else if (next_pts != AV_NOPTS_VALUE)
							frame->pts = av_rescale_q(next_pts, next_pts_tb, tb);

						if (frame->pts != AV_NOPTS_VALUE) 
						{
							next_pts = frame->pts + frame->nb_samples;
							next_pts_tb = tb;
						}
					}
					break;
				}
				//到达文件末尾，需要刷新解码器
				if (ret == AVERROR_EOF) 
				{
					finished = pkt_serial;
					avcodec_flush_buffers(avctx);
					return 0;
				}
				if (ret >= 0)
					return 1;
			} while (ret != AVERROR(EAGAIN));
		}

		do {
			//PacketQueue为空时，发送empty_queue_cond条件信号，通知读线程继续读数据
			if (pkt_queue->get_nb_packets() == 0)
				SDL_CondSignal(empty_queue_cond);

			//如果有待重发的pkt，则先取待重发的pkt
			if (packet_pending) 
			{
				av_packet_move_ref(&packet, &pkt);
				packet_pending = 0;
			}
			else 
			{
				//从队列中获取一个packet
				if (pkt_queue->packet_queue_get(&packet, 1, &pkt_serial) < 0)
					return -1;
			}
		} while (pkt_queue->get_serial() != pkt_serial);

		//如果当前packet是flush_pkt
		if (packet.data == flush_pkt.data) 
		{
			avcodec_flush_buffers(avctx);
			finished = 0;
			next_pts = start_pts;
			next_pts_tb = start_pts_tb;
		}
		else 
		{
			//如果是字幕，进行解码处理
			if (avctx->codec_type == AVMEDIA_TYPE_SUBTITLE) 
			{
				int got_frame = 0;
				ret = avcodec_decode_subtitle2(avctx, sub, &got_frame, &packet);
				if (ret < 0) 
				{
					ret = AVERROR(EAGAIN);
				}
				else 
				{
					if (got_frame && !packet.data) 
					{
						packet_pending = 1;
						av_packet_move_ref(&pkt, &packet);
					}
					ret = got_frame ? 0 : (packet.data ? AVERROR(EAGAIN) : AVERROR_EOF);
				}
			}
			//不是字幕 
			else 
			{
				//发送packet进行解码，发送失败时重新发送
				if (avcodec_send_packet(avctx, &packet) == AVERROR(EAGAIN)) 
				{
					av_log(avctx, AV_LOG_ERROR, "Receive_frame and send_packet both returned EAGAIN, which is an API violation.\n");
					packet_pending = 1;				   //标志位置1，供上面的条件使用
					av_packet_move_ref(&pkt, &packet); //将发送失败的packet重新填入d->pkt
				}
			}
			av_packet_unref(&packet);
		}
	}
}

int Decoder::is_finished()
{
	return finished;
}

int Decoder::get_pkt_serial()
{
	return pkt_serial;
}

AVCodecContext* Decoder::get_avctx()
{
	return avctx;
}

void Decoder::set_start_pts(int64_t str_pts)
{
	start_pts = str_pts;
}

void Decoder::set_start_pts_tb(AVRational str_pts_tb)
{
	start_pts_tb = str_pts_tb;
}

