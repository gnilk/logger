//
// Created by gnilk on 19.10.23.
// These unit-tests are meant to give me a base-line for when creating the new logger...
// I want the new logger to be a 90% drop-in-replacement...
//
#include <string>
#include <vector>
#include <testinterface.h>
#include "logger.h"

using namespace gnilk;
extern "C" {
DLL_EXPORT int test_logger(ITesting *t);
DLL_EXPORT int test_logger_exit(ITesting *t);
DLL_EXPORT int test_logger_msgclasses(ITesting *t);
DLL_EXPORT int test_logger_enabledisable(ITesting *t);
DLL_EXPORT int test_logger_enterleave(ITesting *t);
DLL_EXPORT int test_logger_indent(ITesting *t);
}

class MockOutputSink : public LogBaseSink {
public:
    struct LogItem {
        int dbgLevel;
        std::string header;
        std::string string;
    };
public:
    MockOutputSink() = default;
    virtual ~MockOutputSink() = default;

    const char *GetName() override {
        static const std::string name = "mocksink";
        return name.c_str();
    }

    void Initialize(int argc, const char **argv) override {
    }

    int WriteLine(int dbgLevel, char *hdr, char *string) override {
        MockOutputSink::LogItem item = {
                .dbgLevel = dbgLevel,
                .header = std::string(hdr),
                .string = std::string(string)
        };
        logItems.push_back(item);
        return 1;
    }
    void Close() override {

    }
public: // my mock functions
    const LogItem &LastItem() {
        return logItems.back();
    }
private:
    std::vector<MockOutputSink::LogItem> logItems = {};
};

// Logger don't allow 'static Sink mysink'
// Because it wraps the sink in a unique ptr...
static MockOutputSink *mysink = new MockOutputSink();


DLL_EXPORT int test_logger(ITesting *t) {

    Logger::Initialize();
    Logger::AddSink(mysink, "sink", 0, nullptr);

    return kTR_Pass;
}

DLL_EXPORT int test_logger_exit(ITesting *t) {
    return kTR_Pass;
}

DLL_EXPORT int test_logger_msgclasses(ITesting *t) {
    auto logger = Logger::GetLogger("test");
    logger->Debug("wef");
    TR_ASSERT(t, mysink->LastItem().dbgLevel == Logger::MessageClass::kMCDebug);

    logger->Info("wef");
    TR_ASSERT(t, mysink->LastItem().dbgLevel == Logger::MessageClass::kMCInfo);

    logger->Warning("wef");
    TR_ASSERT(t, mysink->LastItem().dbgLevel == Logger::MessageClass::kMCWarning);

    logger->Error("wef");
    TR_ASSERT(t, mysink->LastItem().dbgLevel == Logger::MessageClass::kMCError);

    logger->Critical("wef");
    TR_ASSERT(t, mysink->LastItem().dbgLevel == Logger::MessageClass::kMCCritical);

    return kTR_Pass;
}

DLL_EXPORT int test_logger_enabledisable(ITesting *t) {

    auto logA = Logger::GetLogger("log_A");
    auto logB = Logger::GetLogger("log_B");

    // Both enabled
    logA->Debug("from log a");
    TR_ASSERT(t, mysink->LastItem().string == std::string("from log a"));
    logB->Debug("from log b");
    TR_ASSERT(t, mysink->LastItem().string == std::string("from log b"));

    // disable A
    Logger::DisableLogger("log_A");
    TR_ASSERT(t, logA->IsEnabled() == false);
    // this should not show up..
    logA->Debug("from log a2");
    TR_ASSERT(t, mysink->LastItem().string != std::string("from log a2"));
    // but this should...
    logB->Debug("from log b2");
    TR_ASSERT(t, mysink->LastItem().string == std::string("from log b2"));

    // also disable B
    Logger::DisableLogger("log_B");
    TR_ASSERT(t, logA->IsEnabled() == false);
    TR_ASSERT(t, logB->IsEnabled() == false);

    logA->Debug("from log a3");
    TR_ASSERT(t, mysink->LastItem().string != std::string("from log a3"));
    logB->Debug("from log b3");
    TR_ASSERT(t, mysink->LastItem().string != std::string("from log b3"));
    // None of the above should be passed down to the sink - the 'b2' log should still be there..
    TR_ASSERT(t, mysink->LastItem().string == std::string("from log b2"));


    // Re-enable both
    Logger::EnableAllLoggers();
    TR_ASSERT(t, logA->IsEnabled() == true);
    TR_ASSERT(t, logB->IsEnabled() == true);

    logA->Debug("from log a4");
    TR_ASSERT(t, mysink->LastItem().string == std::string("from log a4"));
    logB->Debug("from log b4");
    TR_ASSERT(t, mysink->LastItem().string == std::string("from log b4"));

    return kTR_Pass;
}

