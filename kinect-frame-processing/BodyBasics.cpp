//------------------------------------------------------------------------------
// <copyright file="BodyBasics.cpp" company="Microsoft">
//     Copyright (c) Microsoft Corporation.  All rights reserved.
// </copyright>
//------------------------------------------------------------------------------

// 
// Configurar opencv: 
// https://docs.opencv.org/2.4/doc/tutorials/introduction/windows_visual_studio_Opencv/windows_visual_studio_Opencv.html
//

#include "stdafx.h"
#include <strsafe.h>

#include "resource.h"
#include "BodyBasics.h"
#include "KinectDrawer.h"

static const float c_JointThickness = 3.0f;
static const float c_TrackedBoneThickness = 6.0f;
static const float c_InferredBoneThickness = 1.0f;
static const float c_HandSize = 30.0f;

/// <summary>
/// Entry point for the application
/// </summary>
/// <param name="hInstance">handle to the application instance</param>
/// <param name="hPrevInstance">always 0</param>
/// <param name="lpCmdLine">command line arguments</param>
/// <param name="nCmdShow">whether to display minimized, maximized, or normally</param>
/// <returns>status</returns>
int APIENTRY wWinMain(    
	_In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR lpCmdLine,
    _In_ int nShowCmd
)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    CBodyBasics application;
    application.Run(hInstance, nShowCmd);
}

/// <summary>
/// Constructor
/// </summary>
CBodyBasics::CBodyBasics() :
    m_hWnd(NULL),
    m_nStartTime(0),
    m_nLastCounter(0),
    m_nFramesSinceUpdate(0),
    m_fFreq(0),
    m_nNextStatusTime(0LL),
    m_pKinectSensor(NULL),
    m_pCoordinateMapper(NULL),
    m_pBodyFrameReader(NULL),
	m_pDepthFrameReader(NULL),
	m_pColorFrameReader(NULL)
{
    LARGE_INTEGER qpf = {0};
    if (QueryPerformanceFrequency(&qpf))
    {
        m_fFreq = double(qpf.QuadPart);
    }
}
  

/// <summary>
/// Destructor
/// </summary>
CBodyBasics::~CBodyBasics()
{
    // clean up Direct2D
    SafeRelease(m_pD2DFactory);

    // done with body frame reader
    SafeRelease(m_pBodyFrameReader);

	// done with depth frame reader
	SafeRelease(m_pDepthFrameReader);

	//// done with color frame reader
	SafeRelease(m_pColorFrameReader);

    // done with coordinate mapper
    SafeRelease(m_pCoordinateMapper);

    // close the Kinect Sensor
    if (m_pKinectSensor)
    {
        m_pKinectSensor->Close();
    }

    SafeRelease(m_pKinectSensor);
}

/// <summary>
/// Creates the main window and begins processing
/// </summary>
/// <param name="hInstance">handle to the application instance</param>
/// <param name="nCmdShow">whether to display minimized, maximized, or normally</param>
int CBodyBasics::Run(HINSTANCE hInstance, int nCmdShow)
{
    MSG       msg = {0};
    WNDCLASS  wc;

    // Dialog custom window class
    ZeroMemory(&wc, sizeof(wc));
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.cbWndExtra    = DLGWINDOWEXTRA;
    wc.hCursor       = LoadCursorW(NULL, IDC_ARROW);
    wc.hIcon         = LoadIconW(hInstance, MAKEINTRESOURCE(IDI_APP));
    wc.lpfnWndProc   = DefDlgProcW;
    wc.lpszClassName = L"BodyBasicsAppDlgWndClass";

    if (!RegisterClassW(&wc))
    {
        return 0;
    }

    // Create main application window
    HWND hWndApp = CreateDialogParamW(
        NULL,
        MAKEINTRESOURCE(IDD_APP),
        NULL,
        (DLGPROC)CBodyBasics::MessageRouter, 
        reinterpret_cast<LPARAM>(this));

    // Show window
    ShowWindow(hWndApp, nCmdShow);

    // Main message loop
    while (WM_QUIT != msg.message)
    {
        Update();

        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE))
        {
            // If a dialog message will be taken care of by the dialog proc
            if (hWndApp && IsDialogMessageW(hWndApp, &msg))
            {
                continue;
            }

            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    return static_cast<int>(msg.wParam);
}

