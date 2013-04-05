#include "CTransformerClass.h"

CTransformer::CTransformer( const TCHAR *pObjectName,
							LPUNKNOWN lpUnk,
							REFCLSID clsid,
							HRESULT *phr,
							char* cP,
							int w,
							int h,
							int cT,
							int sT )
							: CTransInPlaceFilter( pObjectName, lpUnk, clsid, phr, TRUE )
{
	nFrame = 0;
	lastSound = 0;
	targetPrev = false;
	motionFrame = NULL;

	comPort = cP;
	arduino = new Serial( cP );

	imageWidth = w;
	imageHeight = h;

	colourThreshold = cT;
	sizeThreshold = sT;

	trgtAc = false;
	trgtLost = false;
	trgtNotThere = false;

	targetList.reserve( 40000 );

	bufCount = 0;
	xBuffer.resize(10);
	yBuffer.resize(10);

	refBuff = 0;
}

CTransformer::~CTransformer()
{
	delete motionFrame;
	delete arduino;
}

HRESULT CTransformer::Transform( IMediaSample *pSample )
{
	BYTE* buff;
	pSample->GetPointer( &buff );
	long len = pSample->GetActualDataLength();
	
	const int P_MOTION = -2;
	const int P_NO_MOTION = -1;
	
	nFrame++;

	if( NULL == motionFrame )
	{
		//motionFrame will be filled with motion pixels
		//either -1 if motion or 0 if not motion
		// it is one bigger to the top simplify code later on
		motionFrame = new int[ (imageHeight + 1) * (imageWidth + 1) ];

		//fills the top row with no motion
		for( int i = 0; i <= imageWidth + 1; i++ )
			motionFrame[ i ] = P_NO_MOTION;
		for( int i = 0; i < imageHeight; i ++ )
			motionFrame[ (i+1) * (imageWidth + 1) ] = P_NO_MOTION;
	}
	if( nFrame < 200 )
	{
		if( refBuff == 0 )
			refBuff = new BYTE[ len ];

		memcpy( refBuff, buff, len );
		buff[5] = 0xff;
	}
	else
	{
		//this is used to offset the imgae, since the top and left border should be left blank
		int count = imageWidth + 2;
		int maxTarget = -1;
		//this fills up the motion frame array
		for( DWORD i = len / 3 - 1; i < len/3 ; i -- )
		{
			//if its an edge pixel, skip over it
			if( xOf( count ) == 0 )
				count++;

			int cutOff = 5;

			if( xOf( count ) < cutOff ||
				yOf( count ) < cutOff ||
				xOf( count ) > imageWidth - cutOff ||
				yOf( count ) > imageHeight - cutOff )
			{
				buff[ i*3 + 0 ] = 0xff;
				buff[ i*3 + 1 ] = 0xff;
				buff[ i*3 + 2 ] = 0xff;
				motionFrame[ count ] = P_NO_MOTION;
			}

			//checks to see if the pixel is a motion pixel
			else if(abs( buff[ i*3 + 0 ] - refBuff[i*3 + 0] ) > colourThreshold ||
					abs( buff[ i*3 + 1 ] - refBuff[i*3 + 1] ) > colourThreshold ||
					abs( buff[ i*3 + 2 ] - refBuff[i*3 + 2] ) > colourThreshold )
			{
				motionFrame[ count ] = P_MOTION;
				int i = count;
				//target detection happens bellow
		
				//processing the grouped targets is below				

				//case 1: no motin pixels above, or to the left
				if( P_NO_MOTION == motionFrame[ pixelLeft( i ) ] && P_NO_MOTION == motionFrame[ pixelAbove( i ) ] )
				{
					maxTarget++;

					motionFrame[ i ] = maxTarget;

					targetList.push_back( Target( xOf( i ), yOf( i ) ) );
				}
				//case 2a: the pixel above is not a motion pixel, thus pixel to the left is a motion pixel
				else if( P_NO_MOTION == motionFrame[ pixelAbove( i ) ] )
				{
					motionFrame[ i ] = motionFrame[ pixelLeft( i ) ];
					targetList[ motionFrame[ i ] ].size++;

					//check if the new pixel is out of the bounds, and if so, set new bounds
					//for this case, only xmax needs to be checked

					if( xOf( i ) > targetList[ motionFrame[ i ] ].xmax )
						targetList[ motionFrame[ i ] ].xmax = xOf( i );
				}
				//case 2b: the pixel to the left is not a motion pixel, thus the pixel above is a motion pixel
				else if( P_NO_MOTION == motionFrame[ pixelLeft( i ) ] )
				{
					motionFrame[ i ] = motionFrame[ pixelAbove( i ) ];
					targetList[ motionFrame[ i ] ].size++;

					//check if the new pixel is out of the bounds, and if so, set new bounds
					//for this case, only ymax needs to be checked

					if( yOf( i ) > targetList[ motionFrame[ i ] ].ymax )
						targetList[ motionFrame[ i ] ].ymax = yOf( i );
				}
				//case 3: both pixels are motion pixels, and they need to be grouped
				else
				{
					//first check if they are the same
					if( motionFrame[ pixelAbove( i ) ] == motionFrame[ pixelLeft( i ) ] )
					{
						motionFrame[ i ] = motionFrame[ pixelLeft( i ) ];// pixel left choosen arbitrarily
						targetList[ motionFrame[ i ] ].size++;
					}
					else
					{
						int smalli = pixelAbove( i );
						int bigi = pixelLeft( i );

						if( motionFrame[ pixelAbove( i ) ] > motionFrame[ pixelLeft( i ) ] )
						{
							smalli = pixelLeft( i );
							bigi = pixelAbove( i );
						}
						//always choose the target with the smallest index
						motionFrame[ i ] = motionFrame[ smalli ];
						targetList[ motionFrame[ i ] ].size++;

						//same as will come into play here
						//targets will be grouped here
						//work in progress

						if( -1 == targetList[ motionFrame[ bigi ] ].sameAs )
						{
							//this target has no same as, can be given one
							if( -1 == targetList[ motionFrame[ smalli ] ].sameAs )
							{
								//means that the target is is touching is not related to any other target
								//this means that this target (bigi) is related to (smalli)
								targetList[ motionFrame[ bigi ] ].sameAs = motionFrame[ smalli ];
							}
							else
							{
								targetList[ motionFrame[ bigi ] ].sameAs = targetList[ motionFrame[ smalli ] ].sameAs;
							}
						}
						else
						{
							//this means that that the target already has a related target
							//we have to make the bigger related target related to the smaller related target
							//makes sense?
							if( targetList[ motionFrame[ bigi ] ].sameAs > targetList[ motionFrame[ smalli ] ].sameAs )
							{
								targetList[ targetList[ motionFrame[ bigi ] ].sameAs ].sameAs = targetList[ motionFrame[ smalli ] ].sameAs;
								//the next line is not really needed, but just to simpify just in case
								targetList[ motionFrame[ bigi ] ].sameAs = targetList[ motionFrame[ smalli ] ].sameAs;
							}
							else if( targetList[ motionFrame[ bigi ] ].sameAs < targetList[ motionFrame[ smalli ] ].sameAs )
							{
								targetList[ targetList[ motionFrame[ smalli ] ].sameAs ].sameAs = targetList[ motionFrame[ bigi ] ].sameAs;

								targetList[ motionFrame[ smalli ] ].sameAs = targetList[ motionFrame[ bigi ] ].sameAs;
							}
						}
						targetList[ motionFrame[ bigi ] ].sameAs = motionFrame[ smalli ];
					}
				}
			}
			else
			{
				buff[ i*3 + 0 ] = 0xff;
				buff[ i*3 + 1 ] = 0xff;
				buff[ i*3 + 2 ] = 0xff;
				motionFrame[ count ] = P_NO_MOTION;
			}
			count++;
		}
	

		//now motionFrame is filled with motion pixels and targets
		
		//now that i have all the targets, i need to add up the sizes of the related targets
		bool targetCur = false;
		if( ! (targetList.size() == 0) )
		{
						
			int maxSize = targetList[ targetList.size() - 1 ].size;
			int index = targetList.size() - 1;
			for( int i = targetList.size() - 1; i >= 0; i-- )
			{
				if( -1 == targetList[i].sameAs )
				{
					//check if its the biggest
					if( targetList[ i ].size > maxSize )
					{
						maxSize = targetList[ i ].size;
						index = i;
					}
				}
				else
				{
					targetList[ targetList[i].sameAs ].size += targetList[ i ].size;
					//maxs
					if( targetList[ targetList[i].sameAs ].xmax < targetList[ i ].xmax )
						targetList[ targetList[i].sameAs ].xmax = targetList[ i ].xmax;
					if( targetList[ targetList[i].sameAs ].ymax < targetList[ i ].ymax )
						targetList[ targetList[i].sameAs ].ymax = targetList[ i ].ymax;
					//mins
					if( targetList[ targetList[i].sameAs ].xmin > targetList[ i ].xmin )
						targetList[ targetList[i].sameAs ].xmin = targetList[ i ].xmin;
					if( targetList[ targetList[i].sameAs ].ymin > targetList[ i ].ymin )
						targetList[ targetList[i].sameAs ].ymin = targetList[ i ].ymin;
				}
			}

						
			if( maxSize > sizeThreshold )
			{
				targetCur = true;
							
				int x1 = imageWidth - (targetList[ index ].xmin);
				int x2 = imageWidth - (targetList[ index ].xmax);	
				int xm = (x1+x2)/2;
				int y1 = imageHeight - targetList[ index ].ymin;
				int y2 = imageHeight - targetList[ index ].ymax;
				int ym = (y1+y2)/2;

				
				//smooth the stuff
				xBuffer[bufCount] = xm;
				yBuffer[bufCount] = ym;
				//int xo = xm;
				//int yo = ym;
				int sum = 0;
				int i;
				for( i = 0; i < xBuffer.size(); i++ )
				{
					sum += xBuffer[i];
				}
				sum /= i;
				xm = sum;

				sum = 0;
				for( i = 0; i < yBuffer.size(); i++ )
				{
					sum += yBuffer[i];
				}
				sum /= i;
				ym = sum;

				bufCount++;
				bufCount %= 10;
				
				char data[256];
				sprintf( data, "T%iX%.4iY%.4i", 1, xm, ym );
				arduino->WriteData( data, strlen( data ) );


				for( int i = 0; i < imageHeight; i++ )
				{
					buff[ (x1 + i* imageWidth)*3 + 0 ] = 0;
					buff[ (x1 + i* imageWidth)*3 + 1 ] = 0;
					buff[ (x1 + i* imageWidth)*3 + 2 ] = 0xff;

					buff[ (xm + i* imageWidth)*3 + 0 ] = 0;
					buff[ (xm + i* imageWidth)*3 + 1 ] = 0;
					buff[ (xm + i* imageWidth)*3 + 2 ] = 0;

					buff[ (x2 + i* imageWidth)*3 + 0 ] = 0;
					buff[ (x2 + i* imageWidth)*3 + 1 ] = 0xff;
					buff[ (x2 + i* imageWidth)*3 + 2 ] = 0;
				}
				for( int i = 0; i < imageWidth; i++ )
				{
					buff[ ((y1)*imageWidth + i)*3 + 0 ] = 0xff;
					buff[ ((y1)*imageWidth + i)*3 + 1 ] = 0;
					buff[ ((y1)*imageWidth + i)*3 + 2 ] = 0;

					buff[ ((ym)*imageWidth + i)*3 + 0 ] = 0;
					buff[ ((ym)*imageWidth + i)*3 + 1 ] = 0;
					buff[ ((ym)*imageWidth + i)*3 + 2 ] = 0;

					buff[ ((y2)*imageWidth + i)*3 + 0 ] = 0xff;
					buff[ ((y2)*imageWidth + i)*3 + 1 ] = 0;
					buff[ ((y2)*imageWidth + i)*3 + 2 ] = 0xff;
				}
			}

			targetList.clear();
		}

		if( !targetCur )
		{
			//send arduino that no target is present
			char data[256];
			sprintf( data, "T%iX%.4iY%.4i", 0, 0, 0 );
			arduino->WriteData( data, strlen( data ) );

		}
		

		if( false == targetPrev && true == targetCur )
		{
			trgtAc = true;
			//trgtLost = false;
			//trgtNotThere = false;
		}
		else if( true == targetPrev && false == targetCur )
		{
			//trgtAc = false;
			trgtLost = true;
			//trgtNotThere = false;
		}
		else if( false == targetPrev && false == targetCur )
		{
			//trgtAc = false;
			//trgtLost = false;
			trgtNotThere = true;
		}

		if( trgtLost && trgtAc )
		{
			trgtAc = false;
			trgtLost = false;
		}

		//play sound
		if( lastSound > 30 )
		{
			
			if( trgtAc )
			{
				//play target aquired
				PlaySound( TEXT("C:\\SOUND\\turret_active_2.wav"), NULL, SND_ASYNC );
				//PlaySound( TEXT("C:\\SOUND\\turret_active_2.wav"), NULL, SND_ASYNC );
				trgtAc = false;
				trgtNotThere = false;
				lastSound = 0;
			}
			else if( trgtLost )
			{
				//play target lost
				PlaySound( TEXT("C:\\sound\\turret_search_2.wav"), NULL, SND_ASYNC );
				trgtLost = false;
				trgtNotThere = false;
				lastSound = 0;
			}
			else if( trgtNotThere && lastSound > 300 )
			{
				//play are you still there
				PlaySound( TEXT("C:\\sound\\turret_search_1.wav"), NULL, SND_ASYNC );
				trgtNotThere = false;
				lastSound = 0;
			}
			else
				lastSound++;
		}
		else
			lastSound++;
		targetPrev = targetCur;
	}
	
	return S_OK;
}

HRESULT CTransformer::CheckInputType( const CMediaType *mtIn )
{
	if (mtIn->majortype != MEDIATYPE_Video)
		return VFW_E_TYPE_NOT_ACCEPTED;

//	VIDEOINFOHEADER *pVih = reinterpret_cast<VIDEOINFOHEADER*>(mtIn->pbFormat);
	return S_OK;
}
