#include "frame_queue.h"

/* ��ʼ��Frame���� */
FrameQueue::FrameQueue(PacketQueue *pkt_q, int m_size, int keep_lst, bool suc) :success(suc)
{
	//���㣬��������������������
	if (!(mutex = SDL_CreateMutex()))
	{
		av_log(NULL, AV_LOG_FATAL, "SDL_CreateMutex(): %s\n", SDL_GetError());
		success = false;
	}
	if (!(cond = SDL_CreateCond()))
	{
		av_log(NULL, AV_LOG_FATAL, "SDL_CreateCond(): %s\n", SDL_GetError());
		success = false;
	}

	//����Ӧ��packet���й���
	pktq = pkt_q;

	max_size = FFMIN(m_size, FRAME_QUEUE_SIZE);

	keep_last = !!keep_lst;

	//�����е�ÿһ��frame�����ڴ�
	for (int i = 0; i < max_size; i++)
		if (!(fq[i].frame = av_frame_alloc()))
			success = false;
}

/* ����Frame���� */
FrameQueue::~FrameQueue()
{
	for (int i = 0; i < max_size; i++)
	{
		Frame *vp = &fq[i];
		frame_queue_unref_item(vp);
		av_frame_free(&vp->frame);
	}
	SDL_DestroyMutex(mutex);
	SDL_DestroyCond(cond);
}

/* ���źţ����ⲿ������� */
void FrameQueue::frame_queue_signal()
{
	SDL_LockMutex(mutex);
	SDL_CondSignal(cond);
	SDL_UnlockMutex(mutex);
}

/* ����Ƿ�ɶ��������򷵻ص�ǰ�ڵ� */
Frame * FrameQueue::frame_queue_peek_readable()
{
	SDL_LockMutex(mutex);
	while (size - rindex_shown <= 0 &&!pktq->is_abort()) 
	{
		SDL_CondWait(cond, mutex);
	}
	SDL_UnlockMutex(mutex);

	if (pktq->is_abort())
		return NULL;

	return &fq[(rindex + rindex_shown) % max_size];
}

/* ����ǰ�ڵ㣬��frame_queue_peek_readable��Ч����û�м���Ƿ��пɶ��ڵ� */
Frame * FrameQueue::frame_queue_peek()
{
	return &fq[(rindex + rindex_shown) % max_size];
}

/* ����һ���ڵ� */
Frame * FrameQueue::frame_queue_peek_next()
{
	return &fq[(rindex + rindex_shown + 1) % max_size];
}

/* ����һ���ڵ㣬ע�ⲻ��Ҫ������ */
Frame * FrameQueue::frame_queue_peek_last()
{
	return &fq[rindex];
}

/* �������Ƿ���Խ���д�룬���Ƿ�����Ӧ��Frame�Թ���������д�����ֵ */
Frame * FrameQueue::frame_queue_peek_writable()
{
	SDL_LockMutex(mutex);
	while (size >= max_size && !pktq->is_abort()) 
	{
		SDL_CondWait(cond, mutex);
	}
	SDL_UnlockMutex(mutex);

	if (pktq->is_abort())
		return NULL;

	return &fq[windex];
}

/* ��д����ر���ֵ��Frame������� */
void FrameQueue::frame_queue_push()
{
	if (++windex == max_size)
		windex = 0;
	SDL_LockMutex(mutex);
	size++;
	SDL_CondSignal(cond);
	SDL_UnlockMutex(mutex);
}

/* �ڶ���һ���ڵ����ã����ڱ��һ���ڵ��Ѿ������� */
void FrameQueue::frame_queue_next()
{
	if (keep_last && !rindex_shown) 
	{
		rindex_shown = 1;
		return;
	}

	frame_queue_unref_item(&fq[rindex]);
	if (++rindex == max_size)
		rindex = 0;
	SDL_LockMutex(mutex);
	size--;
	SDL_CondSignal(cond);
	SDL_UnlockMutex(mutex);
}


void FrameQueue::frame_queue_destroy()
{
	for (int i = 0; i < max_size; i++)
	{
		Frame *vp = &fq[i];
		frame_queue_unref_item(vp);
		av_frame_free(&vp->frame);
	}
	SDL_DestroyMutex(mutex);
	SDL_DestroyCond(cond);
}

/* ����δ����֡��(��������ǰ�ڵ�) */
int FrameQueue::frame_queue_nb_remaining()
{
	return size - rindex_shown;
}

void FrameQueue::frame_queue_init(PacketQueue * pkt_q, int m_size, int keep_lst)
{
	//���㣬��������������������
	if (!(mutex = SDL_CreateMutex()))
	{
		av_log(NULL, AV_LOG_FATAL, "SDL_CreateMutex(): %s\n", SDL_GetError());
		success = false;
	}
	if (!(cond = SDL_CreateCond()))
	{
		av_log(NULL, AV_LOG_FATAL, "SDL_CreateCond(): %s\n", SDL_GetError());
		success = false;
	}

	//����Ӧ��packet���й���
	pktq = pkt_q;

	max_size = FFMIN(m_size, FRAME_QUEUE_SIZE);

	keep_last = !!keep_lst;

	for (int i = 0; i < max_size; i++)
		fq[i].frame = av_frame_alloc();

}

/* ������һ�����ŵ�֡���ֽ�λ�� */
int64_t FrameQueue::frame_queue_last_pos()
{
	Frame *fp = &fq[rindex];
	if (rindex_shown && fp->serial == pktq->get_serial())
		return fp->pos;
	else
		return -1;
}

/* �ͷ�AVFrame�������ڴ� */
void FrameQueue::frame_queue_unref_item(Frame * vp)
{

	av_frame_unref(vp->frame);
	avsubtitle_free(&vp->sub);
}

bool FrameQueue::is_construct()
{
	return success;
}

SDL_mutex * FrameQueue::get_mutex()
{
	return mutex;
}

int FrameQueue::get_rindex_shown()
{
	return rindex_shown;
}
