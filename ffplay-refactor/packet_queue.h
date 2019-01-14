#ifndef PACKET_QUEUE_H_
#define PACKET_QUEUE_H_

#include <iostream>
#include <queue>
#include <utility>

#include "global.h"

using std::cout;
using std::endl;
using std::queue;
using std::pair;
using std::make_pair;

//PacketQueue的一个节点
typedef struct MyAVPacketList
{
	AVPacket pkt;
	struct MyAVPacketList *next;    //下一个节点
	int serial;                     //序列号，用于标记当前节点的序列号，区分是否连续数据，在seek的时候发挥作用
} MyAVPacketList;

class PacketQueue
{
public:
	PacketQueue() = default;
	PacketQueue(int nb_pkts = 0, int byte_size = 0,
		int64_t dur = 0, bool abort_req = true, int seri = 0, bool suc = true);
	~PacketQueue();
	void packet_queue_init();
	void packet_queue_start();
	void packet_queue_abort();
	void packet_queue_flush();
	void packet_queue_destroy();
	int packet_queue_put_nullpacket(int stream_index);
	int packet_queue_put(AVPacket *pkt);
	int packet_queue_get(AVPacket *pkt, bool block, int *serial);
	bool is_construct();
	bool is_abort();
	int get_serial();
	int get_nb_packets();
	int get_size();
	int64_t get_duration();


private:
	MyAVPacketList *first_pkt, *last_pkt;   
	int nb_packets;                 
	int size;                       
	int64_t duration;               
	bool abort_request;             
	int serial;                     
	SDL_mutex *mutex;               
	SDL_cond *cond;                 
	int success;					

	int packet_queue_put_private(AVPacket *pkt);


};



#endif // !PACKET_QUEUE_H_
