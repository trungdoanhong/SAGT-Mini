// 
// 
// 

#include "VirtualTimer.h"

void VirtualTimerClass::Init()
{
	cli();                                  // Turn off global interrupt

	/* Reset Timer/Counter2 */
	TCCR2A = 0;
	TCCR2B = 0;
	TIMSK2 = 0;

	/* Setup Timer/Counter2 
	CS22	CS21	CS20	Prescale
	0		0		0		Stop
	0		0		1		1
	0		1		0		8
	0		1		1		64
	1		0		0		256
	1		0		1		1024	
	*/
	TCCR2B = (0 << CS22) | (1 << CS21) | (0 << CS20);	// prescale is 8
	TCNT2 = 0;
	TIMSK2 = (1 << TOIE2);                  // Overflow interrupt enable 

	TimerNumber = 0;
	interruptTime = 0;
	timeCounter = 0;
	funcCallingTime = 0;
	nearestTimerOrder = 0;
}

//Add a function into Timer Array and run every time

void VirtualTimerClass::Add(void(*func)(), uint32_t time)
{
	TimerNumber++;		// initial number is 0 and increase 1 value before add and function into array
	Timer *TimerTemp = new Timer[TimerNumber];	// temporary array use to save olde Timer array

	for (uint8_t index = 0; index < (TimerNumber - 1); index++)
	{
		TimerTemp[index] = TimerArray[index];
	}

	if (TimerArray != NULL)
	{
		delete[] TimerArray;
	}

	TimerArray = TimerTemp;
	
	Timer Timer;
	Timer.Func = func;
	Timer.Time = time;
	Timer.CountDown = time;
 
	// Add new Timer into array
	TimerArray[TimerNumber - 1] = Timer;

	if (funcCallingTime == 0 || time < funcCallingTime )
	{
		funcCallingTime = time;
		nearestTimerOrder = TimerNumber - 1;
	}
}

// Sort every Timer increase from small to big base on time.

void VirtualTimerClass::sort()
{
	Timer tempTimer;
	for (uint8_t index = 0; index < TimerNumber - 1; index++)
	{
		for (uint8_t index1 = 1; index1 < TimerNumber; index1++)
		{
			if (TimerArray[index].Time > TimerArray[index1].Time)
			{
				tempTimer = TimerArray[index];
				TimerArray[index] = TimerArray[index1];
				TimerArray[index1] = tempTimer;

			}
		}
	}
}

void VirtualTimerClass::Run()
{
	sort();
	sei();	

	interruptTime = 254 - ((funcCallingTime - timeCounter) % 128) * 2;
	if (funcCallingTime - timeCounter > 127)
		TCNT2 = 0;
	else
		TCNT2 = interruptTime;
}

void VirtualTimerClass::Stop()
{
	for (uint8_t index = 0; index < TimerNumber; index++)
	{
		TimerArray[index].Enough = false;
	}
}

void VirtualTimerClass::Execute()
{
	startTime = millis();
	timeCounter += 128;
	if (timeCounter < funcCallingTime)
	{
		interruptTime = 255 - ((funcCallingTime - timeCounter) % 128 ) * 2;

		if (funcCallingTime - timeCounter > 127)
			TCNT2 = 0 + 2;
		else
			TCNT2 = interruptTime + 2;
		return;
	} 
	//when enough time to call function

	timeCounter = 0;

	for (uint8_t index = 0; index < TimerNumber; index++)
	{
		if (TimerArray[index].Enough == false)
			continue;

		if (TimerArray[index].CountDown == TimerArray[nearestTimerOrder].CountDown)
		{
			
			TimerArray[index].Func();
			TimerArray[index].CountDown = TimerArray[index].Time;
		}
		else
		{
			TimerArray[index].CountDown -= funcCallingTime;
		}			
	}

	nearestTimerOrder = 0;

	for (uint8_t index = 1; index < TimerNumber; index++)
	{
		if (TimerArray[index].Enough == false)
			continue;
		// Find Timer order has count down time is smallest
		if (TimerArray[index].CountDown < TimerArray[nearestTimerOrder].CountDown)
		{
			nearestTimerOrder = index;
		}
	}

	// Set time to call function in next time
	funcCallingTime = TimerArray[nearestTimerOrder].CountDown;

	interruptTime = 255 - ((funcCallingTime - timeCounter) % 128) * 2;

	executeTime = millis() - startTime;

	if (funcCallingTime - timeCounter > 127)
		interruptTime = 0 + executeTime * 2 + 44;
	else
		interruptTime = interruptTime + executeTime * 2 + 44;

	if (interruptTime > 254)
		TCNT2 = 254;
	else
		TCNT2 = interruptTime;
}

void VirtualTimerClass::Change(void(*func)(), uint32_t time)
{
	if (!this->IsFunctionExit(func))
	{
		this->Add(func, time);
		sort();
		return;
	}		

	for (uint8_t index = 0; index < TimerNumber; index++)
	{ 
		if (TimerArray[index].Func == func)
		{
			TimerArray[index].Time = time;
			TimerArray[index].CountDown = time;
		}
	}
}

void VirtualTimerClass::Stop(void(*func)())
{
	for (uint8_t index = 0; index < TimerNumber; index++)
	{
		if (TimerArray[index].Func == func)
		{
			TimerArray[index].Enough = false;
		}
	}
}

void VirtualTimerClass::Resum(void(*func)())
{
	for (uint8_t index = 0; index < TimerNumber; index++)
	{
		if (TimerArray[index].Func == func)
		{
			TimerArray[index].Enough = true;
			TimerArray[index].CountDown = TimerArray[index].Time;
		}
	}

	for (uint8_t index = 0; index < TimerNumber; index++)
	{
		if (TimerArray[index].Enough == false || TimerArray[index].Func == func)
			continue;
		// Set count down time every Timer
		TimerArray[index].CountDown -= (timeCounter + 1);
	}

	nearestTimerOrder = 0;

	for (uint8_t index = 1; index < TimerNumber; index++)
	{
		if (TimerArray[index].Enough == false)
			continue;
		// Find Timer order has count down time is smallest
		if (TimerArray[index].CountDown < TimerArray[nearestTimerOrder].CountDown)
		{
			nearestTimerOrder = index;
		}
	}

	// Set time to call function in next time
	funcCallingTime = TimerArray[nearestTimerOrder].CountDown;

	timeCounter = 0;
}

void VirtualTimerClass::Delete(void(*func)())
{
	if (TimerNumber > 0)

	TimerNumber--;	
	Timer *TimerTemp = new Timer[TimerNumber];	// temporary array use to save olde Timer array

	uint8_t index1 = 0;
	for (uint8_t index2 = 0; index2 < TimerNumber + 1; index2++)
	{
		if (TimerArray[index2].Func != func)
		{
			TimerTemp[index1] = TimerArray[index2];
			index1++;
		}
	}

	if (TimerArray != NULL)
	{
		delete[] TimerArray;
	}

	TimerArray = new Timer[TimerNumber];
	for (uint8_t index = 0; index < TimerNumber; index++)
	{
		TimerArray[index] = TimerTemp[index];
	}
}

bool VirtualTimerClass::IsFunctionExit(void(*func)())
{
	for ( uint8_t index = 0; index < TimerNumber; index++ )
	{
		if (TimerArray[index].Func == func)
		{
			return true;
		}
	}
	return false;
}

ISR(TIMER2_OVF_vect)
{
	VirtualTimer.Execute();
}

VirtualTimerClass VirtualTimer;

