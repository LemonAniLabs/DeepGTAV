#pragma once

#include <Windows.h>
#include <fstream>
#include <ctime>
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>

//#include "../Rewarders/GeneralRewarder.h"
#include "../Scenarios/Scenario.h"

// Screen shared memory
#define SCREEN_IMAGE_SHARED_MEMORY TEXT("Global\\ScreenImage")
struct SharedScreenData
{
	int width;
	int height;
	int stride;
	bool should_toggle_pause_game;
	bool should_agent_wait;
	int sampleCount;
	BYTE imageData[684 * 227];
};

// Reward shared memory
#define REWARD_SHARED_MEMORY TEXT("Global\\AgentReward")
struct SharedRewardData
{
	double distance;
	bool on_road;
	bool should_reset_agent;
	double heading;
	double speed;
	double desired_spin; // for directly setting spin, intermediate step to real control
	double desired_speed; // for directly setting speed, intermediate step to real control
	double desired_speed_change; // for directly setting speed change, intermediate step to real control
	double desired_direction;
	double spin;
	bool should_game_drive;
	bool should_perform_random_action;
	bool is_game_driving;
	int temp_action;
};

class Environment {
private:
	Scenario scenario;

	float recordingPeriod;
	std::clock_t lastRecordingTime;
	int nsample;

	std::wstring datasetDir;
	std::ofstream indexFile;

	int windowWidth;
	int windowHeight;
	int imageWidth;
	int imageHeight;

	HWND hWnd;
	HDC hWindowDC;
	HDC hCaptureDC;
	HBITMAP hCaptureBitmap;
	CLSID pngClsid;

	void saveSample();
	void performActions(float throttle, float brake, float steering);

	void updateRewardInfo();

	SharedScreenData* screen_image_data;
	//SharedRewardData* shared_reward_memory;

public:
	Environment(int _imageWidth, int _imageHeight, int captureFreq, std::string _datasetDir);
	~Environment();

	void setScenario(Scenario _scenario);
	Scenario getScenario();
	void step();

	//Shared Memory
	void InitializeSharedScreenMemory(SharedScreenData **agentControlData);
	void DestroySharedScreenMemory();

	void InitializeSharedRewardMemory(SharedRewardData **rewardData);
	void DestroyRewardSharedMemory();
};

int GetEncoderClsid(const WCHAR* format, CLSID* pClsid);