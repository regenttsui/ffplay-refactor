#include "frame_queue.h"

/* 初始化Frame队列 */
FrameQueue::FrameQueue(PacketQueue *pkt_q, int m_size, int keep_lst, bool suc) :success(suc)
{
	//清零，建立互斥量和条件变量
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

	//跟相应的packet队列关联
	pktq = pkt_q;

	max_size = FFMIN(m_size, FRAME_QUEUE_SIZE);

	keep_last = !!keep_lst;

	//给队列的每一个frame分配内存
	for (int i = 0; i < max_size; i++)
		if (!(fq[i].frame = av_frame_alloc()))
			success = false;
}

/* 销毁Frame队列 */
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

/* 发信号，供外部程序调用 */
void FrameQueue::frame_queue_signal()
{
	SDL_LockMutex(mutex);
	SDL_CondSignal(cond);
	SDL_UnlockMutex(mutex);
}

/* 检测是否可读，若是则返回当前节点 */
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

/* 读当前节点，与frame_queue_peek_readable等效，但没有检查是否有可读节点 */
Frame * FrameQueue::frame_queue_peek()
{
	return &fq[(rindex + rindex_shown) % max_size];
}

/* 读下一个节点 */
Frame * FrameQueue::frame_queue_peek_next()
{
	return &fq[(rindex + rindex_shown + 1) % max_size];
}

/* 读上一个节点，注意不需要求余了 */
Frame * FrameQueue::frame_queue_peek_last()
{
	return &fq[rindex];
}

/* 检测队列是否可以进行写入，若是返回相应的Frame以供后续对其写入相关值 */
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

/* 将写好相关变量值的Frame存入队列 */
void FrameQueue::frame_queue_push()
{
	if (++windex == max_size)
		windex = 0;
	SDL_LockMutex(mutex);
	size++;
	SDL_CondSignal(cond);
	SDL_UnlockMutex(mutex);
}

/* 在读完一个节点后调用，用于标记一个节点已经被读过 */
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

/* 返回未读的帧数(不包括当前节点) */
int FrameQueue::frame_queue_nb_remaining()
{
	return size - rindex_shown;
}

void FrameQueue::frame_queue_init(PacketQueue * pkt_q, int m_size, int keep_lst)
{
	//清零，建立互斥量和条件变量
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

	//跟相应的packet队列关联
	pktq = pkt_q;

	max_size = FFMIN(m_size, FRAME_QUEUE_SIZE);

	keep_last = !!keep_lst;

	for (int i = 0; i < max_size; i++)
		fq[i].frame = av_frame_alloc();

}

/* 返回上一个播放的帧的字节位置 */
int64_t FrameQueue::frame_queue_last_pos()
{
	Frame *fp = &fq[rindex];
	if (rindex_shown && fp->serial == pktq->get_serial())
		return fp->pos;
	else
		return -1;
}

/* 释放AVFrame关联的内存 */
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
