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
#define LED        13

#define Z_ENDSTOP 12
#define R_ENDSTOP A3

#define ARM_SERVO_OPEN 180
#define ARM_SERVO_CLOSE 0
#define HAND_SERVO_OPEN 180
#define HAND_SERVO_CLOSE 85


#define DISTANCE_FOR_READING_HUM 50
#define FLOOR_SPACE				 200
#define Z_OFFSET				 80
#define ANGLE_OFFSET			 0

#define Z		0
#define R		1

#define RIGHT	-1
#define LEFT	1

#define ON		1
#define OFF		0

#define STEPS_MM	      8.5f
#define STEPS_ROUND		  1600
#define ROTATION_RATIO    2.5

struct TimeInDay
{
	uint8_t Hour = 7;
	uint8_t Minute = 30;
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
	uint8_t Length = 6;
	uint8_t Data[30] = {0, 0, 0, 0, 0, 0};
} GardenTower;

SerialCommand serialCMD;

Servo HandServo;
Servo ArmServo;

int32_t DesiredPos[2] = {0, 0};
int32_t CurrentPos[2] = {0, 0};

int32_t JumpSteps[2] = {0, 0};
int8_t Direction[2] = { LEFT, LEFT };
int16_t Speed[2] = { 400 , 800 };

char TreeInfos[3][20] = {"7:30;1;20", "7:30;1;20", "7:30;1;20"};
char GardenInfo[30] = {"012012"};

char UpdateTimeString[10] = "15:30";

float PotIndex;

uint32_t UpdatePoint = 0;
TimeInDay UpdateTime;

void setup()
{
  Serial.begin(9600);
  
	InitIO();

	VirtualTimer.Init();
	VirtualTimer.Run();

	//InitPosition();

	serialCMD = SerialCommand(&Serial, 9600);
 
	serialCMD.AddCommand("t1", TreeInfos[0]);
	serialCMD.AddCommand("t2", TreeInfos[1]);
	serialCMD.AddCommand("t3", TreeInfos[2]);
	serialCMD.AddCommand("g", GardenInfo);
	serialCMD.AddCommand("p", &PotIndex);
  serialCMD.AddCommand("water", Water);
	serialCMD.AddCommand("time", UpdateTimeString);
	serialCMD.AddCommand("move", MoveToPot);
	serialCMD.AddCommand("update", UpdateData);
	serialCMD.AddCommand("measure", MeasureSoil);
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
  digitalWrite(LED, OFF);

	Direction[Z] = LEFT;
	Direction[R] = LEFT;
}

void InitPosition()
{
	Home();
}

void Home()
{
	HandServo.write(HAND_SERVO_CLOSE);
	delay(500);
	ArmServo.write(ARM_SERVO_CLOSE);
	delay(500);

	CurrentPos[R] = 100000;
	CurrentPos[Z] = 100000;

	MoveTo(R, 0, 800);

	while (!IsEndstopActive(R_ENDSTOP))
	{
	}

	StopAxis(R);
	MoveTo(Z, 0, 400);

	while (!IsEndstopActive(Z_ENDSTOP))
	{
	}

	StopAxis(Z);

	CurrentPos[R] = 0;
	CurrentPos[Z] = 0;
}

bool IsEndstopActive(int endstopPin)
{
	if (digitalRead(endstopPin) == LOW)
		return true;

	return false;
}

void PushPulseRotateStepper()
{
	if (CurrentPos[R] == DesiredPos[R])
		VirtualTimer.Stop(PushPulseRotateStepper);

	TOGGLE(STEP_R);
	CurrentPos[R] += Direction[R];
}

void PushPulseZStepper()
{
	if (CurrentPos[Z] == DesiredPos[Z])
		VirtualTimer.Stop(PushPulseZStepper);

	TOGGLE(STEP_Z);
	CurrentPos[Z] += Direction[Z];
}

void MoveZ(int16_t d)
{
	MoveTo(Z, d * STEPS_MM, Speed[Z]);
}

void RotateTower(int angle)
{
	uint32_t pulse = (((float)angle / 360) * STEPS_ROUND) * 2.5;
	Serial.print("p: ");
	Serial.println(pulse);
	MoveTo(R, pulse, Speed[R]);
}

