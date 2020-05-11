/*-------------------------------------------------------------------------
File    :	Logger.cpp
Author  :   
Orginal :	
Descr   :	Debug logger - modified to suit Arduino type systems

--------------------------------------------------------------------------- 
Todo [-:undone,+:inprogress,!:done]:

 
Changes: 

-- Date -- | -- Name ------- | -- Did what...                              
2018-09-18 | Gnilk           | Imported and modified to work with Arduino (ESP32)

---------------------------------------------------------------------------*/
#include <stdlib.h>
#include <stdio.h>

#include <list>
#include <queue>
#include <map>
#include <string>

#ifndef __LOGGER_INTERNAL_H__
#define __LOGGER_INTERNAL_H__

#include "logger.h"

using namespace std;

namespace gnilk
{
	#define LOG_CONF_LOGFILE ("/sd/debug")
	#define LOG_CONF_MAXLOGSIZE ("maxlogsize")
	#define LOG_CONF_MAXBACKUPINDEX ("maxbackupindex")
	#define LOG_CONF_DEBUGLEVEL ("debuglevel")
	#define LOG_CONF_NAME ("name")
	#define LOG_CONF_CLASSNAME ("class")

	extern "C"
	{
		typedef ILogOutputSink *(LOG_CALLCONV *LOG_PFNSINKFACTORY)();
	}
	typedef struct
	{
		//char *name;
		std::string name;
		LOG_PFNSINKFACTORY factory;
		
	} LOG_SINK_FACTORY;


	// Internal class, not available to outside..
	class MsgBuffer
	{
	private:
		char *buffer;
		int sz;
	public:
		MsgBuffer();
		virtual ~MsgBuffer();
		
		__inline char *GetBuffer() { return buffer; }
		__inline int GetSize() { return sz; }
		
		void Extend();
	};

	class LogEvent
	{
	private:
		MsgBuffer *pBuffer;
	public:
		LogEvent() {
			pBuffer = (MsgBuffer *)Logger::RequestBuffer();
		}
		virtual ~LogEvent() {
			Logger::ReleaseBuffer(pBuffer);
		}
		__inline MsgBuffer *GetBuffer() {
			return pBuffer;
		}
	};

	typedef std::pair<std::string, std::string> strStrPair;

}

#endif
