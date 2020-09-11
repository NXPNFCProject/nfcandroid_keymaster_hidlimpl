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
#include "Transport.h"
#include <android-base/logging.h>
#include <android-base/stringprintf.h>
#include <iomanip>

namespace nxp {
namespace se_transport {

std::ostream& operator<<(std::ostream& os, const std::vector<uint8_t>& vec) {
  std::ios_base::fmtflags flags(os.flags());
  os << "{ ";
  for (uint8_t c : vec) os <<std::setfill('0')<<std::hex<< std::uppercase << std::setw(2)<<(0xFF & c);
  os.flags(flags);
  os << " }";
  return os;
}

bool OmapiTransport::openConnection() {
  LOG(INFO) << __PRETTY_FUNCTION__;
	return mAppletConnection.connectToSEService();
}

bool OmapiTransport::sendData(const uint8_t* inData, const size_t inLen, std::vector<uint8_t>& output) {
    bool status = false;
    std::vector<uint8_t> cApdu(inData, inData+inLen);
    LOG(INFO) << __PRETTY_FUNCTION__;
    if (!mAppletConnection.isChannelOpen()) {
       std::vector<uint8_t> selectResponse;
       status = mAppletConnection.openChannelToApplet(selectResponse);
       if(!status) {
          LOG(ERROR) << " Failed to open Logical Channel , return response " << selectResponse;
          output = selectResponse;
          return false;
       }
    }
    status = mAppletConnection.transmit(cApdu,output);
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
} // namespace nxp
