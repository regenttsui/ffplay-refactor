#include "clock.h"

Clock::Clock(int * q_serial, int spd, int pause) : queue_serial(q_serial), speed(spd), paused(pause)
{
	set_clock(NAN, -1);
}

/* ��ȡһ����/��Ƶ��ʱ�� */
double Clock::get_clock()
{
	if (*queue_serial != serial)
		return NAN;
	if (paused)
	{
		return pts; 
	}
	else
	{
		double time = av_gettime_relative() / 1000000.0; //��ȡ��ǰʱ��
		return pts_drift + time - (time - last_updated) * (1.0 - speed);
	}
}

/* ����һ����/��Ƶʱ�� */
void Clock::set_clock_at(double pts, int serial, double time)
{
	this->pts = pts;
	last_updated = time;	//��һ�θ���ʱ����Ϊ��ǰʱ��
	pts_drift = pts - time; //ĳʱ��pts�͵�ǰʱ��Ĳ�ֵ��������һ��У׼ʱ�ӵ�pts
	this->serial = serial;
}

/* ʹ��ϵͳ��ǰʱ�����set_clock_at������һ��ʱ�ӣ���������ʱ��ʹ�õĶ��Ǹú��� */
void Clock::set_clock(double pts, int serial)
{
	double time = av_gettime_relative() / 1000000.0;
	set_clock_at(pts, serial, time);
}

int* Clock::get_serial()
{
	return &serial;
}

double Clock::get_pts()
{
	return pts;
}

void Clock::set_paused(int pause)
{
	paused = pause;
}


double Clock::get_last_updated()
{
	return last_updated;
}

/* ������ʱ���ٶ� */
void Clock::set_clock_speed(double speed)
{
	set_clock(get_clock(), serial);
	this->speed = speed;
}
