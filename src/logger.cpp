/*-------------------------------------------------------------------------
 Author  : $Author: $
 Version : $Revision: $
 Orginal : 2006-07-26, 15:50
 Descr   : Tiny Log4Net look-alike for C++ 

	Use like:
			gnilk::ILogger *pLog = Logger::GetLogger("main");
			pLog->Debug("Hello\n");

	Console output (sink) is enabled by default if compiling with _DEBUG or DEBUG.			

	NOTE: Not thread safe by default, and I would not bet on it even with pthreads - it was never the intention..

	Compile options
		LOGGER_HAVE_NEWLINE, will append newline to each output before sending to sinks
		LOGGER_HAVE_SERIAL, will compile the Arduino Serial sink
		LOGGER_HAVE_PTHREADS, will use pthread_mutex to lock

   On Windows (WIN32) Critical Sections are created by default and there is not need to use 'HAVE_PTHREADS'

 
 Layout can be changed in: Logger::WriteReportString
 
 Comparision in output is '>=' means, setting debug level filtering to 
 INFO gives INFO and everything above..
 
 Levels:
   0..99  - none,
 100..199 - debug,
 200..299 - info,
 300..399 - warning
 400..499 - error
 500..599 - crtical
 600..X   - custom
 
 Modified: $Date: $ by $Author: $
 ---------------------------------------------------------------------------
 TODO: [ -:Not done, +:In progress, !:Completed]
 <pre>
   ! Port thread saftey stuff to pthread's as well
   ! Reintroduce 'IsPrefixEnabled' as we can have auto-split separate from 'IsPrefixEnabled'
     AutoSplit SHOULD enable prefix printing.
   - Possibility to set prefix splitter operator
   ! Level filtering [sink levels]
   ! Global properties [early filtering]
   ! Introduce more debug levels to reduce noise
   - Support for module exclusion/inclusion lists
   ! Rolling file appender would be nice!
   ! Support for threading (Windows only for now)
   - Unicode support...
   - Refactor the configuration handling to optional free-standing 'LogManager' class
 </pre>
 
 
 \History
 - 11.05.2020, gnilk, Removed most warnings, added pthread support
 - 09.04.2019, gnilk, Autosplitting prefix feature
 - 01.04.2019, gnilk, Support for prefix when fetching logger
 - 24.01.2019, gnilk, Corrected bug in timestamp's month was '-1'
 - 14.09.2018, gnilk, Imported to Pucko and made it Arduino compatible
 - 25.10.2010, gnilk, Property handling and from-file-initialization
 - 25.10.2010, gnilk, Simple rolling file appender
 - 21.10.2010, gnilk, Changed messages classes to be ranges instead of fixed
 - 20.10.2010, gnilk, Refactored the creation of report strings
 - 19.10.2010, gnilk, Support for sink management
 - 02.04.2009, gnilk, Support for sink levels
 - 23.03.2009, gnilk, Implementation

 ---------------------------------------------------------------------------*/
#ifdef WIN32
//#include "stdafx.h"
#endif


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#ifdef WIN32
#include <windows.h>
#include <time.h>
#include <winsock.h> // struct timeval

#define strdup _strdup	// bla,bla

#if defined(_MSC_VER) || defined(_MSC_EXTENSIONS)
#define DELTA_EPOCH_IN_MICROSECS  11644473600000000Ui64
#else
#define DELTA_EPOCH_IN_MICROSECS  11644473600000000ULL
#endif

#define snprintf _snprintf
#define vsnprintf _vsnprintf

#else
#include <pthread.h>
#include <sys/time.h>
#endif

#include <list>
#include <map>


#include "logger.h"
#include "logger_internal.h"


#define DEFAULT_DEBUG_LEVEL 0		// used by constructors, default is output everything
#define DEFAULT_SINK_NAME ("")
#define DEFAULT_LOGFILE_NAME ("logfile")
#define DEFAULT_BUFFER_SIZE 4096

//#define DEBUG 1

using namespace std;

namespace gnilk 
{
	static int StrExplode(std::vector<std::string> *strList, char *mString, int chrSplit);
	static char *StrTrim(char *s);
	extern "C" {
		ILogOutputSink * LOG_CALLCONV CreateSink(const char *className) {
			return NULL;
		}
	}

