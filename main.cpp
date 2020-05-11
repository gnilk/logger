//
// Test application for the logger
//
#ifdef WIN32
#include "stdafx.h"
#endif
#include "logger.h"

using namespace gnilk;
static ILogger *pLog;
int tryCatchTest(int wef) {
	if (wef > 2) throw(wef);

	LogIndent idt(pLog);	// Automatic indentation container
	pLog->Debug("wef: %d",wef);
	tryCatchTest(wef+1);
	pLog->Debug("Leave!");	// Will never be printed due to exception
	return wef;

}

void testRollingAppender()
{
	char *argv[] = {"file","logfile",NULL};
	LogRollingFileSink *rollSink = new LogRollingFileSink();
	Logger::AddSink(rollSink, "rollingAppender",2, argv);

	pLog = Logger::GetLogger("main");

	for(int i=0;i<10000;i++)
	{
		pLog->Debug("TESTING-ROLLING-APPENDER: %d",i);
	}
}

#ifdef WIN32
int _tmain(int argc, _TCHAR* argv[])
#else
int main(int argc, char **argv)
#endif
{
	pLog = Logger::GetLogger("main");
	Logger::GetProperties()->AutoPrefixEnable(true);
	// This is enabled by default in debug builds
	#ifndef DEBUG
	Logger::AddSink(new LogConsoleSink,"console");
	#endif	

	pLog->Debug("Testing debug: %s", "hello world");
	pLog->Info("Testing info: %d", 1);
	Logger::GetProperties()->SetDebugLevel(Logger::kMCWarning);
	pLog->Debug("Early exit, no log");
	pLog->Info("Visible");
	pLog->Warning("Warning");
	Logger::GetProperties()->SetDebugLevel(Logger::kMCNone);
	pLog->Debug("Testing exceptions");
	try
	{
		tryCatchTest(0);
	} catch(...) {
		pLog->Error("Exception!");
	}
	// Should be on the previous indentdation level
	pLog->Debug("Done!");


	gnilk::ILogger *logger2 = gnilk::Logger::GetLogger("function","myclass");
	logger2->Debug("With prefix");

	gnilk::ILogger *logger3 = gnilk::Logger::GetLogger("myclass::function");
	logger3->Debug("With prefix 2");

	//testRollingAppender();



	return 0;
}

