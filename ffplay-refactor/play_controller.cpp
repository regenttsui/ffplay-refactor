#include "play_controller.h"

/* ����һ֡�ĳ���ʱ�� */
double vp_duration(MediaState *ms, Frame * vp, Frame * nextvp)
{
	if (vp->serial == nextvp->serial)
	{
		double duration = nextvp->pts - vp->pts;
		if (isnan(duration) || duration <= 0 || duration > ms->max_frame_duration)
			return vp->duration;
		else
			return duration;
	}
	else
	{
		return 0.0;
	}
}

/* ��Ƶ֡��ʾʱ������ */
double compute_target_delay(double delay, MediaState *ms)
{
	double sync_threshold, diff = 0;

	if (ms->get_master_sync_type() != AV_SYNC_VIDEO_MASTER)
	{
		diff = ms->vidclk.get_clock() - ms->get_master_clock(); //������Ƶʱ�Ӻ���ʱ�ӵĲ�ֵ

		sync_threshold = FFMAX(AV_SYNC_THRESHOLD_MIN, FFMIN(AV_SYNC_THRESHOLD_MAX, delay));
		if (!isnan(diff) && fabs(diff) < ms->max_frame_duration)
		{
			if (diff <= -sync_threshold)
				delay = FFMAX(0, delay + diff);
			else if (diff >= sync_threshold && delay > AV_SYNC_FRAMEDUP_THRESHOLD)
				delay = delay + diff;
			else if (diff >= sync_threshold)
				delay = 2 * delay;
		}
	}

	av_log(NULL, AV_LOG_TRACE, "video: delay=%0.3f A-V=%f\n", delay, -diff);

	return delay;
}

void update_video_pts(MediaState * ms, double pts, int64_t pos, int serial)
{
	ms->vidclk.set_clock(pts, serial);
	ms->sync_clock_to_slave(&ms->extclk, &ms->vidclk);
}

void stream_seek(MediaState * ms, int64_t pos, int64_t rel, int seek_by_bytes)
{
	if (!ms->seek_req)
	{
		ms->seek_pos = pos; 
		ms->seek_rel = rel; 
		ms->seek_flags &= ~AVSEEK_FLAG_BYTE; 
		if (seek_by_bytes)
			ms->seek_flags |= AVSEEK_FLAG_BYTE;
		ms->seek_req = 1; 
		SDL_CondSignal(ms->continue_read_thread);
	}
}

/* pause or resume the video ��ͣ/������Ƶ */
void stream_toggle_pause(MediaState * ms)
{
	//�����ǰ״̬������ͣ������������벥��״̬����Ҫ����vidclk(���ű���ͣ����Ҫ����)
	if (ms->paused)
	{
		ms->frame_timer += av_gettime_relative() / 1000000.0 - ms->vidclk.get_last_updated();
		if (ms->read_pause_return != AVERROR(ENOSYS))
		{
			ms->vidclk.set_paused(0); //������Ƶʱ��
		}
		//������Ƶʱ��
		ms->vidclk.set_clock(ms->vidclk.get_clock(), *ms->vidclk.get_serial());
	}
	//������ͣ���ǲ��ţ��������ⲿʱ��
	ms->extclk.set_clock(ms->extclk.get_clock(), *ms->extclk.get_serial());
	//��ת״̬
	ms->paused = !ms->paused;
	ms->audclk.set_paused(!ms->paused);
	ms->vidclk.set_paused(!ms->paused);
	ms->extclk.set_paused(!ms->paused);
}

/* ��ͣ/���� */
void toggle_pause(MediaState * ms)
{
	stream_toggle_pause(ms);
	ms->step = 0;
}

/* ����/�Ǿ��� */
void toggle_mute(MediaState * ms)
{
	ms->muted = !ms->muted;
}

