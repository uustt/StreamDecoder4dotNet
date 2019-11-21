#include "Tools.h"
#include <iostream>
extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

Tools::Tools(){}
Tools::~Tools(){}

char* Tools::av_strerror2(int errnum)
{
	if (logbuf == NULL) logbuf = new char[1024];
	memset(logbuf, 0, 1024);
	av_strerror(errnum, logbuf, 1023);
	return logbuf;
}