/// <summary>
/// Main processing function
/// </summary>
void CBodyBasics::Update()
{
    if (!m_pBodyFrameReader || !m_pDepthFrameReader || !m_pColorFrameReader)
    {
        return;
    }
	uint8* bgMask = nullptr;
	HRESULT hr, hr1, hrDepth, hrColor;
    IBodyFrame* pBodyFrame = NULL;
	IDepthFrame* pDepthFrame = NULL;
	IColorFrame* pColorFrame = NULL;
	IBodyIndexFrame* pBodyIndexFrame = NULL;

	int nColorWidth = 0;
	int nColorHeight = 0;
	RGBQUAD *pColorBuffer = NULL;

	
	hr = m_pBodyFrameReader->AcquireLatestFrame(&pBodyFrame);
	if (SUCCEEDED(hr))
	{
		processBodyFrame(hr, pBodyFrame);
	}

	hr = m_pBodyIndexFrameReader->AcquireLatestFrame(&pBodyIndexFrame);

	hrDepth = m_pDepthFrameReader->AcquireLatestFrame(&pDepthFrame);
	processDepthFrame(hrDepth, pDepthFrame);

	hrColor = m_pColorFrameReader->AcquireLatestFrame(&pColorFrame);
	processColorFrame(hrColor, pColorFrame, pColorBuffer, &nColorWidth, &nColorHeight);
	if (SUCCEEDED(hrDepth) && SUCCEEDED(hrColor))
	{
		hr = pDepthFrame->CopyFrameDataToArray(
			m_depthWidth * m_depthHeight, m_depthBuffer);
		if (FAILED(hr))
		{
			printf("Oh no! Failed map the depth frame to color space!\n");
			return;
		}

		hr = pColorFrame->CopyConvertedFrameDataToArray(
			1920 * 1080 * 4, (BYTE*)m_colorBuffer, ColorImageFormat_Bgra);
		if (FAILED(hr))
		{
			printf("Oh no! Failed map the depth frame to color space!\n");
			return;
		}

		hr = m_pCoordinateMapper->MapDepthFrameToColorSpace(
			m_depthWidth * m_depthHeight, m_depthBuffer,
			m_depthWidth * m_depthHeight, m_colorSpacePoints);
		if (FAILED(hr))
		{
			printf("Oh no! Failed map the depth frame to color space!\n");
			return;
		}

		if (SUCCEEDED(hr))
		{
			processBackground(hr, pBodyIndexFrame, pDepthFrame);

			processData->process(m_depthBuffer, m_colorBuffer, m_colorSpacePoints, backgroundRemoval->background_mask);
		}
	}
	



	SafeRelease(pColorFrame);
	SafeRelease(pDepthFrame);
    SafeRelease(pBodyFrame);
	SafeRelease(pBodyIndexFrame);
}

void CBodyBasics::processBackground(HRESULT hr, IBodyIndexFrame* pBodyIndexFrame, IDepthFrame* pDepthFrame)
{
	//, BYTE *pBodyIndexBuffer, int *nBodyIndexWidth, int *nBodyIndexHeight
	int nDepthWidth = 0;
	int nDepthHeight = 0;
	UINT16 *pDepthBuffer = NULL;
	UINT nDepthBufferSize = 0;

	int nBodyIndexWidth = 0;
	int nBodyIndexHeight = 0;
	BYTE *pBodyIndexBuffer = NULL;
	UINT nBodyIndexBufferSize = 0;

	IFrameDescription* pFrameDescription = NULL;

	if (!pBodyIndexFrame || !pDepthFrame)
	{
		return;
	}

	// get body index frame data
	if (SUCCEEDED(hr))
	{
		hr = pBodyIndexFrame->get_FrameDescription(&pFrameDescription);
	}

	if (SUCCEEDED(hr))
	{
		hr = pFrameDescription->get_Width(&nBodyIndexWidth);
	}

	if (SUCCEEDED(hr))
	{
		hr = pFrameDescription->get_Height(&nBodyIndexHeight);
	}

	if (SUCCEEDED(hr))
	{
		hr = pBodyIndexFrame->AccessUnderlyingBuffer(&nBodyIndexBufferSize, &pBodyIndexBuffer);
	}

	if (SUCCEEDED(hr))
	{
		hr = pDepthFrame->get_FrameDescription(&pFrameDescription);
	}

	if (SUCCEEDED(hr))
	{
		hr = pFrameDescription->get_Width(&nDepthWidth);
	}

	if (SUCCEEDED(hr))
	{
		hr = pFrameDescription->get_Height(&nDepthHeight);
	}

	if (SUCCEEDED(hr))
	{
		hr = pDepthFrame->AccessUnderlyingBuffer(&nDepthBufferSize, &pDepthBuffer);
	}


	SafeRelease(pFrameDescription);

	backgroundRemoval->ProcessFrame(pDepthBuffer, nDepthWidth, nDepthHeight,
		pBodyIndexBuffer, nBodyIndexWidth, nBodyIndexHeight);
}
void CBodyBasics::processBodyFrame(HRESULT hr, IBodyFrame* pBodyFrame)
{
	if (SUCCEEDED(hr))
	{
		INT64 nTime = 0;
		
		hr = pBodyFrame->get_RelativeTime(&nTime);

		IBody* ppBodies[BODY_COUNT] = { 0 };
		if (SUCCEEDED(hr))
		{
			hr = pBodyFrame->GetAndRefreshBodyData(_countof(ppBodies), ppBodies);
		}

		if (SUCCEEDED(hr))
		{
			ProcessBody(nTime, BODY_COUNT, ppBodies);
			processData->storeBodies(nTime, BODY_COUNT, ppBodies);
		}

		for (int i = 0; i < _countof(ppBodies); ++i)
		{
			SafeRelease(ppBodies[i]);
		}
	}
}


