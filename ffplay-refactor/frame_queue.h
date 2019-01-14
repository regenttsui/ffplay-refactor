#ifndef FRAME_QUEUE_H_
#define FRAME_QUEUE_H_

#include "packet_queue.h"

constexpr auto VIDEO_PICTURE_QUEUE_SIZE = 3;
constexpr auto SUBPICTURE_QUEUE_SIZE = 16;
constexpr auto SAMPLE_QUEUE_SIZE = 9;
#define FRAME_QUEUE_SIZE FFMAX(SAMPLE_QUEUE_SIZE, FFMAX(VIDEO_PICTURE_QUEUE_SIZE, SUBPICTURE_QUEUE_SIZE))

class FrameQueue
{
public:
	FrameQueue() = default;
	FrameQueue(PacketQueue *pkt_q, int m_size, int keep_lst, bool suc = true);
	~FrameQueue();
	void frame_queue_signal();
	Frame *frame_queue_peek_readable();
	Frame *frame_queue_peek();
	Frame *frame_queue_peek_next();
	Frame *frame_queue_peek_last();
	Frame *frame_queue_peek_writable();
	void frame_queue_push();
	void frame_queue_next();
	void frame_queue_destroy();
	int frame_queue_nb_remaining();
	void frame_queue_init(PacketQueue *pkt_q, int m_size, int keep_lst);
	int64_t frame_queue_last_pos();
	bool is_construct();
	SDL_mutex* get_mutex();
	int get_rindex_shown();

private:
	Frame fq[FRAME_QUEUE_SIZE];		
	int rindex;                     
	int windex;                     
	int size;                       
	int max_size;                   
	int keep_last;                  
	int rindex_shown;               
	SDL_mutex *mutex;
	SDL_cond *cond;
	PacketQueue *pktq;              
	int success;					

	void frame_queue_unref_item(Frame *vp);
};


#endif // FRAME_QUEUE_H_

