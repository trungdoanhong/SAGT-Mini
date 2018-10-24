#include <Servo.h>
#include "VirtualTimer.h"
#include "fastio.h"
#include "SerialCommand.h"

#define ENABLE_R 2
#define STEP_R 3
#define DIR_R 4

// arm step motor pins
#define ENABLE_Z 5
#define STEP_Z 6
#define DIR_Z 7

#define HAND_SERVO_PIN 8
#define ARM_SERVO_PIN 9

#define HUM_SENSOR_DIG 10
#define HUM_SENSOR_ANA A0

#define PUMP_RELAY 11

#define Z_ENDSTOP 12
#define R_ENDSTOP 13

#define ARM_SERVO_OPEN 180
#define ARM_SERVO_CLOSE 0
#define HAND_SERVO_OPEN 85
#define HAND_SERVO_CLOSE 180


#define DISTANCE_FOR_READING_HUM 50
#define FLOOR_SPACE				 20
#define Z_OFFSET				 50
#define ANGLE_OFFSET			 60

#define Z		0
#define R		1

#define RIGHT	-1
#define LEFT	1

#define ON		HIGH
#define OFF		LOW

#define STEPS_MM	      40
#define STEPS_ROUND		  200
#define ROTATION_RATIO    2.5
#define MM_ROUND		  70

struct TimeInDay
{
	uint8_t Hour = 7;
	uint8_t Minute = 0;
};

struct Tree
{
	uint8_t ID = 0;
	TimeInDay TimeToWater;
	uint8_t DaySpace = 1;
	uint16_t IntervalToWater = 10;
} Trees[3];

struct Garden
{
	uint8_t Length;
	uint8_t Data[30];
} GardenTower;

SerialCommand serialCMD;

Servo HandServo;
Servo ArmServo;

int32_t DesiredPos[2];
int32_t CurrentPos[2];

int32_t JumpSteps[2];
int8_t Direction[2] = { LEFT, LEFT };
int8_t Speed[2] = { 200 , 200 };

char TreeInfos[3][20];
char GardenInfo[30];

char UpdateTimeString[10];

float PotIndex;

uint32_t UpdatePoint;
TimeInDay UpdateTime;

void setup()
{
	InitIO();

	VirtualTimer.Init();
	VirtualTimer.Run();

	InitPosition();

	serialCMD = SerialCommand(&Serial, 9600);
	serialCMD.AddCommand("t1", TreeInfos[0]);
	serialCMD.AddCommand("t2", TreeInfos[1]);
	serialCMD.AddCommand("t2", TreeInfos[2]);
	serialCMD.AddCommand("g", GardenInfo);
	serialCMD.AddCommand("p", &PotIndex);
	serialCMD.AddCommand("time", UpdateTimeString);
	serialCMD.AddCommand("move", MoveToPot);
	serialCMD.AddCommand("update", UpdateData);
	serialCMD.AddCommand("w", Water);
	serialCMD.AddCommand("m", MeasureSoil);
}

void loop()
{
	serialCMD.Execute();
	CheckForWater();
}

void InitIO()
{
	HandServo.attach(HAND_SERVO_PIN);
	ArmServo.attach(ARM_SERVO_PIN);

	pinMode(ENABLE_R, OUTPUT);
	pinMode(STEP_R, OUTPUT);
	pinMode(DIR_R, OUTPUT);

	pinMode(ENABLE_Z, OUTPUT);
	pinMode(STEP_Z, OUTPUT);
	pinMode(DIR_Z, OUTPUT);

	pinMode(Z_ENDSTOP, INPUT_PULLUP);
	pinMode(R_ENDSTOP, INPUT_PULLUP);

	pinMode(PUMP_RELAY, OUTPUT);

	digitalWrite(ENABLE_R, LOW);
	digitalWrite(ENABLE_Z, LOW);

	digitalWrite(DIR_R, LOW);
	digitalWrite(DIR_Z, LOW);

	digitalWrite(PUMP_RELAY, OFF);

	Direction[Z] = LEFT;
	Direction[R] = LEFT;
}

void InitPosition()
{
	HandServo.write(HAND_SERVO_CLOSE);
	ArmServo.write(ARM_SERVO_CLOSE);	

	MoveTo(Z, 100000, 200);
	MoveTo(R, 100000, 200);

	while (!IsEndstopActive(R_ENDSTOP))
	{
		
	}

	StopAxis(R);

	while (!IsEndstopActive(Z_ENDSTOP))
	{

	}

	StopAxis(Z);
}

bool IsEndstopActive(int endstopPin)
{
	if (digitalRead(endstopPin) == LOW)
		return true;

	return false;
}

void PushPulseRotateStepper()
{
	if (CurrentPos[R] == JumpSteps[R])
		VirtualTimer.Stop(PushPulseRotateStepper);

	TOGGLE(STEP_R);
	CurrentPos[R] += Direction[R];
}

