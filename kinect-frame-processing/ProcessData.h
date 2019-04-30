#pragma once

// Kinect Header files
#include <Kinect.h>

// Direct2D Header Files
#include <d2d1.h>

//some useful typedefs for explicit type sizes
typedef unsigned char uint8;
typedef unsigned short uint16;
typedef unsigned int uint32;
typedef unsigned long long uint64;
typedef char int8;
typedef short int16;
typedef int int32;
typedef long long int64;

class ProcessData
{
public:
	ProcessData(ICoordinateMapper* mapper, int wDepth, int hDepth);
	
	~ProcessData();

	void ProcessData::process(uint16 * depthBuffer, uint32* colorBuffer, ColorSpacePoint* m_colorSpacePoints, uint8* bgMask);

	void ProcessData::storeBodies(INT64 nTime, int nBodyCount, IBody** ppBodies);

	D2D1_POINT_2F ProcessData::BodyToScreen(const CameraSpacePoint& bodyPoint, int width, int height);

private:
	int depthWidth = 0, depthHeight = 0;
	int nFrame;

	ICoordinateMapper*      mapper;

	void manageDepthFrame(uint16* depthBuffer);
	void manageColorFrame(uint32* colorBuffer, ColorSpacePoint* m_colorSpacePoints);
	void saveBackgroundMask(uint8 * bgMask);
};

