#include "ApplicationMessages.hpp"
#include "MessageDispatcherDummy.hpp"
#include "MessageDispatcherPassthru.hpp"

DEFINE_MESSAGE_DISPATCHER_DUMMY(LedCommand);

DEFINE_MESSAGE_DISPATCHER_PASSTHRU(AdcStatus);
