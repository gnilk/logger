/*-------------------------------------------------------------------------
File    :	Logger.cpp
Author  :   
Orginal :	
Descr   :	Debug logger - modified to suit Arduino type systems

--------------------------------------------------------------------------- 
Todo [-:undone,+:inprogress,!:done]:

 THIS VERSION DIFFERS FROM OTHERS (this one is ahead).
 - Multithread safe
 - Logger Prefix support
 - Arduino Serial Sink support

Changes: 

-- Date -- | -- Name ------- | -- Did what...                              
2020-04-01 | Gnilk           | Added module prefix support, when multiple instances of same class wants to write
2018-09-18 | Gnilk           | Imported and modified to work with Arduino (ESP32)

---------------------------------------------------------------------------*/

#include <stdlib.h>
#include <stdio.h>

#include <list>
#include <queue>
#include <map>
#include <vector>
#include <string>

#ifdef WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#endif

#ifdef LOGGER_HAVE_PTHREADS
#include <pthread.h>
#endif

#ifndef __LOGGER_H__
#define __LOGGER_H__


using namespace std;

namespace gnilk
{
#ifdef WIN32
#define LOG_CALLCONV __stdcall
#else
#define LOG_CALLCONV
#endif

#define MAX_INDENT 256
	// Main public interface - this is the one you will normally use
	class ILogger
	{
	public:
		// properties
		virtual int GetIndent() = 0;
		virtual int SetIndent(int nIndent) = 0;
        virtual char *GetName() = 0;
        virtual char *GetPrefix() = 0;

		// functions
		virtual void WriteLine(const char *sFormat,...) = 0;
		virtual void WriteLine(int iDbgLevel, const char *sFormat,...) = 0;
		virtual void Critical(const char *sFormat,...) = 0;
		virtual void Error(const char *sFormat, ...) = 0;
		virtual void Warning(const char *sFormat, ...) = 0;
		virtual void Info(const char *sFormat, ...) = 0;
		virtual void Debug(const char *sFormat, ...) = 0;

        virtual void Enter() = 0;
		virtual void Leave() = 0;
	};
	class LogPropertyReader
	{
	private:
		std::map<std::string, std::string> properties;
		void ParseLine(char *line);
	public:
		LogPropertyReader();
		virtual ~LogPropertyReader();

		void ReadFromFile(const char *filename);
		void WriteToFile(const char *filename);

		virtual char *GetValue(const char *key, char *dst, int nMax, const char *defValue);
		virtual void SetValue(const char *key, const char *value);

		virtual int GetAllStartingWith(std::vector<std::pair<std::string, std::string> > *result, const char *filter);

		virtual void OnValueChanged(const char *key, const char *value) = 0;
	};
	// Holds properties for the logger and/or sink
	class LogProperties : public LogPropertyReader
	{
	protected:
		int iDebugLevel;	// Everything above this becomes written to the sink
		char *name;
		int nMaxBackupIndex;
		long nMaxLogfileSize;
		char *logFileName;
		char *className;
		bool autoPrefix;	// This enables splitting logger names like 'prefix::postfix' and print them differently

		void SetDefaults();
	public:
		LogProperties();

		__inline bool IsLevelEnabled(int iDbgLevel) { return ((iDbgLevel>=iDebugLevel)?true:false); }
		__inline int GetDebugLevel() { return iDebugLevel; }
		__inline void SetDebugLevel(int newLevel) { iDebugLevel = newLevel; }
		__inline bool IsAutoPrefixEnabled() { return autoPrefix; }
		__inline void AutoPrefixEnable(bool bEnable) { autoPrefix = bEnable; }

		__inline const char *GetName() { return name; };
		void SetName(const char *newName);

		__inline const char *GetClassName() { return className; };
		void SetClassName(const char *newName);

		__inline const char *GetLogfileName() { return logFileName; };
		void SetLogfileName(const char *newName);

