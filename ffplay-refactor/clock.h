#ifndef CLOCK_H_
#define CLOCK_H_

#ifndef __STDC_CONSTANT_MACROS
#define __STDC_CONSTANT_MACROS
#endif
extern "C"
{
#include <libavutil/avutil.h>
#include <libavutil/time.h>
};

/* 时钟差异小于该阈值则认为已同步，不必进行时钟同步 */
constexpr auto AV_SYNC_THRESHOLD_MIN = 0.04;
/* 时钟差异大于该阈值则需要进行时钟同步 */
constexpr auto AV_SYNC_THRESHOLD_MAX = 0.1;
/* 如果一帧的时长大于该阈值，则为了实现同步，不再重复显示/播放该帧 */
constexpr auto AV_SYNC_FRAMEDUP_THRESHOLD = 0.1;
/* 如果差异大于该阈值，表明错误过大，不进行同步 */
constexpr auto AV_NOSYNC_THRESHOLD = 10.0;

constexpr auto SAMPLE_CORRECTION_PERCENT_MAX = 10;

//视音频同步类型
enum 
{
	AV_SYNC_AUDIO_MASTER,   /* 默认视频同步音频 */
	AV_SYNC_VIDEO_MASTER,
	AV_SYNC_EXTERNAL_CLOCK
};

class Clock
{
public:
	Clock() = default;
	Clock(int *q_serial, int spd = 1.0, int pause = 0);
	~Clock() = default;
	double get_clock();
	void set_clock_at(double pts, int serial, double time);
	void set_clock(double pts, int serial);
	int* get_serial();
	double get_pts();
	void set_paused(int pause);
	double get_last_updated();

private:
	double pts;           	
	double pts_drift;     	
	double last_updated;  	
	double speed;         	
	int serial;           	
	int paused;           	
	int *queue_serial;    

	void set_clock_speed(double speed);
};




#endif // CLOCK_H_

