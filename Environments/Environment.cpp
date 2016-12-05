#include <Windows.h>
#include <ctime>
#include <gdiplus.h>
#include <string>
#pragma comment(lib,"gdiplus.lib")

#include "Environment.h"
#include "../lib/script.h"


//TODO: Fix weather change and bad initial positions.
Environment::Environment(int _imageWidth, int _imageHeight, int captureFreq, std::string _datasetDir) {
	recordingPeriod = 1.0/captureFreq;
	imageWidth = _imageWidth;
	imageHeight = _imageHeight;
	datasetDir.assign(_datasetDir.begin(), _datasetDir.end());
	indexFile.open(datasetDir + L"dataset.txt");

	//Screen capture buffer
	GRAPHICS::_GET_SCREEN_ACTIVE_RESOLUTION(&windowWidth, &windowHeight);
	hWnd = ::FindWindow(NULL, "Grand Theft Auto V");
	hWindowDC = GetDC(hWnd);
	hCaptureDC = CreateCompatibleDC(hWindowDC);
	hCaptureBitmap = CreateCompatibleBitmap(hWindowDC, imageWidth, imageHeight);
	SelectObject(hCaptureDC, hCaptureBitmap);
	SetStretchBltMode(hCaptureDC, COLORONCOLOR);

	Gdiplus::GdiplusStartupInput gdiplusStartupInput;
	ULONG_PTR gdiplusToken;
	GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);
	GetEncoderClsid(L"image/png", &pngClsid);

	nsample = 1;
	lastRecordingTime = std::clock() + ((float) (30.0 * CLOCKS_PER_SEC));

	InitializeSharedScreenMemory(&screen_image_data);
	//InitializeSharedRewardMemory(&shared_reward_memory);
}

Environment::~Environment(){

	DestroySharedScreenMemory();
	//DestroyRewardSharedMemory();

	ReleaseDC(hWnd, hWindowDC);
	DeleteDC(hCaptureDC);
	DeleteObject(hCaptureBitmap);
	
}



void Environment::step() {
	scenario.step();
	
	float delay = ((float)(std::clock() - lastRecordingTime)) / CLOCKS_PER_SEC;
	if (delay >= recordingPeriod) {
		lastRecordingTime = std::clock();
		saveSample();
		updateRewardInfo();
	}
}

void Environment::saveSample() {	
	StretchBlt(hCaptureDC, 0, 0, imageWidth, imageHeight, hWindowDC, 0, 0, windowWidth, windowHeight, SRCCOPY | CAPTUREBLT);

	std::string line = std::to_string(nsample) + " " + std::to_string(scenario.getVehicleSpeed()) + " " + std::to_string(scenario.getVehicleAcceleration()) + " " + std::to_string(scenario.getVehicleBrakePosition()) + " "
		+ std::to_string(scenario.getVehicleSteeringAngle()) + " " + std::to_string(scenario.getVehicleThrottlePosition()) + " " + std::to_string(scenario.getVehicleYawRate()) + " " + std::to_string(scenario.getVehicleDirection()) + "\n";

	Gdiplus::Bitmap image(hCaptureBitmap, (HPALETTE)0);
	//image.Save((datasetDir + std::to_wstring(nsample) + L".png").c_str(), &pngClsid, NULL);
	indexFile.write(line.c_str(), line.length());
	
	screen_image_data->sampleCount = nsample;
	screen_image_data->width = image.GetWidth();
	screen_image_data->height = image.GetHeight();
	
	Gdiplus::BitmapData bmpData;
	Gdiplus::Rect rect(0, 0, screen_image_data->width, screen_image_data->height);
	image.LockBits(&rect, Gdiplus::ImageLockModeRead, PixelFormat24bppRGB, &bmpData);
	BYTE* temp = (bmpData.Stride>0) ? ((BYTE*)bmpData.Scan0) : ((BYTE*)bmpData.Scan0 + bmpData.Stride*(screen_image_data->height - 1));
	screen_image_data->stride = bmpData.Stride;
	memcpy(screen_image_data->imageData, temp, abs(bmpData.Stride)*bmpData.Height);
	image.UnlockBits(&bmpData);

	Ped player_ped = PLAYER::PLAYER_PED_ID();
	int vehicle = PED::GET_VEHICLE_PED_IS_USING(player_ped);
	Vector3 speed = ENTITY::GET_ENTITY_SPEED_VECTOR(vehicle, true);
	//shared_reward_memory->speed = speed.y;

	nsample++;
}