//
// this tests the enter/leave functionality
// note: the indent stepping can't be changed (and not queried)
//
DLL_EXPORT int test_logger_enterleave(ITesting *t) {
    auto log = Logger::GetLogger("logEnterLeave");
    auto defaultIndent = log->GetIndent();
    size_t lenHeaderNoIndent = 0;

    // Make sure we start at zero..
    TR_ASSERT(t, defaultIndent == 0);

    {
        log->Debug("message with indent 0");
        auto &item = mysink->LastItem();
        printf("Item:\n  Hdr: '%s'\n  Str: '%s'\n", item.header.c_str(), item.string.c_str());

        lenHeaderNoIndent = item.header.length();
    }

    {
        // Note: indent is added at the END of the header
        log->Enter();
        TR_ASSERT(t, log->GetIndent() == 2);

        log->Debug("message with indent 2");    // MUST HAVE SAME LENGTH AS FIRST STRING!!!
        auto &item = mysink->LastItem();
        printf("Item:\n  Hdr: '%s'\n  Str: '%s'\n", item.header.c_str(), item.string.c_str());
        auto lenHeader = item.header.length();
        TR_ASSERT(t, lenHeader == (lenHeaderNoIndent+2));

        // Enter again
        log->Enter();
        TR_ASSERT(t, log->GetIndent() == 4);

        log->Debug("message with indent 2");    // MUST HAVE SAME LENGTH AS FIRST STRING!!!
        auto &itemTwice = mysink->LastItem();
        printf("Item:\n  Hdr: '%s'\n  Str: '%s'\n", itemTwice.header.c_str(), itemTwice.string.c_str());
        auto lenHeaderTwice = itemTwice.header.length();
        TR_ASSERT(t, lenHeaderTwice == (lenHeaderNoIndent+4));
        log->Leave();   // back to '2'
        TR_ASSERT(t, log->GetIndent() == 2);

        log->Leave();
        TR_ASSERT(t, log->GetIndent() == 0);
        log->Debug("message with indent 0");    // MUST HAVE SAME LENGTH AS FIRST STRING!!!
        auto &itemafterleave = mysink->LastItem();
        printf("Item:\n  Hdr: '%s'\n  Str: '%s'\n", itemafterleave.header.c_str(), itemafterleave.string.c_str());

        // we should now be 4 wider
        // and the string after leave should be as it was before..
        TR_ASSERT(t, itemafterleave.header.length() == (lenHeaderNoIndent));
    }

    log->SetIndent(defaultIndent);

    return kTR_Pass;
}

DLL_EXPORT int test_logger_indent(ITesting *t) {
    auto log = Logger::GetLogger("logIndent");
    if (log->GetIndent() != 0) {
        log->SetIndent(0);
    }

    // Make sure we start at zero..
    TR_ASSERT(t, log->GetIndent() == 0);
    log->Debug("message indent 0");
    auto &topMessage = mysink->LastItem();

    log->SetIndent(6);  // this does not change the indent level before we call 'enter'
    log->Enter();               // which will add '2'
    log->Debug("message indent 8");
    auto &i8msg = mysink->LastItem();
    TR_ASSERT(t, (topMessage.header.length() + 8) == i8msg.header.length());


    while(log->GetIndent() > 0) {
        log->Leave();
        auto indentOffset = log->GetIndent();
        log->Debug("message indent %d", indentOffset);
        auto &indentMsg = mysink->LastItem();

        auto topLen = topMessage.header.length();
        auto indentLen = indentMsg.header.length();

        TR_ASSERT(t, (topMessage.header.length() + indentOffset) == indentMsg.header.length());
    }
    TR_ASSERT(t, log->GetIndent() == 0);


    return kTR_Pass;
}