	// Loggers currently available
	static LOG_SINK_FACTORY sinkFactoryList[] =
	{
		"LogConsoleSink", LogConsoleSink::CreateInstance,
		"LogRollingFileSink", LogRollingFileSink::CreateInstance,
		"LogFileSink", LogFileSink::CreateInstance,
#if defined(LOGGER_HAVE_SERIAL)		
		"LogSerialSink", LogSerialSink::CreateInstance,
#endif		
		"", NULL,
	};



// --------------------------------------------------------------------------
//
// Serial sink
// outputs all debug messages to the serial port
//

#ifdef LOGGER_HAVE_SERIAL
void LogSerialSink::Initialize(int argc, char **argv) {

}
int LogSerialSink::WriteLine(int dbgLevel, char *hdr, char *string) {
	int res = SINK_WRITE_FILTERED;
	if (WithinRange(dbgLevel))
	{
		res = (int)Serial.print(hdr);
		res += (int)Serial.print(string);
		// res = fprintf(stdout, "%s%s\n", hdr,string);
		// if (res < 0) {
		// 	res = SINK_WRITE_IO_ERROR;
		// }
	}
	return res;
}
void LogSerialSink::Close() {
	// nothing 
}

ILogOutputSink * LOG_CALLCONV LogSerialSink::CreateInstance() {
	return (ILogOutputSink *)(new LogSerialSink());
}
#endif

//
// Console sink
// outputs all debug messages to the console
//
ILogOutputSink *LogConsoleSink::CreateInstance()
{
	return (ILogOutputSink *)(new LogConsoleSink());
}

void LogConsoleSink::Initialize(int argc, char **argv)
{
	// Do stuff here
	int i;
	for (i=0;i<argc;i++)
	{
		if (!strcmp(argv[i],"filename"))
		{
			// 
		}
	}
}
int LogConsoleSink::WriteLine(int dbgLevel, char *hdr, char *string)
{
	int res = SINK_WRITE_FILTERED;
	if (WithinRange(dbgLevel))
	{ 
		res = fprintf(stdout, "%s%s", hdr,string);
		if (res < 0) {
			res = SINK_WRITE_IO_ERROR;
		}
	}
	return res;
}
void LogConsoleSink::Close()
{
	// close file here
}

// --------------------------------------------------------------------------
//
// Simple file sink
//
LogFileSink::LogFileSink()
{
	fOut = NULL;	
}
LogFileSink::~LogFileSink()
{
	if (fOut != NULL) {
		Close();
	}
}

ILogOutputSink *LogFileSink::CreateInstance()
{
	return (ILogOutputSink *)(new LogFileSink());
}

void LogFileSink::ParseArgs(int argc, char **argv)
{
	int i;
	
	for (i=0;i<argc;i++)
	{
		if (!strcmp(argv[i],"file"))
		{
			this->properties.SetValue(LOG_CONF_LOGFILE, argv[++i]);
		}
	}	
}
void LogFileSink::Initialize(int argc, char **argv)
{
	ParseArgs(argc, argv);
	Open(properties.GetLogfileName(), false);
}
long LogFileSink::Size()
{
	if (fOut != NULL)
	{
#if defined(_WIN32) && (_MSC_VER>0x1310)
        return _filelengthi64(_fileno(f));
#else
        long offset,length;
        offset = ftell(fOut);
        if (offset < 0)
            return -1;
        if (fseek(fOut, 0, SEEK_END) != 0)
            return -1;
        length = ftell(fOut);
        if (fseek(fOut, offset, SEEK_SET) != 0)
            return -1;
        return length;
#endif

	}
	return -1;
}

void LogFileSink::Open(const char *filename, bool bAppend)
{
	if (filename != NULL)
	{
		if (bAppend) {
#ifdef DEBUG
	//Serial.printf("LogFileSink::Open, open with append\n");
#endif			
			fOut = fopen(filename,"a");	
		} else
		{
#ifdef DEBUG
	//Serial.printf("LogFileSink::Open, open with overwrite\n");
#endif			
			fOut = fopen(filename,"w");	
		}

#ifdef DEBUG
		if (fOut == NULL) {
			//Serial.printf("LogFileSink::Open, failed to open file - did you mount a file system????\n");
		}
#endif		
	}

}

int LogFileSink::WriteLine(int dbgLevel, char *hdr, char *string)
{
#ifdef DEBUG	
//	Serial.printf("LogFileSink::WriteLine, called with string: %s\n", string);
#endif
	int res = SINK_WRITE_FILTERED;
	if (fOut != NULL)
	{
		if (WithinRange(dbgLevel))
		{
			#ifdef DEBUG
			//Serial.printf("LogFileSink::WriteLine, Writing to file\n");
			#endif
			if (hdr != NULL) {
				res = fprintf(fOut,"%s%s",hdr,string);
			} else {
				res = fprintf(fOut,"%s",string);
			}
			if (res < 0) {
				#ifdef DEBUG
				//Serial.printf("LogFileSink::WriteLine, ERROR: %d\n", res);
				#endif
				res = SINK_WRITE_IO_ERROR;
			}
		}
	} else {
		#ifdef DEBUG
		//Serial.printf("LogFileSink::WriteLine, Error, output file not open (fOut == NULL)\n");
		#endif
		res = SINK_WRITE_IO_ERROR;
	}
	return res;
}
void LogFileSink::Close()
{
	if (fOut != NULL)
	{
#ifdef DEBUG	
	//Serial.printf("LogFileSink::Close, closing log file\n");
#endif
		fclose(fOut);
	}
	fOut = NULL;
}

void LogFileSink::Flush() {
	if (fOut != NULL) {
#ifdef DEBUG	
	//Serial.printf("LogFileSink::Flush, flushing log file\n");
#endif
	//	Serial.println("gnilk::LogFileSink, flush");
		fflush(fOut);
	}
}


// --------------------------------------------------------------------------
//
// Rolling file sink
//
#define LOG_SZ_GB(x) ((x)*1024*1024*1024)
#define LOG_SZ_MB(x) ((x)*1024*1024)
#define LOG_SZ_KB(x) ((x)*1024)

#define LOG_MAX_FILENAME 255

LogRollingFileSink::LogRollingFileSink() : LogFileSink()
{
}
LogRollingFileSink::~LogRollingFileSink()
{

}
ILogOutputSink *LogRollingFileSink::CreateInstance()
{
	return (ILogOutputSink *)(new LogRollingFileSink());
}

char *LogRollingFileSink::GetFileName(char *dst, int idx)
{
	const char *sFileName = properties.GetLogfileName();
	snprintf(dst, LOG_MAX_FILENAME, "%s.%d.log",sFileName, idx);
#ifdef DEBUG
	//Serial.printf("LogRollingFileSink, filename: %s\n",dst);
#endif
	return dst;
}

void LogRollingFileSink::RollOver()
{
	char dstFileName[LOG_MAX_FILENAME];
	char srcFileName[LOG_MAX_FILENAME];

	// 1) Close current file
	LogFileSink::Close();
	// 2) Initiate rename loop
	for(int i=nMaxBackupIndex-1;i>0;i--)
	{
		GetFileName(srcFileName, i);
		GetFileName(dstFileName, i+1);

#ifdef WIN32
#ifdef UNICODE
		{
			wchar_t w_dst[LOG_MAX_FILENAME];
			wchar_t w_src[LOG_MAX_FILENAME];
			mbstowcs(w_dst, dstFileName, LOG_MAX_FILENAME);
			mbstowcs(w_src, srcFileName, LOG_MAX_FILENAME);			
			MoveFile(w_src, w_dst);
		}
#else
	MoveFile(srcFileName, dstFileName);
#endif
#else
#endif
	}
	// 3) Open up new file
	GetFileName(srcFileName, 1);
	LogFileSink::Open(srcFileName, false);

}

void LogRollingFileSink::CheckApplyRules()
{
	if (nBytes > nBytesRollLimit) {
		// Swap to new file..
		RollOver();
		nBytes = 0;
	}
}

void LogRollingFileSink::Initialize(int argc, char **argv)
{
	// ..This is not directly correct..
	//LogFileSink::Initialize(argc, argv);
	char tmp[LOG_MAX_FILENAME];
	ParseArgs(argc, argv);
	GetFileName(tmp, 1);
	Open(tmp, true);


	nBytes = Size();

	// roll size limit of zero not allowed, using 10 MB instead
	nBytesRollLimit = properties.GetMaxLogfileSize(); //LOG_SZ_KB(10);	// 10 Mb
	if (!nBytesRollLimit) nBytesRollLimit = LOG_SZ_MB(10);

	nMaxBackupIndex = properties.GetMaxBackupIndex();	// 0 (zero) Work's like 'reset'
	
#ifdef DEBUG	
	//Serial.printf("LogRollingFileSink, initialized with name: %s\n", tmp);
#endif
}

int LogRollingFileSink::WriteLine(int dbgLevel, char *hdr, char *string)
{
	int res;
	CheckApplyRules();
	res = LogFileSink::WriteLine(dbgLevel, hdr, string);
	if (res > 0) {
		nBytes+=res;
	}
	return res;
}


/////////
//
// -- static functions
//
int Logger::iIndentStep = 2;
bool Logger::bInitialized = false;
std::queue<void *> Logger::buffers;
ILoggerList Logger::loggers;
ILoggerSinkList Logger::sinks;
Logger::TimeFormat Logger::kTimeFormat = kTFLog4Net;
LogProperties Logger::properties;

void Logger::SendToSinks(int dbgLevel, char *hdr, char *string)
{
	ILogOutputSink *pSink = NULL;
	ILoggerSinkList::iterator it;
	it = sinks.begin();
	while(it != sinks.end())
	{
		pSink = (ILogOutputSink *)*it;
		pSink->WriteLine(dbgLevel, hdr, string);
		it++;
	}
}


#ifdef WIN32
struct timezone 
{
	int  tz_minuteswest; /* minutes W of Greenwich */
	int  tz_dsttime;     /* type of dst correction */
};

static int gettimeofday(struct timeval *tv, struct timezone *tz)
{
	FILETIME ft;
	unsigned __int64 tmpres = 0;
	static int tzflag;

	if (NULL != tv)
	{
		GetSystemTimeAsFileTime(&ft);

		tmpres |= ft.dwHighDateTime;
		tmpres <<= 32;
		tmpres |= ft.dwLowDateTime;

		/*converting file time to unix epoch*/
		tmpres /= 10;  /*convert into microseconds*/
		tmpres -= DELTA_EPOCH_IN_MICROSECS; 
		tv->tv_sec = (long)(tmpres / 1000000UL);
		tv->tv_usec = (long)(tmpres % 1000000UL);
	}

	if (NULL != tz)
	{
		if (!tzflag)
		{
			_tzset();
			tzflag++;
		}
		tz->tz_minuteswest = _timezone / 60;
		tz->tz_dsttime = _daylight;
	}
	return 0;
}
#endif
//
// Returns a formatted time string for logging
// string can be either in default kTFLog4Net format or Unix
//
char *Logger::TimeString(int maxchar, char *dst)
{
	struct timeval tmv;
	gettimeofday(&tmv, NULL);
	
	switch(kTimeFormat)
	{
		case kTFDefault :
		case kTFUnix :
#ifdef WIN32
			//ctime_s(&tmv.tv_sec, 24,dst);
#else
			ctime_r(&tmv.tv_sec, dst);
#endif
			dst[24] = '\0';
			break;
		case kTFLog4Net :
			{
				time_t bla = tmv.tv_sec;
				struct tm *gmt = gmtime(&bla);
				snprintf(dst,maxchar,"%.2d.%.2d.%.4d %.2d:%.2d:%.2d.%.3d",
						 gmt->tm_mday,gmt->tm_mon+1,gmt->tm_year+1900, 
						 gmt->tm_hour,gmt->tm_min,gmt->tm_sec,tmv.tv_usec/1000);
			}
			break;
			
	}
	return dst;
}

#ifdef WIN32
// static member...
CRITICAL_SECTION Logger::bufferLock;
#endif
#ifdef LOGGER_HAVE_PTHREADS
pthread_mutex_t Logger::bufferLock;
#endif

void *Logger::RequestBuffer()
{
#ifdef WIN32
	EnterCriticalSection(&bufferLock);
#endif
#ifdef LOGGER_HAVE_PTHREADS
	pthread_mutex_lock(&bufferLock);
#endif
	void *res = NULL;
	if (buffers.empty())
	{
		res = (void *)new MsgBuffer();
	} else {
		res = buffers.front();
		buffers.pop();
	}
#ifdef WIN32
	LeaveCriticalSection(&bufferLock);
#endif
#ifdef LOGGER_HAVE_PTHREADS
	pthread_mutex_unlock(&bufferLock);
#endif


	return res;
}
void Logger::ReleaseBuffer(void *pBuf)
{
#ifdef WIN32
	EnterCriticalSection(&bufferLock);
#endif
#ifdef LOGGER_HAVE_PTHREADS
	pthread_mutex_lock(&bufferLock);
#endif

	buffers.push(pBuf);
#ifdef WIN32
	LeaveCriticalSection(&bufferLock);
#endif
#ifdef LOGGER_HAVE_PTHREADS
	pthread_mutex_unlock(&bufferLock);
#endif
}
ILogger *Logger::GetLoggerFromName(const char *name) {
    ILogger *pLogger = NULL;
    auto it = loggers.begin();
    while(it != loggers.end())
    {
        auto pInstance = (LoggerInstance *)*it;
        pLogger = pInstance->pLogger;
        if (!strcmp(pLogger->GetName(), name))
        {
            return pLogger;
        }
        it++;
    }
    return NULL;
}
ILogger *Logger::GetLoggerFromNameWithPrefix(const char *name, const char *prefix) {
    ILogger *pLogger = NULL;
    auto it = loggers.begin();
    while(it != loggers.end()) {
        auto pInstance = (LoggerInstance *)*it;
        pLogger = pInstance->pLogger;
        if (pLogger->GetPrefix() != NULL) {
			if (!strcmp(pLogger->GetName(), name) && !strcmp(pLogger->GetPrefix(), prefix)) {
				return pLogger;
			}
        }
        it++;
    }
    return NULL;
}

//
// This returns a Logger with a name and optionally a prefix
// You can supply just a name and enable the AutoSplitPrefix feature which will
// automatically generate a prefix from the name.
// Names must be on the form: '<prefix>::<item>' like: "MyClass::Function"
// If AutoSplitPrefix is FALSE the name will be used as is and the prefix added if specified.
// Reason why prefix is added 'behind' is because of API compatibility.
//
ILogger *Logger::GetLogger(const char *name, const char *prefix /* = NULL */)
{
	ILogger *pLogger = NULL;
	LoggerInstance *pInstance;
	ILoggerList::iterator it;

	// Prefix handling could do with refactoring....
	char *logprefix = (char *)prefix;
	char namebuffer[128];
	strncpy(namebuffer, name, 128);

	char *logname = namebuffer;

	Initialize();
	if (prefix == NULL && properties.IsAutoPrefixEnabled()) {
		// a name with 'prefixing' looks like:  "prefix::item"
		// generally the prefix would be the class name and the item equals the function
		char *split = (char *)strchr(namebuffer, ':');
		if ((split != NULL) && (split[1]==':')) {
			logname = (char *)&split[2];	// this is the name
			logprefix = namebuffer;
			split[0] = '\0';
		}
	}


	// If prefix is NULL, just use the same old method as before
	if (logprefix == NULL) {
	    pLogger = GetLoggerFromName(logname);
	} else {
        pLogger = GetLoggerFromNameWithPrefix(logname, logprefix);
	}

    if (pLogger != NULL) {
        return pLogger;
    }

	// Have to create a new logger
	pLogger = (ILogger *)new Logger(logname, logprefix);
	pInstance = new LoggerInstance(pLogger);
	// TODO: Support for exclude list

	loggers.push_back(pInstance);
	return pLogger;	
}

void Logger::CloseAll() {
	ILogOutputSink *pSink;
	ILoggerSinkList::iterator it;
	Initialize();
	
	it = sinks.begin();
	while(it != sinks.end())
	{	
		pSink = (ILogOutputSink *)*it;
		pSink->Close();
		delete pSink;
		it++;
	}

	sinks.clear();
	loggers.clear();

}

void Logger::SetAllSinkDebugLevel(int iNewDebugLevel)
{
	// This might very well be the first call, make sure we are initalized
	Initialize();
	
	LogBaseSink *pSink = NULL;
	ILoggerSinkList::iterator it;
	it = sinks.begin();
	while(it != sinks.end())
	{
		pSink = (LogBaseSink *)*it;
		pSink->GetProperties()->SetDebugLevel(iNewDebugLevel);
		it++;
	}
}
// TODO: Update sink properties!

// Without initialization
void Logger::AddSink(ILogOutputSink *pSink, const char *sName)
{
	LogBaseSink *pBase = (LogBaseSink *)pSink;
	if (pBase != NULL) {
		pBase->SetName(sName);
	}
	sinks.push_back(pSink);
}
// With initialization
void Logger::AddSink(ILogOutputSink *pSink, const char *sName, int argc, char **argv)
{
	pSink->Initialize(argc, argv);
	AddSink(pSink, sName);
}

bool Logger::RemoveSink(const char *sName) {
    auto cbCheckSink = [sName](ILogOutputSink *sink) -> bool {
        if (!strcmp(sName, sink->GetName())) {
            return true;
        }
        return false;
    };
    auto szBefore = sinks.size();
    sinks.erase(std::remove_if(sinks.begin(), sinks.end(), cbCheckSink));
    return (szBefore != sinks.size());
}

//
// Create sink's based on class name and factory instances in the global list
//
ILogOutputSink *Logger::CreateSink(const char *className)
{	
	for(int i=0;sinkFactoryList[i].factory!=NULL;i++) {
		if (sinkFactoryList[i].name == className) {
			return sinkFactoryList[i].factory();			
		}
		// if (!strcmp(sinkFactoryList[i].name, className)) {
		// 	return sinkFactoryList[i].factory();			
		// }
	}
	return NULL;
}

//
// This will clear out everything and rebuild all sink's from the configuration
//
void Logger::RebuildSinksFromConfiguration()
{
	char appenders[256];
	if (!properties.GetValue("sinks",appenders,256,NULL)) return;

	std::vector<std::string> arAppenders;

	// TODO: need to call destructors here I guess
	sinks.clear();

	int nAppenders = StrExplode(&arAppenders, appenders, ',');
	for (int i=0;i<nAppenders;i++)
	{
		char className[256];
		std::string sinkName = arAppenders[i] + ".class";
		if (properties.GetValue(sinkName.c_str(), className, 256, NULL)) 
		{
			LogBaseSink *pSink = (LogBaseSink *)CreateSink(className);
			if (pSink != NULL)
			{
				// 1) Extract all known properties and put to sink
				std::vector<std::pair<std::string, std::string> > sinkProperties;
				properties.GetAllStartingWith(&sinkProperties, arAppenders[i].c_str());
				for(int p=0;p<(int)sinkProperties.size();p++) {
					pSink->GetProperties()->SetValue(sinkProperties[i].first.c_str(),sinkProperties[i].second.c_str());
				}
				// 2) Call initialize and attach
				pSink->Initialize(0,NULL);
				sinks.push_back(pSink);
			}
		}
	}
}

void Logger::Initialize()
{
	if (!Logger::bInitialized)
	{
		// Initialize the rest
#if defined(DEBUG) || defined(_DEBUG)
		ILogOutputSink *pSink = (ILogOutputSink *)new LogConsoleSink();
		AddSink(pSink, "console", 0, NULL);
#endif
		Logger::bInitialized = true;
		properties.SetDebugLevel(DEFAULT_DEBUG_LEVEL);
		properties.SetName("Logger");

		// HACK
		properties.ReadFromFile("logger.res");
		char appenders[256];
		properties.GetValue("sinks",appenders, 256, "");
		if (strcmp(appenders, "")) {
			RebuildSinksFromConfiguration();
		}
#ifdef WIN32
		InitializeCriticalSection(&bufferLock);
#endif
	}
}

// Regular functions

Logger::Logger(const char *sName, const char *sPrefix)
{
    this->sName = strdup(sName);
	if (sPrefix != NULL) {
	    this->sPrefix = strdup(sPrefix);
	} else {
		this->sPrefix = NULL;
	}
	this->iIndentLevel = 0;
	this->sIndent = (char *)malloc(MAX_INDENT+1);
	memset(this->sIndent,0,MAX_INDENT+1);
	Logger::Initialize();
}
Logger::~Logger()
{
	free(this->sName);
	// remove this from list of loggers
	// TODO: better clean up, properties...
}
static std::string lMessageClassNames[] = 
{
	"NONE",			// 0
	"DEBUG",		// 1
	"INFO",			// 2
	"WARN",			// 3
	"ERROR",		// 4
	"CRITICAL"		// 5
	"CUSTOM"		// 6
};
const char *Logger::MessageClassNameFromInt(int mc) 
{
	if ((mc>=Logger::kMCNone) && (mc<(int)Logger::kMCDebug)) 
	{
		return lMessageClassNames[0].c_str();
	}
	else if ((mc>=(int)Logger::kMCDebug) && (mc<(int)Logger::kMCInfo))
	{
		return lMessageClassNames[1].c_str();
	}
	else if ((mc>=(int)Logger::kMCInfo) && (mc<(int)Logger::kMCWarning))
	{
		return lMessageClassNames[2].c_str();
	}
	else if ((mc>=(int)Logger::kMCWarning) && (mc<(int)Logger::kMCError))
	{
		return lMessageClassNames[3].c_str();
	}
	else if ((mc>=(int)Logger::kMCError) && (mc<(int)Logger::kMCCritical))
	{
		return lMessageClassNames[4].c_str();
	}
	else if ((mc>=(int)Logger::kMCCritical))
	{
		return lMessageClassNames[5].c_str();
	} 
	return lMessageClassNames[6].c_str();

}

int Logger::MessageLevelFromName(const char *level)
{
	if (!strcmp(level, "NONE")) return kMCNone;
	if (!strcmp(level, "DEBUG")) return kMCDebug;
	if (!strcmp(level, "INFO")) return kMCInfo;
	if (!strcmp(level, "WARN")) return kMCWarning;
	if (!strcmp(level, "WARNING")) return kMCWarning;
	if (!strcmp(level, "ERROR")) return kMCError;
	if (!strcmp(level, "CRITICAL")) return kMCCritical;
	return kMCNone;
}

//
void Logger::WriteReportString(int mc, MsgBuffer *pBuf)
{
	char sHdr[MAX_INDENT + 64];
	char sTime[32];	// saftey, 26 is enough

//
	char *string = pBuf->GetBuffer();
	#ifdef LOGGER_HAVE_NEWLINE										
	strncat(string, "\n", pBuf->GetSize());				
	#endif																	


	const char *sLevel = MessageClassNameFromInt(mc);

	TimeString(32, sTime);
	// Create the special header string
	// Format: "time [thread] msglevel module - "

#ifdef WIN32
	DWORD tid = 0;
	tid = GetCurrentThreadId();
#else	
	uint32_t tid;
	void *p_thread = NULL;	
	#ifdef LOGGER_HAVE_PTHREADS
	p_thread = pthread_self();	
	#endif
	tid = (uint64_t)(p_thread) & 0xffffffff;
#endif
	if (this->sPrefix == NULL) {
	    if (IsAutoPrefixEnabled()) {
            snprintf(sHdr, MAX_INDENT + 64, "%s [%.8x::                ] %8s %32s - %s", sTime, tid, sLevel, sName, sIndent);
	    } else {
            snprintf(sHdr, MAX_INDENT + 64, "%s [%.8x] %8s %32s - %s", sTime, tid, sLevel, sName, sIndent);
	    }
	} else {
        snprintf(sHdr, MAX_INDENT + 64, "%s [%.8x::%16s] %8s %32s - %s", sTime, tid, sPrefix, sLevel, sName, sIndent);
	}
	
	Logger::SendToSinks((int)mc,sHdr, string);
}

void Logger::GenerateIndentString() {
	memset(this->sIndent,0,MAX_INDENT+1);
	memset(this->sIndent,' ',iIndentLevel);
}


// This functionality is duplicated by all 'write'-functions. It composes the message
// string. The reason why it is not in a function is because of the va_xxx functions.
// Event is essentially a container around the buffer which makes a query for the buffer
// upon creation and releases it in the destructor
#define WRITE_REPORT_STRING(__DBGTYPE__) \
	va_list	values;														\
	char * newstr = NULL;												\
	try {																\
		LogEvent evt;													\
		MsgBuffer *pBuf = evt.GetBuffer();								\
		int res;														\
		do																\
		{																\
			newstr=pBuf->GetBuffer();									\
			va_start( values, sFormat );								\
			res = vsnprintf(newstr, pBuf->GetSize(), sFormat, values);	\
			va_end(	values);											\
			if (res < 0) {												\
				pBuf->Extend();											\
			}															\
		} while(res < 0);												\
		Logger::WriteReportString(__DBGTYPE__, pBuf);		            \
	} catch(...) {														\
	}																	\



void Logger::WriteLine(int iDbgLevel, const char *sFormat,...)
{
	// Always write stuff without global filtering - let appenders figure it out..
	WRITE_REPORT_STRING(iDbgLevel);
}
void Logger::WriteLine(const char *sFormat,...)
{
	// Always write stuff without global filtering - let appenders figure it out..
	WRITE_REPORT_STRING(kMCNone);
}
void Logger::Critical(const char *sFormat,...)
{
	if (IsCriticalEnabled()) {
		WRITE_REPORT_STRING(kMCCritical);
	}
}
void Logger::Error(const char *sFormat, ...)
{
	if (IsErrorEnabled()) {
		WRITE_REPORT_STRING(kMCError);
	}
}
void Logger::Warning(const char *sFormat, ...)
{	
	if (IsWarningEnabled()) {
		WRITE_REPORT_STRING(kMCWarning);
	}
}
void Logger::Info(const char *sFormat, ...)
{
	if (IsInfoEnabled()) {
		WRITE_REPORT_STRING(kMCInfo);
	}
}
void Logger::Debug(const char *sFormat, ...)
{
	if (IsDebugEnabled()) {
		WRITE_REPORT_STRING(kMCDebug);
	}
}


// Increases intendation
void Logger::Enter()
{
	iIndentLevel+=Logger::iIndentStep;
	if (iIndentLevel > MAX_INDENT)
	{
		iIndentLevel = MAX_INDENT;
	}
	GenerateIndentString();
}

// Decreases intendation
void Logger::Leave()
{
	iIndentLevel-=Logger::iIndentStep;
	if (iIndentLevel < 0)
	{
		iIndentLevel = 0;
	}
	GenerateIndentString();
}

// ---------------------------------------------------------------------------
//
// Holds an instance of a logger
//

LoggerInstance::LoggerInstance()
{
	this->pLogger = NULL;
}

LoggerInstance::LoggerInstance(ILogger *pLogger)
{
	this->pLogger = pLogger;
}


// ---------------------------------------------------------------------------
//
// Message buffers are used to minimize buffer allocation.
// Each debug string get's copied in to a message buffer before feed down the
// chain of sink's
//
MsgBuffer::MsgBuffer()
{
	buffer = (char *)malloc(DEFAULT_BUFFER_SIZE);
	sz = DEFAULT_BUFFER_SIZE;
}
MsgBuffer::~MsgBuffer()
{
	free(buffer);
}
void MsgBuffer::Extend()
{
	sz += DEFAULT_BUFFER_SIZE;
	char *tmp = (char *)realloc(buffer, sz);
	if (tmp == NULL)
	{
		//exit(1);	// can't allocate memory, just leave..
		tmp = buffer;
		sz -= DEFAULT_BUFFER_SIZE;
	}
	buffer = tmp;
}
// ---------------------------------------------------------------------------
//
// Property handling
//
LogProperties::LogProperties() : LogPropertyReader()
{
	SetDefaults();
}
void LogProperties::SetDefaults()
{
	this->iDebugLevel = DEFAULT_DEBUG_LEVEL;
	this->name = strdup(DEFAULT_SINK_NAME);
	this->logFileName = strdup(DEFAULT_LOGFILE_NAME);
	this->nMaxBackupIndex = 10;
	this->nMaxLogfileSize = LOG_SZ_MB(10);
	this->autoPrefix = false; // Automatically split logger names like "Prefix::PostFix"
}

#define REPLACE_STR(__dst,__src)\
	if (__dst != NULL)		\
	{						\
		free(__dst);		\
	}						\
	__dst = strdup(__src);

void LogProperties::SetName(const char *newName)
{
	REPLACE_STR(this->name, newName);
}

void LogProperties::SetLogfileName(const char *newName)
{
	REPLACE_STR(this->logFileName, newName);
}

void LogProperties::SetClassName(const char *newName)
{
	REPLACE_STR(this->className, newName);
}

// Called by the base class on 'SetValue' - use to update internal proper variables
void LogProperties::OnValueChanged(const char *key, const char *value)
{
	if (!strcmp(key, LOG_CONF_NAME)) {
		SetName(value);
	} else if (!strcmp(key, LOG_CONF_DEBUGLEVEL)) {
		int level = atoi(value);
		if (!level)
			level = Logger::MessageLevelFromName(value);			
		SetDebugLevel(level);
	} else if (!strcmp(key, LOG_CONF_MAXBACKUPINDEX)) {
		SetMaxBackupIndex(atoi(value));		
	} else if (!strcmp(key, LOG_CONF_MAXLOGSIZE)) {
		SetMaxLogfileSize(atoi(value));
	} else if (!strcmp(key, LOG_CONF_LOGFILE)) {
		SetLogfileName(value);
	} else if (!strcmp(key, LOG_CONF_CLASSNAME)) {
		SetClassName(value);
	}
}

// ---------------------------------------------------------------------------
//
// Property file reading
//
LogPropertyReader::LogPropertyReader()
{

}
LogPropertyReader::~LogPropertyReader()
{

}

void LogPropertyReader::ParseLine(char *_line)
{
	if (_line == NULL) return;

	if (_line[0]!='#')
	{
		char *line = StrTrim(_line);
		char *value = strchr(line, '=');
		if (value != NULL)
		{
			*value='\0';
			value++;
			SetValue(line, value);
		}
	}
}

void LogPropertyReader::ReadFromFile(const char *filename)
{
	char line[256];
	FILE *f = fopen(filename, "rb");
	if (f != NULL)
	{
		while (fgets(line,256, f)!=NULL)
		{
			ParseLine(line);
		}
		fclose(f);
	}
}
void LogPropertyReader::WriteToFile(const char *filename)
{
	// TODO: Implement this one...
}

int LogPropertyReader::GetAllStartingWith(std::vector<std::pair<std::string, std::string> > *result, const char *filter)
{
	std::string sFilter(filter);
	std::map<std::string, std::string>::iterator it;

	for(it = properties.begin(); it!=properties.end(); it++)
	{
		if (it->first.compare(0,sFilter.length(),sFilter) == 0) {
			result->push_back(std::pair<std::string, std::string>(it->first, it->second));
		}
	}
	return result->size();
}

char *LogPropertyReader::GetValue(const char *key, char *dst, int nMax, const char *defValue)
{
	if (properties.find(key) != properties.end())
	{
		strncpy(dst, properties.find(key)->second.c_str(), nMax);
	} else
	{
		strncpy(dst, defValue, nMax);
	}
	return dst;
}
void LogPropertyReader::SetValue(const char *key, const char *value)
{
	if (properties.find(key) != properties.end())
	{
		// Update needed, just remove and reinsert..
		properties.erase(key);
	}
	properties.insert(std::pair<std::string, std::string>(key, value));
	OnValueChanged(key, value);
}

// -------------------------------------------------------------------------
//
// Internal helpers
//

static char *StrTrim(char *s) {
    char *ptr;
    if (!s)
        return NULL;   // handle NULL string
    if (!*s)
        return s;      // handle empty string
    for (ptr = s + strlen(s) - 1; (ptr >= s) && isspace(*ptr); --ptr);
    ptr[1] = '\0';
    return s;
}

//
// Splits the string into substrings for a given character
//
static int StrExplode(std::vector<std::string> *strList, char *mString, int chrSplit)
{
	std::string strTmp,strPart;
	size_t ofs,pos;
	int count;

	strTmp = std::string(mString);
	pos = count = 0;
	do 	
	{
		ofs = strTmp.find_first_of(chrSplit,pos);

		if (ofs == -1)
		{
			if (pos != -1)
			{
				strPart = strTmp.substr(pos,strTmp.length()-pos);
				strList->push_back(strPart);
				count++;
			}
			else
			{
				// We had trailing spaces...
			}

			break;
		}
		strPart = strTmp.substr(pos,ofs - pos);
		strList->push_back(strPart);
		pos = ofs+1;
		pos = strTmp.find_first_not_of(chrSplit,pos);
		count++;
	} while(1);

	return count;
} // StrExplode

} // axcore