void Environment::performActions(float throttle, float brake, float steering) {
	CONTROLS::_SET_CONTROL_NORMAL(27, 71, throttle); //[0,1]
	CONTROLS::_SET_CONTROL_NORMAL(27, 72, brake); //[0,1];
	CONTROLS::_SET_CONTROL_NORMAL(27, 59, steering); //[-1,1]
}

void Environment::setScenario(Scenario _scenario) {
	scenario = _scenario;
}

Scenario Environment::getScenario(){
	return scenario;
}




int GetEncoderClsid(const WCHAR* format, CLSID* pClsid)
{
	UINT  num = 0;          // number of image encoders
	UINT  size = 0;         // size of the image encoder array in bytes

	Gdiplus::ImageCodecInfo* pImageCodecInfo = NULL;

	Gdiplus::GetImageEncodersSize(&num, &size);
	if (size == 0)
		return -1;  // Failure

	pImageCodecInfo = (Gdiplus::ImageCodecInfo*)(malloc(size));
	if (pImageCodecInfo == NULL)
		return -1;  // Failure

	GetImageEncoders(num, size, pImageCodecInfo);

	for (UINT j = 0; j < num; ++j)
	{
		if (wcscmp(pImageCodecInfo[j].MimeType, format) == 0)
		{
			*pClsid = pImageCodecInfo[j].Clsid;
			free(pImageCodecInfo);
			return j;  // Success
		}
	}

	free(pImageCodecInfo);
	return -1;  // Failure
}

// Screen image shared memory
HANDLE screenImageFileMap;
LPBYTE lpScreenImageSharedMemory = NULL;
void Environment::InitializeSharedScreenMemory(SharedScreenData **agentControlData)
{
	int totalSize = sizeof(SharedScreenData);


	screenImageFileMap = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, totalSize, SCREEN_IMAGE_SHARED_MEMORY);
	if (!screenImageFileMap)
	{
		return;
	}

	lpScreenImageSharedMemory = (LPBYTE)MapViewOfFile(screenImageFileMap, FILE_MAP_ALL_ACCESS, 0, 0, totalSize);
	if (!lpScreenImageSharedMemory)
	{
		CloseHandle(screenImageFileMap);
		screenImageFileMap = NULL;
		return;
	}

	*agentControlData = reinterpret_cast<SharedScreenData*>(lpScreenImageSharedMemory);
}

void Environment::DestroySharedScreenMemory()
{
	if (lpScreenImageSharedMemory && screenImageFileMap)
	{
		UnmapViewOfFile(lpScreenImageSharedMemory);
		CloseHandle(screenImageFileMap);

		screenImageFileMap = NULL;
		lpScreenImageSharedMemory = NULL;
	}
}

// Reward shared memory
HANDLE rewardFileMap;
LPBYTE lpRewardSharedMemory = NULL;
void Environment::InitializeSharedRewardMemory(SharedRewardData **rewardData)
{
	int totalSize = sizeof(SharedRewardData);

	std::stringstream strName;
	strName << REWARD_SHARED_MEMORY;
	rewardFileMap = CreateFileMapping(INVALID_HANDLE_VALUE, NULL,
		PAGE_READWRITE, 0, totalSize, strName.str().c_str());

	if (!rewardFileMap)
	{
		return;
	}

	lpRewardSharedMemory = static_cast<LPBYTE>(
		MapViewOfFile(rewardFileMap, FILE_MAP_ALL_ACCESS, 0, 0, totalSize));

	if (!lpRewardSharedMemory)
	{
		CloseHandle(rewardFileMap);
		rewardFileMap = NULL;
		return;
	}

	*rewardData = reinterpret_cast<SharedRewardData*>(lpRewardSharedMemory);
	(*rewardData)->should_reset_agent = true;
	(*rewardData)->desired_speed = -8192;
	(*rewardData)->desired_spin = -8192;

}

void Environment::DestroyRewardSharedMemory()
{
	if (lpRewardSharedMemory && rewardFileMap)
	{
		UnmapViewOfFile(lpRewardSharedMemory);
		CloseHandle(rewardFileMap);

		rewardFileMap = NULL;
		lpRewardSharedMemory = NULL;
	}
}

void Environment::updateRewardInfo()
{
	//shared_reward_memory->on_road = scenario.isVehicleOnRoad();
	//shared_reward_memory->speed = scenario.getVehicleSpeed();
	
}

// end reward shared memory