void CBodyBasics::processDepthFrame(HRESULT hr, IDepthFrame* pDepthFrame)
{
	if (SUCCEEDED(hr))
	{
		INT64 nTime = 0;
		IFrameDescription* pFrameDescription = NULL;
		USHORT nDepthMinReliableDistance = 0;
		USHORT nDepthMaxDistance = 0;
		UINT nBufferSize = 0;

		hr = pDepthFrame->get_RelativeTime(&nTime);

		if (SUCCEEDED(hr))
		{
			hr = pDepthFrame->get_FrameDescription(&pFrameDescription);
		}

		if (SUCCEEDED(hr))
		{
			hr = pDepthFrame->get_DepthMinReliableDistance(&nDepthMinReliableDistance);
		}

		if (SUCCEEDED(hr))
		{
			// In order to see the full range of depth (including the less reliable far field depth)
			// we are setting nDepthMaxDistance to the extreme potential depth threshold
			nDepthMaxDistance = USHRT_MAX;

			// Note:  If you wish to filter by reliable depth distance, uncomment the following line.
			//// hr = pDepthFrame->get_DepthMaxReliableDistance(&nDepthMaxDistance);
		}

		///*if (SUCCEEDED(hr))
		//{
		//	hr = pDepthFrame->AccessUnderlyingBuffer(&nBufferSize, &pBuffer);
		//}*/

		SafeRelease(pFrameDescription);
	}

}

void CBodyBasics::processColorFrame(HRESULT hr, IColorFrame* pColorFrame, RGBQUAD *pBuffer, int *nWidth, int *nHeight) {
	
	if (SUCCEEDED(hr))
	{
		INT64 nTime = 0;
		IFrameDescription* pFrameDescription = NULL;
		ColorImageFormat imageFormat = ColorImageFormat_None;
		UINT nBufferSize = 0;
		RGBQUAD *pBuffer = NULL;

		hr = pColorFrame->get_RelativeTime(&nTime);

		if (SUCCEEDED(hr))
		{
			hr = pColorFrame->get_FrameDescription(&pFrameDescription);
		}

		if (SUCCEEDED(hr))
		{
			hr = pFrameDescription->get_Width(nWidth);
		}

		if (SUCCEEDED(hr))
		{
			hr = pFrameDescription->get_Height(nHeight);
		}

		if (SUCCEEDED(hr))
		{
			hr = pColorFrame->get_RawColorImageFormat(&imageFormat);
		}

		if (SUCCEEDED(hr))
		{
			if (imageFormat == ColorImageFormat_Bgra)
			{
				hr = pColorFrame->AccessRawUnderlyingBuffer(&nBufferSize, reinterpret_cast<BYTE**>(&pBuffer));
			}
		/*	else if (m_pColorRGBX)
			{
				pBuffer = m_pColorRGBX;
				nBufferSize = cColorWidth * cColorHeight * sizeof(RGBQUAD);
				hr = pColorFrame->CopyConvertedFrameDataToArray(nBufferSize, reinterpret_cast<BYTE*>(pBuffer), ColorImageFormat_Bgra);
			}*/
			else
			{
				hr = E_FAIL;
			}

		}

		/*if (SUCCEEDED(hr))
		{
			ProcessColor(nTime, pBuffer, nWidth, nHeight);
		}*/

		SafeRelease(pFrameDescription);
	}
}


