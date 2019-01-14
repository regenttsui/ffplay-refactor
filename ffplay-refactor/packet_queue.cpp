#include "packet_queue.h"

PacketQueue::PacketQueue(int nb_pkts, int byte_size,
	int64_t dur, bool abort_req, int seri, bool suc) : nb_packets(nb_pkts),
	size(byte_size), duration(dur), abort_request(abort_req), serial(seri), success(suc)
{
	mutex = SDL_CreateMutex();
	if (!mutex)
	{
		av_log(NULL, AV_LOG_FATAL, "SDL_CreateMutex(): %s\n", SDL_GetError());
		success = false;
	}
	cond = SDL_CreateCond();
	if (!cond)
	{
		av_log(NULL, AV_LOG_FATAL, "SDL_CreateCond(): %s\n", SDL_GetError());
		success = false;
	}
}

PacketQueue::~PacketQueue()
{
	packet_queue_flush();
	SDL_DestroyMutex(mutex);
	SDL_DestroyCond(cond);
}

void PacketQueue::packet_queue_init()
{
	mutex = SDL_CreateMutex();
	if (!mutex) 
	{
		av_log(NULL, AV_LOG_FATAL, "SDL_CreateMutex(): %s\n", SDL_GetError());
	}
	cond = SDL_CreateCond();
	if (!cond) 
	{
		av_log(NULL, AV_LOG_FATAL, "SDL_CreateCond(): %s\n", SDL_GetError());
	}
	abort_request = 1; //并未启用队列
	success = true;
}

bool PacketQueue::is_construct()
{
	return success;
}

bool PacketQueue::is_abort()
{
	return abort_request;
}

int PacketQueue::get_serial()
{
	return serial;
}

int PacketQueue::get_nb_packets()
{
	return nb_packets;
}

int PacketQueue::get_size()
{
	return size;
}

int64_t PacketQueue::get_duration()
{
	return duration;
}

/* 真正开始启用队列 */
void PacketQueue::packet_queue_start()
{
	SDL_LockMutex(mutex);
	abort_request = false; 
	packet_queue_put_private(&flush_pkt); 
	SDL_UnlockMutex(mutex);
}

/* 中止队列，实际上只是将终止请求为置1并发出信号 */
void PacketQueue::packet_queue_abort()
{
	SDL_LockMutex(mutex);
	abort_request = 1;
	SDL_CondSignal(cond);  //发出信号
	SDL_UnlockMutex(mutex);
}


void PacketQueue::packet_queue_flush()
{
	MyAVPacketList *pkt, *pkt1;

	SDL_LockMutex(mutex);
	for (pkt = first_pkt; pkt; pkt = pkt1) 
	{
		pkt1 = pkt->next;
		av_packet_unref(&pkt->pkt);
		av_freep(&pkt);
	}
	last_pkt = NULL;
	first_pkt = NULL;
	nb_packets = 0;
	size = 0;
	duration = 0;
	SDL_UnlockMutex(mutex);
}

void PacketQueue::packet_queue_destroy()
{
	packet_queue_flush();
	SDL_DestroyMutex(mutex);
	SDL_DestroyCond(cond);
}

/* 放入空packet意味着流的结束，一般在视频读取完成的时候放入空packet，用于刷新拿到解码器中缓存的最后几帧 */
int PacketQueue::packet_queue_put_nullpacket(int stream_index)
{
	//先创建一个空的packet，然后调用packet_queue_put
	AVPacket pkt1, *pkt = &pkt1;
	av_init_packet(pkt);
	pkt->data = NULL;
	pkt->size = 0;
	pkt->stream_index = stream_index;
	return packet_queue_put(pkt);
}

/* 将packet写入PacketQueue */
int PacketQueue::packet_queue_put(AVPacket * pkt)
{
	int ret;

	SDL_LockMutex(mutex);
	ret = packet_queue_put_private(pkt);
	SDL_UnlockMutex(mutex);

	if (pkt != &flush_pkt && ret < 0)
		av_packet_unref(pkt);

	return ret;
}

int PacketQueue::packet_queue_get(AVPacket * pkt, bool block, int * serial)
{
	MyAVPacketList *pkt1;
	int ret;

	SDL_LockMutex(mutex);

	while (true)
	{
		//中止时跳出循环
		if (abort_request) 
		{
			ret = -1;
			break;
		}

		pkt1 = first_pkt;
		//队列中有数据
		if (pkt1) 
		{
			//第二个节点变为队头，如果第二个节点为空，则队尾为空
			first_pkt = pkt1->next;
			if (!first_pkt)
				last_pkt = NULL;

			//各种统计数据相应减少
			nb_packets--;
			size -= pkt1->pkt.size + sizeof(*pkt1);
			duration -= pkt1->pkt.duration;

			*pkt = pkt1->pkt;

			if (serial)
				*serial = pkt1->serial;
			av_free(pkt1);
			ret = 1;
			break;
		}
		else if (!block) 
		{//队列中没有数据，且非阻塞调用，则跳出循环
			ret = 0;
			break;
		}
		else 
		{//队列中没有数据，且阻塞调用
			SDL_CondWait(cond, mutex);
		}
	}
	SDL_UnlockMutex(mutex);
	return ret;
}

/* 真正的实现写入PacketQueue的函数 */
int PacketQueue::packet_queue_put_private(AVPacket * pkt)
{
	MyAVPacketList *pkt1;

	if (abort_request)
		return -1;

	pkt1 = (MyAVPacketList*)av_malloc(sizeof(MyAVPacketList));
	if (!pkt1)
		return -1;
	pkt1->pkt = *pkt;
	pkt1->next = NULL;

	if (pkt == &flush_pkt)
		serial++;
	pkt1->serial = serial;

	if (!last_pkt)
		first_pkt = pkt1;
	else
		last_pkt->next = pkt1;
	last_pkt = pkt1; 

	nb_packets++;
	size += pkt1->pkt.size + sizeof(*pkt1);
	duration += pkt1->pkt.duration;
	//发出信号，表明当前队列中有数据了，通知等待中的读线程可以取数据了
	SDL_CondSignal(cond);
	return 0;
}

