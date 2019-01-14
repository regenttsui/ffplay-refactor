#include "media_state.h"

/* ���FrameQueue�Ƿ�ɶ���Ȼ���Frame����ر������и�ֵ��д����� */
int MediaState::queue_picture(AVFrame * src_frame, double pts, double duration, int64_t pos, int serial)
{
	Frame *vp;

	//ȡFrameQueue�ĵ�ǰ��д�Ľڵ㸳��vp
	if (!(vp = pict_fq.frame_queue_peek_writable()))
		return -1;

	//��������õ�֡��������ݸ��ڵ㱣��
	vp->sar = src_frame->sample_aspect_ratio;
	vp->uploaded = 0;

	vp->width = src_frame->width;
	vp->height = src_frame->height;
	vp->format = src_frame->format;

	vp->pts = pts;
	vp->duration = duration;
	vp->pos = pos;
	vp->serial = serial;

	set_default_window_size(vp->width, vp->height, vp->sar);

	av_frame_move_ref(vp->frame, src_frame);

	//pushд�õĽڵ㵽����
	pict_fq.frame_queue_push();
	return 0;

}

/* ��ȡ��ʱ�ӵ����� */
int MediaState::get_master_sync_type()
{
	if (av_sync_type == AV_SYNC_VIDEO_MASTER) 
	{
		if (video_st)
			return AV_SYNC_VIDEO_MASTER;
		else
			return AV_SYNC_AUDIO_MASTER;
	}
	else if (av_sync_type == AV_SYNC_AUDIO_MASTER) 
	{
		if (audio_st)
			return AV_SYNC_AUDIO_MASTER;
		else
			return AV_SYNC_EXTERNAL_CLOCK;
	}
	else 
	{
		return AV_SYNC_EXTERNAL_CLOCK;
	}
}

/* ��ȡ��ʱ�ӵĵ�ǰֵ */
double MediaState::get_master_clock()
{
	double val;
	switch (get_master_sync_type()) 
	{
	case AV_SYNC_VIDEO_MASTER:
		val = vidclk.get_clock();
		break;
	case AV_SYNC_AUDIO_MASTER:
		val = audclk.get_clock();
		break;
	default:
		val = extclk.get_clock();
		break;
	}
	return val;
}

void MediaState::sync_clock_to_slave(Clock * c, Clock * slave)
{
	double clock = c->get_clock();
	double slave_clock = slave->get_clock();
	if (!isnan(slave_clock) && (isnan(clock) || fabs(clock - slave_clock) > AV_NOSYNC_THRESHOLD))
		c->set_clock(slave_clock, *slave->get_serial());
}

