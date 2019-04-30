#include "BackgroundRemovalKinect.h"


#include <limits>

BackgroundRemovalKinect::BackgroundRemovalKinect(ICoordinateMapper* mapper)
{
	const RGBQUAD c_green = { 0, 255, 0 };

	// create heap storage for composite image pixel data in RGBX format
	m_pOutputRGBX = new RGBQUAD[cColorWidth * cColorHeight];

	// create heap storage for background image pixel data in RGBX format
	m_pBackgroundRGBX = new RGBQUAD[cColorWidth * cColorHeight];

	// create heap storage for color pixel data in RGBX format
	m_pColorRGBX = new RGBQUAD[cColorWidth * cColorHeight];

	// create heap storage for the coorinate mapping from color to depth
	m_pDepthCoordinates = new DepthSpacePoint[cColorWidth * cColorHeight];

	background_mask = new UINT8[cDepthWidth * cDepthHeight];

	// Fill in with a background colour of green if we can't load the background image
	for (int i = 0; i < cColorWidth * cColorHeight; ++i)
	{
		m_pBackgroundRGBX[i] = c_green;
	}
	this->mapper = mapper;

}



/// <summary>
/// Handle new depth and color data
/// <param name="pDepthBuffer">pointer to depth frame data</param>
/// <param name="nDepthWidth">width (in pixels) of input depth image data</param>
/// <param name="nDepthHeight">height (in pixels) of input depth image data</param>
/// <param name="pBodyIndexBuffer">pointer to body index frame data</param>
/// <param name="nBodyIndexWidth">width (in pixels) of input body index data</param>
/// <param name="nBodyIndexHeight">height (in pixels) of input body index data</param>
/// </summary>
void BackgroundRemovalKinect::ProcessFrame(
	unsigned short* pDepthBuffer, int nDepthWidth, int nDepthHeight,
	BYTE* pBodyIndexBuffer, int nBodyIndexWidth, int nBodyIndexHeight)
{
	memset(background_mask, 255, cDepthWidth * cDepthHeight);

	// Make sure we've received valid data
	if (mapper && m_pDepthCoordinates && m_pOutputRGBX &&
		pDepthBuffer && (nDepthWidth == cDepthWidth) && (nDepthHeight == cDepthHeight) &&
		pBodyIndexBuffer && (nBodyIndexWidth == cDepthWidth) && (nBodyIndexHeight == cDepthHeight))
	{
		HRESULT hr = mapper->MapColorFrameToDepthSpace(nDepthWidth * nDepthHeight, (UINT16*)pDepthBuffer, cColorWidth * cColorHeight, m_pDepthCoordinates);
		if (SUCCEEDED(hr))
		{
			// loop over output pixels
			for (int colorIndex = 0; colorIndex < (cColorWidth * cColorHeight); ++colorIndex)
			{
				// default setting source to copy from the background pixel
				const RGBQUAD* pSrc = m_pBackgroundRGBX + colorIndex;

				DepthSpacePoint p = m_pDepthCoordinates[colorIndex];

				// Values that are negative infinity means it is an invalid color to depth mapping so we
				// skip processing for this pixel
				if (p.X != -std::numeric_limits<float>::infinity() && p.Y != -std::numeric_limits<float>::infinity())
				{
					int depthX = static_cast<int>(p.X + 0.5f);
					int depthY = static_cast<int>(p.Y + 0.5f);

					if ((depthX >= 0 && depthX < nDepthWidth) && (depthY >= 0 && depthY < nDepthHeight))
					{
						BYTE player = pBodyIndexBuffer[depthX + (depthY * cDepthWidth)];

						// if we're tracking a player for the current pixel, draw from the color camera
						if (player != 0xff)
						{
							// set source for copy to the color pixel
							pSrc = m_pColorRGBX + colorIndex;
							background_mask[depthX + depthY*cDepthWidth] = 0;
						}
					}
				}

				// write output
				m_pOutputRGBX[colorIndex] = *pSrc;
			}

		}
	}
}

BackgroundRemovalKinect::~BackgroundRemovalKinect()
{
	if (m_pOutputRGBX)
	{
		delete[] m_pOutputRGBX;
		m_pOutputRGBX = NULL;
	}

	if (m_pBackgroundRGBX)
	{
		delete[] m_pBackgroundRGBX;
		m_pBackgroundRGBX = NULL;
	}

	if (m_pColorRGBX)
	{
		delete[] m_pColorRGBX;
		m_pColorRGBX = NULL;
	}

	if (m_pDepthCoordinates)
	{
		delete[] m_pDepthCoordinates;
		m_pDepthCoordinates = NULL;
	}
}
