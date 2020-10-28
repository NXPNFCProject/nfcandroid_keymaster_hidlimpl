/*
 **
 ** Copyright 2020, The Android Open Source Project
 **
 ** Licensed under the Apache License, Version 2.0 (the "License");
 ** you may not use this file except in compliance with the License.
 ** You may obtain a copy of the License at
 **
 **     http://www.apache.org/licenses/LICENSE-2.0
 **
 ** Unless required by applicable law or agreed to in writing, software
 ** distributed under the License is distributed on an "AS IS" BASIS,
 ** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 ** See the License for the specific language governing permissions and
 ** limitations under the License.
 */
/******************************************************************************
 **
 ** The original Work has been changed by NXP.
 **
 ** Licensed under the Apache License, Version 2.0 (the "License");
 ** you may not use this file except in compliance with the License.
 ** You may obtain a copy of the License at
 **
 ** http://www.apache.org/licenses/LICENSE-2.0
 **
 ** Unless required by applicable law or agreed to in writing, software
 ** distributed under the License is distributed on an "AS IS" BASIS,
 ** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 ** See the License for the specific language governing permissions and
 ** limitations under the License.
 **
 ** Copyright 2020 NXP
 **
 *********************************************************************************/
#define LOG_TAG "OmapiTransport"

#include <vector>
#include <signal.h>
#include <android-base/logging.h>
#include <android-base/stringprintf.h>
#include <iomanip>

#include <Transport.h>
#include <EseTransportUtils.h>
#include <IntervalTimer.h>

namespace se_transport {
void SessionTimerFunc(union sigval arg){
     LOG(INFO) << "Session Timer expired !!";
     OmapiTransport *obj = (OmapiTransport*)arg.sival_ptr;
     if(obj != nullptr)
       obj->closeConnection();
}
bool OmapiTransport::openConnection() {
    LOG(INFO) << __PRETTY_FUNCTION__;
	  return mAppletConnection.connectToSEService();
}

bool OmapiTransport::sendData(const uint8_t* inData, const size_t inLen, std::vector<uint8_t>& output) {
    bool status = false;
    std::vector<uint8_t> cApdu(inData, inData+inLen);
    LOG(INFO) << __FUNCTION__;
#ifdef INTERVAL_TIMER
     LOG(INFO) << "stop the timer";
     mTimer.kill();
#endif
    if (!mAppletConnection.isChannelOpen()) {
       std::vector<uint8_t> selectResponse;
       status = mAppletConnection.openChannelToApplet(selectResponse);
       if(!status) {
          LOG(ERROR) << " Failed to open Logical Channel ,response " << selectResponse;
          output = selectResponse;
          return false;
       }
    }
    status = mAppletConnection.transmit(cApdu,output);
#ifdef INTERVAL_TIMER
     LOG(INFO) << "Set the timer";
     mTimer.set(SESSION_TIMEOUT,this, SessionTimerFunc);
#endif
    return status;
}

bool OmapiTransport::closeConnection() {
    LOG(INFO) << __PRETTY_FUNCTION__;
    return mAppletConnection.close();
}

bool OmapiTransport::isConnected() {
    LOG(INFO) << __PRETTY_FUNCTION__;
    return mAppletConnection.isChannelOpen();
}
} // namespace se_transport