/* �ı����� */
void update_volume(MediaState * ms, int sign, double step)
{
	double volume_level = ms->audio_volume ? (20 * log(ms->audio_volume / (double)SDL_MIX_MAXVOLUME) / log(10)) : -1000.0;
	int new_volume = lrint(SDL_MIX_MAXVOLUME * pow(10.0, (volume_level + sign * step) / 20.0));
	ms->audio_volume = av_clip(ms->audio_volume == new_volume ? (ms->audio_volume + sign) : new_volume, 0, SDL_MIX_MAXVOLUME);
}

void step_to_next_frame(MediaState * ms)
{
	if (ms->paused)
		stream_toggle_pause(ms);
	ms->step = 1; 
}

/* ȫ��/��ȫ���л� */
void toggle_full_screen(MediaState * ms)
{
	is_full_screen = !is_full_screen;
	SDL_SetWindowFullscreen(window, is_full_screen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
}

/* �л���ʾģʽ */
void toggle_audio_display(MediaState * ms)
{
	//���õ���ǰ��ʾģʽ
	int next = ms->show_mode;

	//�л�����һ�ַ��ϵ�ǰý���ļ���ģʽ
	do
	{
		next = (next + 1) % SHOW_MODE_NB;
	} while (next != ms->show_mode && (next == SHOW_MODE_VIDEO && !ms->video_st || next != SHOW_MODE_VIDEO && !ms->audio_st));

	//��������ҵ�����ģʽ����ǿ��ˢ��Ϊ�µ�ģʽ�����򲻱�
	if (ms->show_mode != next)
	{
		ms->force_refresh = 1;
		ms->show_mode = next;
	}
}

/* ��ʾÿһ֡�Ĺؼ�������������Ƶ����Ļ����ʾ */
void video_refresh(void * opaque, double * remaining_time)
{
	MediaState *ms = (MediaState*)opaque;
	double time;

	Frame *sp, *sp2;

	if (ms->show_mode != SHOW_MODE_VIDEO && ms->audio_st)
	{
		time = av_gettime_relative() / 1000000.0;
		if (ms->force_refresh || ms->last_vis_time + rdftspeed < time)
		{
			video_display(ms);
			ms->last_vis_time = time;
		}
		*remaining_time = FFMIN(*remaining_time, ms->last_vis_time + rdftspeed - time);
	}

	if (ms->video_st)
	{
		do {
			if (ms->pict_fq.frame_queue_nb_remaining() == 0)
			{
			}
			else
			{
				double last_duration, duration, delay;
				
				Frame *vp, *lastvp;
				
				lastvp = ms->pict_fq.frame_queue_peek_last();
				vp = ms->pict_fq.frame_queue_peek();

				if (vp->serial != ms->video_pq.get_serial())
				{
					ms->pict_fq.frame_queue_next();
					continue;
				}

				if (lastvp->serial != vp->serial)
					ms->frame_timer = av_gettime_relative() / 1000000.0;
				do {
					if (ms->paused)
						break;

					last_duration = vp_duration(ms, lastvp, vp);
					delay = compute_target_delay(last_duration, ms); 

					time = av_gettime_relative() / 1000000.0;
					if (time < ms->frame_timer + delay)
					{
						*remaining_time = FFMIN(ms->frame_timer + delay - time, *remaining_time); 
						continue;
					}

					ms->frame_timer += delay;

					if (delay > 0 && time - ms->frame_timer > AV_SYNC_THRESHOLD_MAX)
						ms->frame_timer = time;

					SDL_LockMutex(ms->pict_fq.get_mutex());
					if (!isnan(vp->pts))
						update_video_pts(ms, vp->pts, vp->pos, vp->serial); 
					SDL_UnlockMutex(ms->pict_fq.get_mutex());

					if (ms->pict_fq.frame_queue_nb_remaining() > 1)
					{
						Frame *nextvp = ms->pict_fq.frame_queue_peek_next();
						duration = vp_duration(ms, vp, nextvp);
						if (!ms->step && (framedrop > 0 || (framedrop && ms->get_master_sync_type() != AV_SYNC_VIDEO_MASTER)) &&
							time > ms->frame_timer + duration)
						{
							ms->frame_drops_late++;
							ms->pict_fq.frame_queue_next();
							continue;
						}
					}

					if (ms->subtitle_st)
					{
						while (ms->sub_fq.frame_queue_nb_remaining() > 0)
						{
							sp = ms->sub_fq.frame_queue_peek();

							if (ms->sub_fq.frame_queue_nb_remaining() > 1)
								sp2 = ms->sub_fq.frame_queue_peek_next();
							else
								sp2 = NULL;

							if (sp->serial != ms->subtitle_pq.get_serial()
								|| (ms->vidclk.get_pts() > (sp->pts + ((float)sp->sub.end_display_time / 1000)))
								|| (sp2 && ms->vidclk.get_pts() > (sp2->pts + ((float)sp2->sub.start_display_time / 1000))))
							{
								if (sp->uploaded)
								{
									int i;
									for (i = 0; i < sp->sub.num_rects; i++)
									{
										AVSubtitleRect *sub_rect = sp->sub.rects[i]; 
										uint8_t *pixels;
										int pitch, j;

										if (!SDL_LockTexture(ms->sub_texture, (SDL_Rect *)sub_rect, (void **)&pixels, &pitch))
										{
											for (j = 0; j < sub_rect->h; j++, pixels += pitch)
												memset(pixels, 0, sub_rect->w << 2);
											SDL_UnlockTexture(ms->sub_texture);
										}
									}
								}
								ms->sub_fq.frame_queue_next();
							}
							else
							{
								break;
							}
						}
					}

					ms->pict_fq.frame_queue_next();
					ms->force_refresh = 1;

					if (ms->step && !ms->paused)
						stream_toggle_pause(ms);
				} while (0);
			}
			/* ��ʾͼƬ/��Ƶ/��Ļ */
			if (ms->force_refresh && ms->show_mode == SHOW_MODE_VIDEO && ms->pict_fq.get_rindex_shown())
				video_display(ms);
		} while (0);
	}
	ms->force_refresh = 0;
	//��ʾһЩ��Ϣ
	if (show_status)
	{
		static int64_t last_time;
		int64_t cur_time;
		int aqsize, vqsize, sqsize;
		double av_diff;

		cur_time = av_gettime_relative();
		if (!last_time || (cur_time - last_time) >= 30000)
		{
			aqsize = 0;
			vqsize = 0;
			sqsize = 0;
			if (ms->audio_st)
				aqsize = ms->audio_pq.get_size();
			if (ms->video_st)
				vqsize = ms->video_pq.get_size();
			if (ms->subtitle_st)
				sqsize = ms->subtitle_pq.get_size();
			av_diff = 0;
			if (ms->audio_st && ms->video_st)
				av_diff = ms->audclk.get_clock() - ms->vidclk.get_clock();
			else if (ms->video_st)
				av_diff = ms->get_master_clock() - ms->vidclk.get_clock();
			else if (ms->audio_st)
				av_diff = ms->get_master_clock() - ms->audclk.get_clock();
			av_log(NULL, AV_LOG_INFO,
				"%7.2f %s:%7.3f fd=%4d aq=%5dKB vq=%5dKB sq=%5dB f=%" PRId64 "/%" PRId64 "   \r",
				ms->get_master_clock(),
				(ms->audio_st && ms->video_st) ? "A-V" : (ms->video_st ? "M-V" : (ms->audio_st ? "M-A" : "   ")),
				av_diff,
				ms->frame_drops_early + ms->frame_drops_late,
				aqsize / 1024,
				vqsize / 1024,
				sqsize,
				ms->video_st ? ms->viddec.get_avctx()->pts_correction_num_faulty_dts : 0,
				ms->video_st ? ms->viddec.get_avctx()->pts_correction_num_faulty_pts : 0);
			fflush(stdout);
			last_time = cur_time;
		}
	}
}

/* ѭ��ˢ�»��棬ͬʱ����Ƿ����¼� */
void refresh_loop_wait_event(MediaState * ms, SDL_Event * event)
{
	double remaining_time = 0.0;
	SDL_PumpEvents();
	while (!SDL_PeepEvents(event, 1, SDL_GETEVENT, SDL_FIRSTEVENT, SDL_LASTEVENT))
	{
		if (!cursor_hidden && av_gettime_relative() - cursor_last_shown > CURSOR_HIDE_DELAY)
		{
			SDL_ShowCursor(0);
			cursor_hidden = 1;
		}
				if (remaining_time > 0.0)
			av_usleep((int64_t)(remaining_time * 1000000.0));
		remaining_time = REFRESH_RATE;
		if (ms->show_mode != SHOW_MODE_NONE && (!ms->paused || ms->force_refresh))
			video_refresh(ms, &remaining_time);
		SDL_PumpEvents();
	}
}

/* �����¼���ѭ�� */
void event_loop(MediaState * cur_stream)
{
	SDL_Event event;
	double incr, pos, frac;

	while (true)
	{
		double x;
		refresh_loop_wait_event(cur_stream, &event);
		//��Ӧ�����¼�
		switch (event.type)
		{
		case SDL_KEYDOWN:
			if (exit_on_keydown || event.key.keysym.sym == SDLK_ESCAPE || event.key.keysym.sym == SDLK_q) 
			{
				do_exit(cur_stream);
				break;
			}
			if (!cur_stream->width)
				continue;
			switch (event.key.keysym.sym) 
			{
				//ȫ��/��ȫ��
			case SDLK_f:
				toggle_full_screen(cur_stream);
				cur_stream->force_refresh = 1;
				break;
				//��ͣ/����
			case SDLK_p:
			case SDLK_SPACE:
				toggle_pause(cur_stream);
				break;
				//����/�Ǿ���
			case SDLK_m:
				toggle_mute(cur_stream);
				break;
				//�˺ź�0��������
			case SDLK_KP_MULTIPLY:
			case SDLK_0:
				update_volume(cur_stream, 1, SDL_VOLUME_STEP);
				break;
				//���ź�9��������
			case SDLK_KP_DIVIDE:
			case SDLK_9:
				update_volume(cur_stream, -1, SDL_VOLUME_STEP);
				break;
				//��֡���Ż���ͣʱ��seek
			case SDLK_s: 
				step_to_next_frame(cur_stream);
				break;
				//�л���ʾģʽ
			case SDLK_w:
				toggle_audio_display(cur_stream);
				break;
				//�̿���
			case SDLK_LEFT:
				incr = seek_interval ? -seek_interval : -10.0; //����ʱ������10��
				goto do_seek;
				//�̿��
			case SDLK_RIGHT:
				incr = seek_interval ? seek_interval : 10.0;
				goto do_seek;
				//�����
			case SDLK_UP:
				incr = 60.0;
				goto do_seek;
				//������
			case SDLK_DOWN:
				incr = -60.0;
			do_seek:
				if (seek_by_bytes) 
				{
					pos = -1;
					if (pos < 0 && cur_stream->video_stream >= 0)
						pos = cur_stream->pict_fq.frame_queue_last_pos();
					if (pos < 0 && cur_stream->audio_stream >= 0)
						pos = cur_stream->pict_fq.frame_queue_last_pos();
					if (pos < 0)
						pos = avio_tell(cur_stream->ic->pb);
					if (cur_stream->ic->bit_rate)
						incr *= cur_stream->ic->bit_rate / 8.0;
					else
						incr *= 180000.0;
					pos += incr;
					stream_seek(cur_stream, pos, incr, 1);
				}
				else
				{
					pos = cur_stream->get_master_clock();
					if (isnan(pos))
						pos = (double)cur_stream->seek_pos / AV_TIME_BASE; 
					pos += incr;
					if (cur_stream->ic->start_time != AV_NOPTS_VALUE && pos < cur_stream->ic->start_time / (double)AV_TIME_BASE)
						pos = cur_stream->ic->start_time / (double)AV_TIME_BASE;
					stream_seek(cur_stream, (int64_t)(pos * AV_TIME_BASE), (int64_t)(incr * AV_TIME_BASE), 0);
				}
				break;
			default:
				break;
			}
			break;
			//�������
		case SDL_MOUSEBUTTONDOWN:
			if (exit_on_mousedown) 
			{
				do_exit(cur_stream);
				break;
			}
			//����ǰ������
			if (event.button.button == SDL_BUTTON_LEFT) 
			{
				static int64_t last_mouse_left_click = 0; 
				if (av_gettime_relative() - last_mouse_left_click <= 500000) 
				{
					toggle_full_screen(cur_stream);
					cur_stream->force_refresh = 1;
					last_mouse_left_click = 0;    
				}
				else 
				{
					last_mouse_left_click = av_gettime_relative(); //����ǲ���˫��������Ϊ��ǰʱ��
				}
			}
			//����ƶ�
		case SDL_MOUSEMOTION:
			//���������������صģ���ô����Ϊ�ɼ��������ñ�־λ
			if (cursor_hidden) 
			{
				SDL_ShowCursor(1);
				cursor_hidden = 0;
			}
			//��¼���տ�ʼ��ʾ��ʱ��
			cursor_last_shown = av_gettime_relative();
			if (event.type == SDL_MOUSEBUTTONDOWN) 
			{
				if (event.button.button != SDL_BUTTON_RIGHT)
					break;
				x = event.button.x;  
			}
			else 
			{
				if (!(event.motion.state & SDL_BUTTON_RMASK))
					break;
				x = event.motion.x;
			}
			if (seek_by_bytes || cur_stream->ic->duration <= 0) 
			{
				uint64_t size = avio_size(cur_stream->ic->pb);
				stream_seek(cur_stream, size*x / cur_stream->width, 0, 1);
			}
			//Ĭ��
			else 
			{
				int64_t ts;
				int ns, hh, mm, ss;
				int tns, thh, tmm, tss;
				tns = cur_stream->ic->duration / 1000000LL; //long long����������Ƶ��ʱ��ת��Ϊ��
				//����3�佫��ʱ�����ΪxСʱx����x��
				thh = tns / 3600; //Сʱ
				tmm = (tns % 3600) / 60; //����
				tss = (tns % 60); //��
				frac = x / cur_stream->width; //���λ������Ƶ��ǰ��ȵ����λ��
				ns = frac * tns;  //�������λ�ö�Ӧ����Ƶ�����ʱ��
				//����ת��ΪxСʱx����x�룬������ʾseek���ʱ��
				hh = ns / 3600;
				mm = (ns % 3600) / 60;
				ss = (ns % 60);
				av_log(NULL, AV_LOG_INFO,
					"Seek to %2.0f%% (%2d:%02d:%02d) of total duration (%2d:%02d:%02d)       \n", frac * 100,
					hh, mm, ss, thh, tmm, tss);
				//��seek���ʱ��ת����΢�룬�Խ���seek����
				ts = frac * cur_stream->ic->duration;
				if (cur_stream->ic->start_time != AV_NOPTS_VALUE)
					ts += cur_stream->ic->start_time; //������Ƶ����ʼʱ��
				stream_seek(cur_stream, ts, 0, 0); //����seek
			}
			break;
			//�����¼�
		case SDL_WINDOWEVENT:
			switch (event.window.event) 
			{
				//���ڴ�С��ֱ�Ӹ��������ݼ���
			case SDL_WINDOWEVENT_RESIZED:
				screen_width = cur_stream->width = event.window.data1;
				screen_height = cur_stream->height = event.window.data2;
				if (cur_stream->vis_texture) {
					SDL_DestroyTexture(cur_stream->vis_texture);
					cur_stream->vis_texture = NULL;
				}
				//���»��ƴ���
			case SDL_WINDOWEVENT_EXPOSED:
				cur_stream->force_refresh = 1;
			}
			break;
			//SDL�˳����Զ����˳�
		case SDL_QUIT:
		case FF_QUIT_EVENT: //�Զ����¼������ڳ���ʱ�������˳�
			do_exit(cur_stream);
			break;
		default:
			break;
		}
	}
}
