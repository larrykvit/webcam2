#include <stdio.h>

#include <initguid.h>

DEFINE_GUID(CLSID_DIFFilter, 
0x1915c5c8, 0x2aa, 0x415f, 0x89, 0xf, 0x76, 0xd9, 0x4c, 0x85, 0xaa, 0xf2);

#include "CTransformerClass.h"


int	main()
{
	// for playing
	IGraphBuilder *pGraphBuilder;
	ICaptureGraphBuilder2 *pCaptureGraphBuilder2;
	IMediaControl *pMediaControl;
	IBaseFilter *pDeviceFilter = NULL;

	// to select a video input device
	ICreateDevEnum *pCreateDevEnum = NULL;
	IEnumMoniker *pEnumMoniker = NULL;
	IMoniker *pMoniker = NULL;
	ULONG nFetched = 0;

	// initialize COM
	CoInitialize(NULL);

	//
	// selecting a device
	//

	// Create CreateDevEnum to list device
	CoCreateInstance(CLSID_SystemDeviceEnum, NULL, CLSCTX_INPROC_SERVER, 
		IID_ICreateDevEnum, (PVOID *)&pCreateDevEnum);

	// Create EnumMoniker to list VideoInputDevice 
	pCreateDevEnum->CreateClassEnumerator(CLSID_VideoInputDeviceCategory,
		&pEnumMoniker, 0);
	if (pEnumMoniker == NULL) {
		// this will be shown if there is no capture device
		printf("no device\n");
		return 0;
	}

	// reset EnumMoniker
	pEnumMoniker->Reset();

	// get each Moniker
	while (pEnumMoniker->Next(1, &pMoniker, &nFetched) == S_OK)
	{
		IPropertyBag *pPropertyBag;
		TCHAR devname[256];

		// bind to IPropertyBag
		pMoniker->BindToStorage(0, 0, IID_IPropertyBag,
			(void **)&pPropertyBag);

		VARIANT var;

		// get FriendlyName
		var.vt = VT_BSTR;
		pPropertyBag->Read(L"FriendlyName", &var, 0);
		WideCharToMultiByte(CP_ACP, 0,
			var.bstrVal, -1, devname, sizeof(devname), 0, 0);
		VariantClear(&var);

		printf("%s\r\n", devname);
		printf("  select this device ? [y] or [n]\r\n");
		int ch = getchar();

		// you can start playing by 'y' + return key
		// if you press the other key, it will not be played.
		if (ch == 'y')
		{
			// Bind Monkier to Filter
			pMoniker->BindToObject(0, 0, IID_IBaseFilter,
				(void**)&pDeviceFilter );
		}
		else
		{
			getchar();
		}

		// release
		pMoniker->Release();
		pPropertyBag->Release();

		if (pDeviceFilter != NULL)
		{
			// go out of loop if getchar() returns 'y'
			break;
		}
	}

	if (pDeviceFilter != NULL) {
		//
		// PLAY
		//

		// create FilterGraph
		CoCreateInstance(CLSID_FilterGraph,
			NULL,
			CLSCTX_INPROC,
			IID_IGraphBuilder,
			(LPVOID *)&pGraphBuilder);

		// create CaptureGraphBuilder2
		CoCreateInstance(CLSID_CaptureGraphBuilder2, NULL, CLSCTX_INPROC, 
			IID_ICaptureGraphBuilder2, 
			(LPVOID *)&pCaptureGraphBuilder2);

		//============================================================
		//===========  MY CODE  ======================================
		//=============================================================
		HRESULT hr = CoInitialize(0);
		IAMStreamConfig *pConfig = NULL;
		hr = pCaptureGraphBuilder2->FindInterface(&PIN_CATEGORY_CAPTURE, 0, pDeviceFilter, IID_IAMStreamConfig, (void**)&pConfig);

		int iCount = 0, iSize = 0;
		hr = pConfig->GetNumberOfCapabilities(&iCount, &iSize);

		int w = -1, h = -1;

		// Check the size to make sure we pass in the correct structure.
		if (iSize == sizeof(VIDEO_STREAM_CONFIG_CAPS))
		{
			// Use the video capabilities structure.

			for (int iFormat = 0; iFormat < iCount; iFormat++)
			{
				VIDEO_STREAM_CONFIG_CAPS scc;
				AM_MEDIA_TYPE *pmtConfig;
				hr = pConfig->GetStreamCaps(iFormat, &pmtConfig, (BYTE*)&scc);
				if (SUCCEEDED(hr))
				{
					// Examine the format, and possibly use it.
					if ((pmtConfig->majortype == MEDIATYPE_Video) &&
						(pmtConfig->bFixedSizeSamples) &&
						(pmtConfig->subtype == MEDIASUBTYPE_RGB24) &&
//						(pmtConfig->formattype == FORMAT_VideoInfo) &&
						(pmtConfig->cbFormat >= sizeof (VIDEOINFOHEADER)) &&
						(pmtConfig->pbFormat != NULL))
					{
						VIDEOINFOHEADER *pVih = (VIDEOINFOHEADER*)pmtConfig->pbFormat;
						// pVih contains the detailed format information.
						LONG lWidth = pVih->bmiHeader.biWidth;
						LONG lHeight = pVih->bmiHeader.biHeight;
						
						if( lWidth == 1280 )
							//					if (iFormat == 26)
						{ //2 = '1280x720YUV' YUV, 22 = '1280x800YUV', 26 = '1280x720RGB'
							hr = pConfig->SetFormat(pmtConfig);
							if (SUCCEEDED(hr))
								iFormat = iCount; // break
							w = lWidth;
							h = lHeight;
						}
					}
					// Delete the media type when you are done.
					DeleteMediaType(pmtConfig);
				}
			}
		}
/*
		// Query the capture filter for the IAMCameraControl interface.
		IAMCameraControl *pCameraControl = 0;
		hr = pDeviceFilter->QueryInterface(IID_IAMCameraControl, (void**)&pCameraControl);
		if (FAILED(hr))
		{
			// The device does not support IAMCameraControl
		}
		else
		{
			long Min, Max, Step, Default, Flags, Val;

			// Get the range and default values 
			hr = pCameraControl->GetRange(CameraControl_Exposure, &Min, &Max, &Step, &Default, &Flags);
			hr = pCameraControl->GetRange(CameraControl_Focus, &Min, &Max, &Step, &Default, &Flags);
			if (SUCCEEDED(hr))
			{
				hr = pCameraControl->Set(CameraControl_Exposure, -11, CameraControl_Flags_Manual ); // Min = -11, Max = 1, Step = 1
				hr = pCameraControl->Set(CameraControl_Focus, 12, CameraControl_Flags_Manual );
			}
		}

	
		// Query the capture filter for the IAMVideoProcAmp interface.
		IAMVideoProcAmp *pProcAmp = 0;
		hr = pDeviceFilter->QueryInterface(IID_IAMVideoProcAmp, (void**)&pProcAmp);
		if (FAILED(hr))
		{
			// The device does not support IAMVideoProcAmp
		}
		else
		{
			long Min, Max, Step, Default, Flags, Val;
			
			// Get the range and default values 
			hr = pProcAmp->GetRange(VideoProcAmp_Brightness, &Min, &Max, &Step, &Default, &Flags);
			hr = pProcAmp->GetRange(VideoProcAmp_BacklightCompensation, &Min, &Max, &Step, &Default, &Flags);
			hr = pProcAmp->GetRange(VideoProcAmp_Contrast, &Min, &Max, &Step, &Default, &Flags);
			hr = pProcAmp->GetRange(VideoProcAmp_Saturation, &Min, &Max, &Step, &Default, &Flags);
			hr = pProcAmp->GetRange(VideoProcAmp_Sharpness, &Min, &Max, &Step, &Default, &Flags);
			
			hr = pProcAmp->GetRange(VideoProcAmp_WhiteBalance, &Min, &Max, &Step, &Default, &Flags);
			if (SUCCEEDED(hr))
			{
				
				hr = pProcAmp->Set(VideoProcAmp_Brightness, 142, VideoProcAmp_Flags_Manual);
				hr = pProcAmp->Set(VideoProcAmp_BacklightCompensation, 0, VideoProcAmp_Flags_Manual);
				hr = pProcAmp->Set(VideoProcAmp_Contrast, 4, VideoProcAmp_Flags_Manual);
				hr = pProcAmp->Set(VideoProcAmp_Saturation, 100, VideoProcAmp_Flags_Manual);
				hr = pProcAmp->Set(VideoProcAmp_Sharpness, 0, VideoProcAmp_Flags_Manual);
				
				hr = pProcAmp->Set(VideoProcAmp_WhiteBalance, 6500, VideoProcAmp_Flags_Manual);
			}
		}
*/

		//============================================================
		//=========== END MY CODE  ======================================
		//=============================================================

		printf( "give me a com port, how about COM3?\n" );
		char comPort[256];
		scanf( "%s", comPort );
		int cT, sT;
		printf( "give me a colour threshold, how about 25?\n" );
		scanf( "%i", &cT );
		printf( "give me a size threshold, how about 1000?\n" );
		scanf( "%i", &sT );

		hr = S_OK;
		CTransformer* pTrans = new CTransformer( "Dif trans", 0, CLSID_DIFFilter, &hr, comPort, w, h, cT, sT );

		if( ! SUCCEEDED(hr) )
			::MessageBox( NULL, "2", "2", MB_OK );
//		IBaseFilter* ttt = 0;
//		trans->QueryInterface(IID_IBaseFilter, (LPVOID *)&ttt);
		// set FilterGraph
		hr = pCaptureGraphBuilder2->SetFiltergraph(pGraphBuilder);

		// get MediaControl interface
		hr = pGraphBuilder->QueryInterface(IID_IMediaControl, (LPVOID *)&pMediaControl);

		// add device filter to FilterGraph
		hr = pGraphBuilder->AddFilter(pTrans, L"Dif trans");
		if( ! SUCCEEDED(hr) )
			::MessageBox( NULL, "1", "1", MB_OK );
		hr = pGraphBuilder->AddFilter(pDeviceFilter, L"Device Filter");

		// create Graph
		hr = pCaptureGraphBuilder2->RenderStream(&PIN_CATEGORY_CAPTURE, &MEDIATYPE_Video, pDeviceFilter, pTrans, NULL);
		if( ! SUCCEEDED(hr) )
			::MessageBox( NULL, "4", "4", MB_OK );

		// start playing
		hr = pMediaControl->Run();

		// to block execution
		// without this messagebox, the graph will be stopped immediately
		MessageBox(NULL,
			"Block Execution",
			"Block",
			MB_OK);

		// release
		pMediaControl->Release();
		pCaptureGraphBuilder2->Release();
		pGraphBuilder->Release();
	}

	// release
	pEnumMoniker->Release();
	pCreateDevEnum->Release();

	// finalize COM
	CoUninitialize();

	return 0;
}