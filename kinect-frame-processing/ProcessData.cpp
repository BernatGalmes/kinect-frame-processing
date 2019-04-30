#include "ProcessData.h"

#include <windows.h>

// Kinect Header files
#include <Kinect.h>


#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>

// basic file operations
#include <fstream>
#include <iostream>
#include <string>

#define COLOR_WIDTH 1920
#define COLOR_HEIGHT 1080

using namespace std;
using namespace cv;

string PATH_RESULTS = "D:\\frames";
string PATH_DEPTH = PATH_RESULTS + "\\depth";
string PATH_COLOR = PATH_RESULTS + "\\color";
string PATH_BG = PATH_RESULTS + "\\background";
string FILE_HAND_TRACK = PATH_RESULTS + "\\handJoints.csv";

/**
* If the specified directory by param don't exist create it
*/
void mkdirIfNotExist(const wchar_t * directory)
{
	if (CreateDirectory(directory, NULL))
	{
		// Directory created
	}
	else if (ERROR_ALREADY_EXISTS == GetLastError())
	{
		// Directory already exists
	}
	else if (ERROR_PATH_NOT_FOUND == GetLastError())
	{
		// One or more intermediate directories do not exist
	}
	else
	{
		// Failed for some other reason
	}

}
ProcessData::ProcessData(ICoordinateMapper* mapper, int wDepth, int hDepth)
{
	const wchar_t * dir_res = std::wstring(PATH_RESULTS.begin(), PATH_RESULTS.end()).c_str();
	const wchar_t * dir_color = std::wstring(PATH_COLOR.begin(), PATH_COLOR.end()).c_str();
	const wchar_t * dir_depth = std::wstring(PATH_DEPTH.begin(), PATH_DEPTH.end()).c_str();
	const wchar_t * dir_bg = std::wstring(PATH_BG.begin(), PATH_BG.end()).c_str();

	mkdirIfNotExist(dir_res);
	mkdirIfNotExist(dir_depth);
	mkdirIfNotExist(dir_color);
	mkdirIfNotExist(dir_bg);

	this->mapper = mapper;
	depthHeight = hDepth;
	depthWidth = wDepth;
	nFrame = 0;

	// Write file headers
	ofstream bodyTrackFile;
	bodyTrackFile.open(FILE_HAND_TRACK, ios::trunc);
	bodyTrackFile << "nFrame, Time, leftHandX, leftHandY, leftHandState, rightHandX, rightHandY, rightHandState" <<  endl;
	bodyTrackFile.close();
}

ProcessData::~ProcessData()
{

}

void ProcessData::process(uint16 * depthBuffer, uint32* colorBuffer, ColorSpacePoint* m_colorSpacePoints, uint8* bgMask)
{
	manageDepthFrame(depthBuffer);
	manageColorFrame(colorBuffer, m_colorSpacePoints);
	saveBackgroundMask(bgMask);
	nFrame++;
}

void ProcessData::storeBodies(INT64 nTime, int nBodyCount, IBody** ppBodies)
{
	HRESULT hr;
	int width = depthWidth;
	int height = depthHeight;
	ofstream bodyTrackFile;
	
	bodyTrackFile.open(FILE_HAND_TRACK, ios::app);
	for (int i = 0; i < nBodyCount; ++i)
	{
		IBody* pBody = ppBodies[i];
		if (pBody)
		{
			BOOLEAN bTracked = false;
			hr = pBody->get_IsTracked(&bTracked);

			if (SUCCEEDED(hr) && bTracked)
			{
				Joint joints[JointType_Count];
				Joint leftHandJoint, rightHandJoint;
				D2D1_POINT_2F jointPoints[JointType_Count];
				HandState leftHandState = HandState_Unknown;
				HandState rightHandState = HandState_Unknown;

				pBody->get_HandLeftState(&leftHandState);
				pBody->get_HandRightState(&rightHandState);

				hr = pBody->GetJoints(_countof(joints), joints);
				if (SUCCEEDED(hr))
				{
					for (int j = 0; j < _countof(joints); ++j)
					{
						jointPoints[j] = BodyToScreen(joints[j].Position, width, height);
					}
					leftHandJoint = joints[JointType_HandLeft];
					rightHandJoint = joints[JointType_HandRight];

					bodyTrackFile << nFrame << ", " << 
						nTime << ", " <<
						jointPoints[JointType_HandLeft].x << ", " <<
						jointPoints[JointType_HandLeft].y << ", " <<
						leftHandState << ", ";
					bodyTrackFile << jointPoints[JointType_HandRight].x << ", " <<
						jointPoints[JointType_HandRight].y << ", " <<
						rightHandState << endl;

				}
			}
		}
	}
	bodyTrackFile.close();
}

void ProcessData::manageDepthFrame(uint16 * depthBuffer)
{
	Mat dMat;
	string imName = "\\depth_" + to_string(nFrame) + ".png";

	dMat = Mat(depthHeight, depthWidth, CV_16UC1, depthBuffer);
	cv::imwrite(PATH_DEPTH + imName, dMat);
}

void ProcessData::saveBackgroundMask (uint8 * bgMask)
{
	Mat bgMat;
	string imName = "\\bg_" + to_string(nFrame) + ".png";

	bgMat = Mat(depthHeight, depthWidth, CV_8UC1, bgMask);
	cv::imwrite(PATH_BG + imName, bgMat);
}

void ProcessData::manageColorFrame(uint32* colorBuffer, ColorSpacePoint* m_colorSpacePoints)
{
	ColorSpacePoint csp;
	Mat colorMat;
	uint32* m_pixelBuffer = new uint32[depthWidth * depthHeight];

	//copy depth data to the screen
	for (int i = 0; i < depthWidth * depthHeight; ++i)
	{
		csp = m_colorSpacePoints[i];
		int ix = (int)csp.X;
		int iy = (int)csp.Y;

		if (ix >= 0 && ix < COLOR_WIDTH && iy >= 0 && iy < COLOR_HEIGHT)
			m_pixelBuffer[i] = colorBuffer[ix + iy * COLOR_WIDTH];
		else
			m_pixelBuffer[i] = 0xff0000;

		//uint16 d = m_depthBuffer[i];
		//uint8 a = d & 0xff;
		//m_pixelBuffer[i] = (a << 16) | (a << 8) | a;
	}

	// Every array item contains the 4 channels
	colorMat = Mat(Size(depthWidth, depthHeight), CV_8UC4, m_pixelBuffer);

	string imName = "\\color_" + to_string(nFrame) + ".png";
	cv::imwrite(PATH_COLOR + imName, colorMat);
}

/// <summary>
/// Converts a body point to screen space
/// </summary>
/// <param name="bodyPoint">body point to tranform</param>
/// <param name="width">width (in pixels) of output buffer</param>
/// <param name="height">height (in pixels) of output buffer</param>
/// <returns>point in screen-space</returns>
D2D1_POINT_2F ProcessData::BodyToScreen(const CameraSpacePoint& bodyPoint, int width, int height)
{
	// Calculate the body's position on the screen
	DepthSpacePoint depthPoint = { 0 };
	mapper->MapCameraPointToDepthSpace(bodyPoint, &depthPoint);

	float screenPointX = static_cast<float>(depthPoint.X * width) / depthWidth;
	float screenPointY = static_cast<float>(depthPoint.Y * height) / depthHeight;

	return D2D1::Point2F(screenPointX, screenPointY);
}