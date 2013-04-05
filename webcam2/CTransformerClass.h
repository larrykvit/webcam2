/*
The ctransformer class .h and .cpp files were written by larry

The source.cpp contains a couple of lines written
by larry, but they were so that he could interface
the sample with his code. They have nothing of
value to mark

*/


#ifndef CTRANSFORMERCLASS_H_INCLUDED
#define CTRANSFORMERCLASS_H_INCLUDED

#include <vector>
#include "SerialClass.h"
#include <streams.h>

using namespace std;

class CTransformer : public CTransInPlaceFilter
{
public:
	CTransformer(	const TCHAR *pObjectName,
					LPUNKNOWN lpUnk,
					REFCLSID clsid,
					HRESULT *phr,
					char* cP,
					int w,
					int h,
					int cT,
					int sT );
	~CTransformer();

	HRESULT Transform( IMediaSample *pSample ) override;
	HRESULT CheckInputType( const CMediaType *mtIn ) override;
private:
	BYTE* refBuff;
	int nFrame;
	bool targetPrev;
	bool trgtAc;
	bool trgtLost;
	bool trgtNotThere;
	int lastSound;
	int * motionFrame;
	
	char* comPort;
	Serial* arduino;

	int colourThreshold;
	int sizeThreshold;

	struct Target
	{
		int sameAs;
		int xmax, xmin, ymax, ymin;
		int size;
		Target( int x, int y ) : xmax( x ), xmin( x ), ymax( y ), ymin( y ), size( 1 ), sameAs( -1 ) {}
	};

	//const int P_MOTION = -2;
	//const int P_NO_MOTION = -1;
	
	int pixelLeft( int i ){ return i - 1; };
	int pixelAbove( int i ){ return i - imageWidth - 1; };

	int xOf( int i ){ return i % (imageWidth + 1); };
	int yOf( int i ){ return ( i - xOf( i ) ) / ( imageWidth + 1 ); };

	vector <Target> targetList;
	vector <int> xBuffer;
	vector <int> yBuffer;
	int bufCount;

private:
	int imageWidth, imageHeight;
};

#endif