# logger
Tiny single file Log4Net inspired logger with minimal amount of dependencies.
Works on Windows, macOS, Arduino environment should compile on Linux and for Android.

The Logger has pretty small footprint which makes it handy in embedded projects.

Intended use is in smaller projects. The logger has been used in quite big projects as well but since thread saftey was never a concern 
and was added quite late I would not recommend you to use it. It has been used in massive threaded projects with good results but it's a
new feature so take it for what it is.

## Use like
```C++
int main(int argc, char **argv) {
	gnilk::ILogger *pLog = gnilk::Logger::GetLogger("main");
	// This is enabled by default in debug builds
	#ifndef DEBUG
	gnilk::Logger::AddSink(new LogConsoleSink,"console");
	#endif	

	pLog->Debug("Testing debug: %s", "hello world");
	pLog->Info("Testing info: %d", 1);
}
```

There are several different types of log sinks available
- Console
- File
- RollingFile
- Serial output (Arduino)
- Android LogCat output (Android)

## Details
Just drop the files into your project and include it. When building in Debug (-DDEBUG or -D_DEBUG) the console output sink is 
enabled by default and warning level filter is set to 'NONE'.

### Control and filter debug levels
You can set the output filter per sink or overall. The following levels exists:
```C++
			kMCNone = 0,
			kMCDebug = 100,
			kMCInfo = 200,
			kMCWarning = 300,
			kMCError = 400,
			kMCCritical = 500,
```
### Output format
The output format is locked and can not be changed without a source change. This is to keep the foot-print down and avoid cluttering 
the API. I very seldom need to change - thus never implemented it.

Example (from test application):
```
11.05.2020 08:49:47.710 [0x0]    DEBUG                             main - Testing debug: hello world
11.05.2020 08:49:47.710 [0x0]     INFO                             main - Testing info: 1
11.05.2020 08:49:47.710 [0x0]     WARN                             main - Warning
11.05.2020 08:49:47.710 [0x0]    DEBUG                             main - Testing exceptions
11.05.2020 08:49:47.710 [0x0]    DEBUG                             main -   wef: 0
11.05.2020 08:49:47.710 [0x0]    DEBUG                             main -     wef: 1
11.05.2020 08:49:47.710 [0x0]    DEBUG                             main -       wef: 2
11.05.2020 08:49:47.710 [0x0]    ERROR                             main - Exception!
11.05.2020 08:49:47.710 [0x0]    DEBUG                             main - Done!
```
Formal fields:
- Date
- Time
- ThreadID + (prefix - if used)
- Level
- Module/Logger
- String

It is pretty straight forward to parse the logfiles to make a viewer or similar.

### Note about prefix
It is possible to create a 'prefix' to a logger, like:
```C++
   gnilk::ILogger *logger = gnilk:Logger::GetLogger("myclass::function");
   logger->Debug("With prefix");
```
And the output will change to:
```
11.05.2020 09:17:34.294 [0a32fdc0::         myclass]    DEBUG                         function - With prefix
```

It also possible to create with an explicit prefix:
```C++
   gnilk::ILogger *logger = gnilk:Logger::GetLogger("function", "myclass);
   logger->Debug("With prefix");
```
Which would produce the same output.
Using prefixes is very handy when you have many instances of a class and need to separate the instances in the debug trace. In this case
I usually construct a prefix with an instance counter or similar. 

## Adding Custom SINKS
Add custom sinks is pretty straight forward. Take a look at the AndroidDebugLogSink and you should have a good idea.
Make sure you inherit 'LogBaseSink' as it provides a default (mostly empty) implementation of the 'ILogOutputSink' interface.

Definition:
```C++
	class AndroidDebugLogCatSink : public LogBaseSink
	{
	public:
		virtual void Initialize(int argc, char **argv);
		virtual void WriteLine(int dbgLevel, char *hdr, char *string);
		virtual void Close();
	};
```

Implementation:
```C++
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
```
