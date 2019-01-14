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

/* ʱ�Ӳ���С�ڸ���ֵ����Ϊ��ͬ�������ؽ���ʱ��ͬ�� */
constexpr auto AV_SYNC_THRESHOLD_MIN = 0.04;
/* ʱ�Ӳ�����ڸ���ֵ����Ҫ����ʱ��ͬ�� */
constexpr auto AV_SYNC_THRESHOLD_MAX = 0.1;
/* ���һ֡��ʱ�����ڸ���ֵ����Ϊ��ʵ��ͬ���������ظ���ʾ/���Ÿ�֡ */
constexpr auto AV_SYNC_FRAMEDUP_THRESHOLD = 0.1;
/* ���������ڸ���ֵ������������󣬲�����ͬ�� */
constexpr auto AV_NOSYNC_THRESHOLD = 10.0;

constexpr auto SAMPLE_CORRECTION_PERCENT_MAX = 10;

//����Ƶͬ������
enum 
{
	AV_SYNC_AUDIO_MASTER,   /* Ĭ����Ƶͬ����Ƶ */
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

