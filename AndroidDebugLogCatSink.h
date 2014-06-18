#ifndef __ANDROID_DEBUG_LOG_SINK_H__
#define __ANDROID_DEBUG_LOG_SINK_H__
#include "logger.h"

namespace gnilk
{ 
	class AndroidDebugLogCatSink : public LogBaseSink
	{
	public:
		virtual void Initialize(int argc, char **argv);
		virtual void WriteLine(int dbgLevel, char *hdr, char *string);
		virtual void Close();
	};
}

#endif