		__inline long GetMaxLogfileSize() { return nMaxLogfileSize; };
		__inline void SetMaxLogfileSize(const long nSize) { nMaxLogfileSize = nSize; };
		__inline int GetMaxBackupIndex() { return nMaxBackupIndex; };
		__inline void SetMaxBackupIndex(const int nIndex) { nMaxBackupIndex = nIndex; }; 


		// Event from reader
		void OnValueChanged(const char *key, const char *value);
	};

	// Used to wrap up indentation when using exceptions
	// Use this class for automatic and proper indent handling
	class LogIndent
	{
	private:
		ILogger *pLogger;
	public:
		LogIndent(ILogger *_pLogger) : pLogger(_pLogger) { pLogger->Enter(); }
		virtual ~LogIndent() { pLogger->Leave(); }
	};


	// return valus from 'WriteLine'
#define SINK_WRITE_UNKNOWN_ERROR -100
#define SINK_WRITE_IO_ERROR -1
#define SINK_WRITE_FILTERED 0

	class ILogOutputSink
	{
	public:
		virtual ~ILogOutputSink() {}
		virtual const char *GetName() = 0;
		virtual void Initialize(int argc, const char **argv) = 0;
		virtual int WriteLine(int dbgLevel, char *hdr, char *string) = 0;
		virtual void Flush() = 0;
		virtual void Close() = 0;
	};
	class LogBaseSink : public ILogOutputSink
	{
	protected:
		char *name;
		LogProperties properties;
		__inline bool WithinRange(int iDbgLevel) { return (iDbgLevel>=properties.GetDebugLevel())?true:false; }
	public:	
		LogProperties *GetProperties() { return &properties; }
	public:
		virtual ~LogBaseSink() {}
		void SetName(const char *newName) { properties.SetName(newName); }
		const char *GetName() override { return properties.GetName(); }

		virtual void Initialize(int argc, const char **argv) override = 0;
		virtual int WriteLine(int dbgLevel, char *hdr, char *string) override = 0;
		virtual void Close() override = 0;
		virtual void Flush() override {}
	};
	class LogConsoleSink : 	public LogBaseSink
	{
	public:
		void Initialize(int argc, const char **argv) override;
		int WriteLine(int dbgLevel, char *hdr, char *string) override;
		void Close() override;

		static ILogOutputSink * LOG_CALLCONV CreateInstance();
	};
	#ifdef LOGGER_HAVE_SERIAL
	class LogSerialSink : public LogBaseSink
	{
	public:
		void Initialize(int argc, const char **argv) override;
		int WriteLine(int dbgLevel, char *hdr, char *string) override;
		void Close() override;
		static ILogOutputSink * LOG_CALLCONV CreateInstance();
	};
	#endif


	class LogFileSink : public LogBaseSink
	{
	protected:
		FILE *fOut;

		void Open(const char *filename, bool bAppend);
		long Size();
		void ParseArgs(int argc, const char **argv);
	public:
		LogFileSink();
		virtual ~LogFileSink();
		void Initialize(int argc, const char **argv) override;
		int WriteLine(int dbgLevel, char *hdr, char *string) override;
		void Close() override;
		void Flush() override;

		static ILogOutputSink * LOG_CALLCONV CreateInstance();
    private:
        bool autoflush = false;
	};	

	class LogRollingFileSink : public LogFileSink
	{
	private:
		int nMaxBackupIndex;
		long nBytesRollLimit;
		long nBytes;

		char *GetFileName(char *dst, int idx);
		void RollOver();
		void CheckApplyRules();
	public:
		LogRollingFileSink();
		virtual ~LogRollingFileSink();
		void Initialize(int argc, const char **argv) override;
		int WriteLine(int dbgLevel, char *hdr, char *string) override;

		static ILogOutputSink * LOG_CALLCONV CreateInstance();
	};
	
	class LoggerInstance
	{
	public:
		ILogger *pLogger;
		//std::list<char *> lExcludedModules;
	public:
		LoggerInstance();
		LoggerInstance(ILogger *pLogger);
	};



	typedef std::list<LoggerInstance *> ILoggerList;
	typedef std::list<ILogOutputSink *>ILoggerSinkList;

