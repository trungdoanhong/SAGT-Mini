// VirtualTimer.h

#ifndef _VirtualTimer_h
#define _VirtualTimer_h

#if defined(ARDUINO) && ARDUINO >= 100
	#include "arduino.h"
#else
	#include "WProgram.h"
#endif

struct Timer
{
	void(*Func)(void) = NULL;
	uint32_t Time = 0;
	uint32_t CountDown = 0;
	bool Enough = true;
};

class VirtualTimerClass
{
public:
	Timer *TimerArray;
	void Init();
	void Add(void(*func)(), uint32_t time);
	void Change(void(*func)(), uint32_t time);
	void Run();
	void Stop();
	void Stop(void(*func)());
	void Resum(void(*func)());
	void Delete(void(*func)());
	bool IsFunctionExit(void(*func)());
	void Execute();
private:
	uint8_t TimerNumber = 0;
	uint16_t interruptTime = 0;
	uint32_t timeCounter = 0;
	uint32_t funcCallingTime = 0;
	uint8_t nearestTimerOrder = 0;
	uint32_t startTime = 0;
	uint32_t executeTime = 0;

	void sort();
};

extern VirtualTimerClass VirtualTimer;

#endif