/// <summary>
/// Handles window messages, passes most to the class instance to handle
/// </summary>
/// <param name="hWnd">window message is for</param>
/// <param name="uMsg">message</param>
/// <param name="wParam">message data</param>
/// <param name="lParam">additional message data</param>
/// <returns>result of message processing</returns>
LRESULT CALLBACK CBodyBasics::MessageRouter(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CBodyBasics* pThis = NULL;
    
    if (WM_INITDIALOG == uMsg)
    {
        pThis = reinterpret_cast<CBodyBasics*>(lParam);
        SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
    }
    else
    {
        pThis = reinterpret_cast<CBodyBasics*>(::GetWindowLongPtr(hWnd, GWLP_USERDATA));
    }

    if (pThis)
    {
        return pThis->DlgProc(hWnd, uMsg, wParam, lParam);
    }

    return 0;
}

/// <summary>
/// Handle windows messages for the class instance
/// </summary>
/// <param name="hWnd">window message is for</param>
/// <param name="uMsg">message</param>
/// <param name="wParam">message data</param>
/// <param name="lParam">additional message data</param>
/// <returns>result of message processing</returns>
LRESULT CALLBACK CBodyBasics::DlgProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(wParam);
    UNREFERENCED_PARAMETER(lParam);

    switch (message)
    {
        case WM_INITDIALOG:
        {
            // Bind application window handle
            m_hWnd = hWnd;

            // Init Direct2D
            D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &m_pD2DFactory);

            // Get and initialize the default Kinect sensor
            InitializeDefaultSensor();
        }
        break;

        // If the titlebar X is clicked, destroy app
        case WM_CLOSE:
            DestroyWindow(hWnd);
            break;

        case WM_DESTROY:
            // Quit the main message pump
            PostQuitMessage(0);
            break;
    }

    return FALSE;
}

/// <summary>
/// Initializes the default Kinect sensor
/// </summary>
/// <returns>indicates success or failure</returns>
HRESULT CBodyBasics::InitializeDefaultSensor()
{
    HRESULT hr;

    hr = GetDefaultKinectSensor(&m_pKinectSensor);
    if (FAILED(hr))
    {
        return hr;
    }

    if (m_pKinectSensor)
    {
        // Initialize the Kinect and get coordinate mapper and the readers
        IBodyFrameSource* pBodyFrameSource = NULL;
		IBodyIndexFrameSource* pBodyIndexFrameSource = NULL;
		IDepthFrameSource* pDepthFrameSource = NULL;
		IColorFrameSource* pColorFrameSource = NULL;

        hr = m_pKinectSensor->Open();

        if (SUCCEEDED(hr))
        {
            hr = m_pKinectSensor->get_CoordinateMapper(&m_pCoordinateMapper);
        }

		// Initialize body frame reader
        if (SUCCEEDED(hr))
        {
            hr = m_pKinectSensor->get_BodyFrameSource(&pBodyFrameSource);
			
        }
	
		if (SUCCEEDED(hr))
		{
			hr = pBodyFrameSource->OpenReader(&m_pBodyFrameReader);
		}

		if (SUCCEEDED(hr))
		{
			hr = m_pKinectSensor->get_BodyIndexFrameSource(&pBodyIndexFrameSource);
		}

		if (SUCCEEDED(hr))
		{
			hr = pBodyIndexFrameSource->OpenReader(&m_pBodyIndexFrameReader);
		}

		// Initialize depth frame reader
		if (SUCCEEDED(hr))
		{
			hr = m_pKinectSensor->get_DepthFrameSource(&pDepthFrameSource);
		}

		if (SUCCEEDED(hr))
		{
			hr = pDepthFrameSource->OpenReader(&m_pDepthFrameReader);
		}

		// Initialize color frame reader
		if (SUCCEEDED(hr))
		{
			hr = m_pKinectSensor->get_ColorFrameSource(&pColorFrameSource);
		}

		if (SUCCEEDED(hr))
		{
			hr = pColorFrameSource->OpenReader(&m_pColorFrameReader);
		}

		//get depth frame description
		IFrameDescription *frameDesc;
		pDepthFrameSource->get_FrameDescription(&frameDesc);
		frameDesc->get_Width(&m_depthWidth);
		frameDesc->get_Height(&m_depthHeight);
		
		//allocate depth buffer
		m_depthBuffer = new uint16[m_depthWidth * m_depthHeight];

		//allocate color buffer
		m_colorBuffer = new uint32[1920 * 1080];
		//allocate a buffer of color space points
		m_colorSpacePoints = new ColorSpacePoint[m_depthWidth * m_depthHeight];

		processData = new ProcessData(m_pCoordinateMapper, m_depthWidth, m_depthHeight);
		backgroundRemoval = new BackgroundRemovalKinect(m_pCoordinateMapper);

		SafeRelease(pColorFrameSource);
		SafeRelease(pDepthFrameSource);
        SafeRelease(pBodyFrameSource);
		SafeRelease(pBodyIndexFrameSource);
    }

	

    if (!m_pKinectSensor || FAILED(hr))
    {
        SetStatusMessage(L"No ready Kinect found!", 10000, true);
        return E_FAIL;
    }

    return hr;
}