void PushPulseZStepper()
{
	if (CurrentPos[Z] == JumpSteps[Z])
		VirtualTimer.Stop(PushPulseZStepper);

	TOGGLE(STEP_Z);
	CurrentPos[Z] += Direction[Z];
}

void MoveZ(int16_t d)
{
	MoveTo(Z, d * STEPS_MM, 200);
}

void RotateTower(int angle)
{
	MoveTo(R, ((float)angle / 1.8f) * 2.5, 200);
}

void MoveTo(uint8_t axis, int32_t pos, uint32_t speed)
{
	uint32_t duty = 1000000 / speed;
	
	DesiredPos[axis] = pos;
	JumpSteps[axis] = DesiredPos[axis] - CurrentPos[axis];

	int dir = JumpSteps[axis] / abs(JumpSteps[axis]);

	if (axis == R)
	{
		VirtualTimer.Change(PushPulseRotateStepper, duty);
		SetDirection(R, dir);
	}
	else
	{
		VirtualTimer.Change(PushPulseZStepper, duty);
		SetDirection(Z, dir);
	}
}

void StopAxis(uint8_t axis)
{
	if (axis == R)
	{
		VirtualTimer.Stop(PushPulseRotateStepper);
	}
	else
	{
		VirtualTimer.Stop(PushPulseZStepper);
	}
}

void SetDirection(uint8_t axis, int8_t dir)
{
	Direction[axis] = dir;

	uint8_t state;

	if (dir > 0)
		state = HIGH;
	else
		state = LOW;

	if (axis == R)
	{
		digitalWrite(DIR_R, state);
	}
	else
	{
		digitalWrite(DIR_Z, state);
	}
}

void UpdateData()
{
	for (int i = 0; i < 3; i++)
	{
		ParseTreeData(String(TreeInfos[i]), Trees[i]);
	}

	GardenTower.Length = strlen(GardenInfo);

	for (int i = 0; i < GardenTower.Length; i++)
	{
		GardenTower.Data[i] = GardenInfo[i] - '0';
	}

	ParseTimeData();
}

void ParseTreeData(String s, Tree tree)
{
	String hour;
	String min;
	String spaceDay;
	String interval;

	uint8_t semicon[2];

	semicon[0] = s.indexOf(';');
	semicon[1] = s.indexOf(';', semicon[0] + 1);

	String time = s.substring(0, semicon[0]);
	hour = time.substring(0, time.indexOf(':'));
	min = time.substring(time.indexOf(':') + 1);

	spaceDay = s.substring(semicon[0] + 1, semicon[1]);
	interval = s.substring(semicon[1] + 1);

	tree.TimeToWater.Hour = hour.toInt();
	tree.TimeToWater.Minute = min.toInt();
	tree.DaySpace = spaceDay.toInt();
	tree.IntervalToWater = interval.toInt();	
}

void ParseTimeData()
{
	String s(UpdateTimeString);

	UpdateTime.Hour = s.substring(0, s.indexOf(":")).toInt();
	UpdateTime.Minute = s.substring(s.indexOf(":") + 1).toInt();

	UpdatePoint = millis();
}

void Water()
{
	digitalWrite(PUMP_RELAY, ON);
	
	DelayS(Trees[GardenTower.Data[(int)PotIndex]].IntervalToWater);

	digitalWrite(PUMP_RELAY, OFF);
}

void MeasureSoil()
{
	ArmServo.write(ARM_SERVO_OPEN);
	MoveZ(50);

	DelayS(500);

	uint16_t sensorValue = map(analogRead(HUM_SENSOR_ANA), 0, 1023, 0, 100);

	Serial.print("m-");
	Serial.println(sensorValue);
}

void DelayS(uint32_t t)
{
	uint32_t p = millis();

	while (millis() - p < t)
	{
		serialCMD.Execute();
	}
}

void MoveToPot()
{
	MoveZ((PotIndex / 3) * FLOOR_SPACE + Z_OFFSET);
	
	RotateTower((int)PotIndex % 3 * 120 + ANGLE_OFFSET);
}

void CheckForWater()
{
	for (int i = 0; i < GardenTower.Length; i++)
	{
		Tree tree = Trees[GardenTower.Data[i]];
		uint32_t st = (tree.TimeToWater.Hour * 60 + tree.TimeToWater.Minute) - (UpdateTime.Hour * 60 + UpdateTime.Minute);
		
		uint32_t delta = (millis() - UpdatePoint) / 60000;

		if (st == delta)
		{
			HandServo.write(HAND_SERVO_OPEN);
			ArmServo.write(ARM_SERVO_OPEN);

			PotIndex = i;
			MoveToPot();

			while (CurrentPos[R] != JumpSteps[R] || CurrentPos[Z] != JumpSteps[Z])
			{
				
			}

			Water();

			ArmServo.write(ARM_SERVO_CLOSE);
			HandServo.write(HAND_SERVO_CLOSE);

			DelayS(500);
		}
	}
}