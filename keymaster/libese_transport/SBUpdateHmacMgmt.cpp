/******************************************************************************
 *
 *  Copyright 2021 NXP
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 ******************************************************************************/
#define LOG_TAG "OmapiTransport"

#include <android-base/logging.h>
#include <android-base/stringprintf.h>
#include <log/log.h>
#include <vector>

#include <EseTransportUtils.h>
#include <SBUpdateHmacMgmt.h>
#include <cutils/properties.h>

/**
 * This is keymaster HAL vendor property for triggering
 * wait_for_keymaster operation which will implicitly
 * request HMAC sharing on setting this property value
 * "true"
 */
const char* km_prop = "vendor.strongbox.triggerhmac";

namespace se_transport {
#ifdef HMAC_RETRIGGER_REQUIRED

void request_wait_for_keymaster_opertaion() {
  LOG(INFO) << "Trigger Wait_for_keymaster!!";
  if (property_set(km_prop, "true") != 0) {
    LOG(ERROR) << "Error in setting vendor.km.wfk: %s" << __func__;
  }
  // wait for operation completion
  usleep(100 * 1000);
  if (property_set(km_prop, "false") != 0) {
    LOG(ERROR) << "Error in resetting vendor.km.wfk : %s" << __func__;
  }
}

static void HMACTimerFunc(union sigval arg) {
  LOG(INFO) << "HMACTimerFunc Timer expired !!";

  SBUpdateHmacMgmt* obj = (SBUpdateHmacMgmt*)arg.sival_ptr;
  if (obj->isSessionOpen) {
    LOG(INFO) << "Restart HMAC Timer !!!";
    obj->StartStopHMACTimer(true);
  } else {
    LOG(INFO) << "Trigger HMAC sharing!!!";
    request_wait_for_keymaster_opertaion();
  }
}

void SBUpdateHmacMgmt::StartStopHMACTimer(bool isStart) {
  mTimer.kill();

  if (isStart) mTimer.set(HMAC_RETRIGGER_POLL_TIMER, this, HMACTimerFunc);
}

void SBUpdateHmacMgmt::updateChannelState(bool isOpen) { isSessionOpen = isOpen; }

bool SBUpdateHmacMgmt::checkAndTriggerHMACReshare(std::vector<uint8_t>& CommandApdu,
                                                  std::vector<uint8_t>& output,
                                                  bool isUpdateSession, bool isCommandAllow) {
  bool stat = false;

  if (isUpdateSession && CommandApdu[APDU_INS_OFFSET] == INS_GET_HMAC_SHARING_PARAM) {
    isHMACSharingrequired = true;
    StartStopHMACTimer(true);
    LOG(INFO) << "HMAC re-sharing scenario detected!!!";
    stat = true;
  } else if (!isUpdateSession && isHMACSharingrequired &&
             CommandApdu[APDU_INS_OFFSET] == INS_GET_HMAC_SHARING_PARAM) {
    isHMACSharingrequired = false;
    StartStopHMACTimer(false);
    LOG(INFO) << "HMAC re-sharing triggered, completed!!!";
    stat = true;
  } else if (!isUpdateSession && isHMACSharingrequired && !isCommandAllow) {
    output.clear();
    output.push_back(0xFF);
    output.push_back(0xFF);
    request_wait_for_keymaster_opertaion();
    stat = false;
  } else {
    // Allow all command Ins
  }
  return stat;
}

#endif
}  // namespace se_transport