void MoveTo(uint8_t axis, int32_t pos, uint32_t speed)
{
	uint32_t duty = 1000000 / speed;
	
	DesiredPos[axis] = pos;
	JumpSteps[axis] = DesiredPos[axis] - CurrentPos[axis];

	if (JumpSteps[axis] == 0)
		return;

	int dir = JumpSteps[axis] / abs(JumpSteps[axis]);

	if (axis == R)
	{
		VirtualTimer.Change(PushPulseRotateStepper, duty);
		VirtualTimer.Resum(PushPulseRotateStepper);

		SetDirection(R, dir);
	}
	else
	{
		VirtualTimer.Change(PushPulseZStepper, duty);
		VirtualTimer.Resum(PushPulseZStepper);

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
		state = LOW;
	else
		state = HIGH;

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
		ParseTreeData(i);

    Serial.println(String(Trees[i].TimeToWater.Hour) + ":" + String(Trees[i].TimeToWater.Minute));
    Serial.println(Trees[i].DaySpace);
    Serial.println(Trees[i].IntervalToWater);
	}

	GardenTower.Length = strlen(GardenInfo);

	for (int i = 0; i < GardenTower.Length; i++)
	{
		GardenTower.Data[i] = GardenInfo[i] - '0';
    Serial.print(GardenTower.Data[i]);
	}

	ParseTimeData();

  Serial.println();
  Serial.println(UpdateTime.Hour);
  Serial.println(UpdateTime.Minute);
}

void ParseTreeData(uint8_t index)
{
	String hour;
	String min;
	String spaceDay;
	String interval;
  String s = String(TreeInfos[index]);

	uint8_t semicon[2];

	semicon[0] = s.indexOf(';');
	semicon[1] = s.indexOf(';', semicon[0] + 1);

	String time = s.substring(0, semicon[0]);
	hour = time.substring(0, time.indexOf(':'));
	min = time.substring(time.indexOf(':') + 1);

	spaceDay = s.substring(semicon[0] + 1, semicon[1]);
	interval = s.substring(semicon[1] + 1);

	Trees[index].TimeToWater.Hour = hour.toInt();
	Trees[index].TimeToWater.Minute = min.toInt();
	Trees[index].DaySpace = spaceDay.toInt();
	Trees[index].IntervalToWater = interval.toInt();	
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
  Serial.println("Expand");
  ArmServo.write(ARM_SERVO_OPEN);
  delay(500);
  HandServo.write(HAND_SERVO_CLOSE);
  delay(500);
  Serial.println("Spray");
	//digitalWrite(PUMP_RELAY, ON);
  digitalWrite(LED, ON);
  
  uint8_t treeId = GardenTower.Data[(int)PotIndex];
  
	delay(Trees[treeId].IntervalToWater * 1000);
  
	//digitalWrite(PUMP_RELAY, OFF);
  digitalWrite(LED, OFF);
  
  ArmServo.write(ARM_SERVO_CLOSE);
  Serial.println("Collapse");
}

void MeasureSoil()
{
	Serial.println("Expand");
  ArmServo.write(ARM_SERVO_OPEN);
  delay(2000);
  HandServo.write(HAND_SERVO_OPEN);
  delay(500);
  Serial.println("Measure");
  
  uint16_t height = ((int)PotIndex / 3) * FLOOR_SPACE + Z_OFFSET;
  MoveZ(height - 50);

  delay(3000);

  
	uint16_t sensorValue = map(analogRead(HUM_SENSOR_ANA), 0, 1023, 0, 100);

	Serial.print("m-");
	Serial.println(100 - sensorValue);
  
  MoveZ(height);
  delay(500);
  
  HandServo.write(HAND_SERVO_CLOSE);
  delay(500);
  ArmServo.write(ARM_SERVO_CLOSE);
  delay(1500);
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
	uint16_t height = ((int)PotIndex / 3) * FLOOR_SPACE + Z_OFFSET;
	int angle = (int)PotIndex % 3 * 120 + ANGLE_OFFSET;

	MoveZ(height);
	
	RotateTower(angle);
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
