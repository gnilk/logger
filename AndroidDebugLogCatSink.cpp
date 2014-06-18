#include <android/log.h>  

#include "logger.h"
#include "AndroidDebugLogCatSink.h"

#define ANDROID_TAG "LOGGER"

using namespace gnilk;

void AndroidDebugLogCatSink::Initialize(int argc, char **argv)
{
	argc;
	argv;
}

void AndroidDebugLogCatSink::WriteLine(int dbgLevel, char *hdr, char *string)
{
    __android_log_print(ANDROID_LOG_DEBUG, ANDROID_TAG, "%s%s", hdr, string);  

}

void AndroidDebugLogCatSink::Close()
{

}
