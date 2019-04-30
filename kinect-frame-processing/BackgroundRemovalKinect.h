#pragma once

#include "stdafx.h"
class BackgroundRemovalKinect
{
	static const int        cDepthWidth = 512;
	static const int        cDepthHeight = 424;
	static const int        cColorWidth = 1920;
	static const int        cColorHeight = 1080;
public:
	
	UINT8 * background_mask;

	BackgroundRemovalKinect(ICoordinateMapper* mapper);
	~BackgroundRemovalKinect();

	void BackgroundRemovalKinect::ProcessFrame(
		unsigned short* pDepthBuffer, int nDepthWidth, int nDepthHeight,
		BYTE* pBodyIndexBuffer, int nBodyIndexWidth, int nBodyIndexHeight);

private:
	RGBQUAD*				m_pBackgroundRGBX;
	RGBQUAD*                m_pOutputRGBX;
	RGBQUAD*                m_pColorRGBX;
	DepthSpacePoint*		m_pDepthCoordinates;
	ICoordinateMapper*      mapper;
};