	class MsgBuffer;	// defined in logger_internal.h

	class Logger : public ILogger
	{
	public:
		typedef enum
		{
			kMCNone = 0,
			kMCDebug = 100,
			kMCInfo = 200,
			kMCWarning = 300,
			kMCError = 400,
			kMCCritical = 500,
		} MessageClass;
		
		typedef enum
		{
			kTFDefault,
			kTFLog4Net,
			kTFUnix,			
		} TimeFormat;
	private:
		
		char *sName;
		char *sPrefix;
		char *sIndent;
		int iIndentLevel;
		Logger(const char *sName, const char *sPrefix);		
        void WriteReportString(int mc, gnilk::MsgBuffer *pBuf);		
		void GenerateIndentString();
		
	private:
		static TimeFormat kTimeFormat;
		static bool bInitialized;
		static int iIndentStep;
		static ILoggerList loggers;
		static ILoggerSinkList sinks;
		static LogProperties properties;
		static std::queue<void *> buffers;
		static char *TimeString(int maxchar, char *dst);
		static void SendToSinks(int dbgLevel, char *hdr, char *string);
		static ILogOutputSink *CreateSink(const char *className);
		static void RebuildSinksFromConfiguration();
		static ILogger *GetLoggerFromName(const char *name);
        static ILogger *GetLoggerFromNameWithPrefix(const char *name, const char *prefix);

#ifdef WIN32
		static CRITICAL_SECTION bufferLock;
#endif
#ifdef LOGGER_HAVE_PTHREADS
		static pthread_mutex_t bufferLock;
#endif
	public:
	
		virtual ~Logger();
        static void Initialize();   // Call this first..
		static ILogger *GetLogger(const char *name, const char *prefix = NULL);
		static void CloseAll();
		static void SetAllSinkDebugLevel(int iNewDebugLevel);
		static void AddSink(ILogOutputSink *pSink, const char *sName);
		static void AddSink(ILogOutputSink *pSink, const char *sName, int argc, const char **argv);
        static bool RemoveSink(const char *sName);

		// Refactor this to a LogManager
		static void *RequestBuffer();
		static void ReleaseBuffer(void *pBuf);

		static const char *MessageClassNameFromInt(int mc);
		static int MessageLevelFromName(const char *level);


		static LogProperties *GetProperties() { return &Logger::properties; }

		__inline bool IsDebugEnabled() { return (Logger::properties.IsLevelEnabled((int)kMCDebug)?true:false);}
		__inline bool IsInfoEnabled() { return (Logger::properties.IsLevelEnabled((int)kMCInfo)?true:false);}
		__inline bool IsWarningEnabled() { return (Logger::properties.IsLevelEnabled((int)kMCWarning)?true:false);}
		__inline bool IsErrorEnabled() { return (Logger::properties.IsLevelEnabled((int)kMCError)?true:false);}
		__inline bool IsCriticalEnabled() { return (Logger::properties.IsLevelEnabled((int)kMCCritical)?true:false);}
        __inline bool IsAutoPrefixEnabled() { return Logger::properties.IsAutoPrefixEnabled(); }



        // properties
		virtual int GetIndent() { return iIndentLevel; };
		virtual int SetIndent(int nIndent) { iIndentLevel = nIndent; return iIndentLevel; };
        virtual char *GetName() { return sName;};
        virtual char *GetPrefix() { return sPrefix;};

		// Functions
		virtual void WriteLine(int iDbgLevel, const char *sFormat,...);
		virtual void WriteLine(const char *sFormat,...);
		virtual void Critical(const char *sFormat,...);
		virtual void Error(const char *sFormat, ...);
		virtual void Warning(const char *sFormat, ...);
		virtual void Info(const char *sFormat, ...);
		virtual void Debug(const char *sFormat, ...);


        // Enter leave functions, use to auto-indent flow statements, take care on exceptions!
		virtual void Enter();
		virtual void Leave();
	};
	
}

#endif