/// <summary>
/// Handle new body data
/// <param name="nTime">timestamp of frame</param>
/// <param name="nBodyCount">body data count</param>
/// <param name="ppBodies">body data in frame</param>
/// </summary>
void CBodyBasics::ProcessBody(INT64 nTime, int nBodyCount, IBody** ppBodies)
{
    if (m_hWnd)
    {
		KinectDrawer drawer = KinectDrawer();
        HRESULT hr = drawer.EnsureDirect2DResources(m_pD2DFactory, m_hWnd);
		if (FAILED(hr)) {
			SetStatusMessage(drawer.getMessage(), 10000, true);
		}
		
        if (SUCCEEDED(hr) && drawer.m_pRenderTarget && m_pCoordinateMapper)
        {
			drawer.m_pRenderTarget->BeginDraw();
			drawer.m_pRenderTarget->Clear();

            RECT rct;
            GetClientRect(GetDlgItem(m_hWnd, IDC_VIDEOVIEW), &rct);
            int width = rct.right;
            int height = rct.bottom;
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
                                jointPoints[j] = processData->BodyToScreen(joints[j].Position, width, height);
                            }

                            drawer.DrawBody(joints, jointPoints);

							drawer.DrawHand(leftHandState, jointPoints[JointType_HandLeft]);
                            drawer.DrawHand(rightHandState, jointPoints[JointType_HandRight]);
                        }
                    }
                }
            }

            hr = drawer.m_pRenderTarget->EndDraw();

            // Device lost, need to recreate the render target
            // We'll dispose it now and retry drawing
            if (D2DERR_RECREATE_TARGET == hr)
            {
                hr = S_OK;
                drawer.DiscardDirect2DResources();
            }
        }

        if (!m_nStartTime)
        {
            m_nStartTime = nTime;
        }

        double fps = 0.0;

        LARGE_INTEGER qpcNow = {0};
        if (m_fFreq)
        {
            if (QueryPerformanceCounter(&qpcNow))
            {
                if (m_nLastCounter)
                {
                    m_nFramesSinceUpdate++;
                    fps = m_fFreq * m_nFramesSinceUpdate / double(qpcNow.QuadPart - m_nLastCounter);
                }
            }
        }

        WCHAR szStatusMessage[64];
        StringCchPrintf(szStatusMessage, _countof(szStatusMessage), L" FPS = %0.2f    Time = %I64d", fps, (nTime - m_nStartTime));

        if (SetStatusMessage(szStatusMessage, 1000, false))
        {
            m_nLastCounter = qpcNow.QuadPart;
            m_nFramesSinceUpdate = 0;
        }
    }
}

/// <summary>
/// Set the status bar message
/// </summary>
/// <param name="szMessage">message to display</param>
/// <param name="showTimeMsec">time in milliseconds to ignore future status messages</param>
/// <param name="bForce">force status update</param>
bool CBodyBasics::SetStatusMessage(_In_z_ WCHAR* szMessage, DWORD nShowTimeMsec, bool bForce)
{
    INT64 now = GetTickCount64();

    if (m_hWnd && (bForce || (m_nNextStatusTime <= now)))
    {
        SetDlgItemText(m_hWnd, IDC_STATUS, szMessage);
        m_nNextStatusTime = now + nShowTimeMsec;

        return true;
    }

    return false;
}

