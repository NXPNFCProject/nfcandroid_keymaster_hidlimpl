/*
**
** Copyright 2018, The Android Open Source Project
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
 ** Copyright 2020-2021 NXP
 **
 *********************************************************************************/
#define LOG_TAG "OmapiTransport"

#include <android-base/logging.h>
#include <android-base/stringprintf.h>
#include <log/log.h>
#include <signal.h>
#include <iomanip>
#include <map>
#include <mutex>
#include <string>
#include <vector>

#include <AppletConnection.h>
#include <EseTransportUtils.h>
#include <SignalHandler.h>

using ::android::hardware::secure_element::V1_0::SecureElementStatus;
using ::android::hardware::secure_element::V1_0::LogicalChannelResponse;
using android::base::StringPrintf;


namespace se_transport {

const std::map<uint8_t, std::string> allowedCmdIns = {
    {0x06, "INS_SET_BOOT_PARAMS"},     {0x09, "INS_SET_VERSION_PATCHLEVEL"},
    {0x2A, "INS_COMPUTE_SHARED_HMAC"}, {0x2D, "INS_GET_HMAC_SHARING_PARAM"},
    {0x2F, "INS_GET_HW_INFO"},         {0x35, "INS_EARLY_BOOT_ENDED"}};

class SecureElementCallback : public ISecureElementHalCallback {
 public:
    Return<void> onStateChange(bool state) override {
        mSEClientState = state;
        return Void();
    };
    Return<void> onStateChange_1_1(bool state, const hidl_string& reason) override {
        LOGD_OMAPI("connected =" << (state?"true " : "false " ) << "reason: " << reason);
        mSEClientState = state;
        return Void();
    };
    bool isClientConnected() {
        return mSEClientState;
    }
 private:
    bool mSEClientState = false;
};

sp<SecureElementCallback> mCallback = nullptr;

class SEDeathRecipient : public android::hardware::hidl_death_recipient {
  virtual void serviceDied(uint64_t /*cookie*/, const android::wp<::android::hidl::base::V1_0::IBase>& /*who*/) {
    LOG(ERROR) << "Secure Element Service died disconnecting SE HAL .....";
    if(mCallback != nullptr) {
      LOG(INFO) << "Changing state to disconnect ...";
      mCallback->onStateChange(false);// Change state to disconnect
    }
  }
};

sp<SEDeathRecipient> mSEDeathRecipient = nullptr;

AppletConnection::AppletConnection(const std::vector<uint8_t>& aid)
    : kAppletAID(aid), isUpdateSession(false) {}

bool AppletConnection::isCommandAllowed(uint8_t cmdIns) {
  std::map<uint8_t, std::string>::const_iterator it = allowedCmdIns.find(cmdIns);
  // If command is not present in allow list
  if (it == allowedCmdIns.end()) {
    return false;
  }
  return true;
}

bool AppletConnection::isCommandAllowedForUpdateSession(uint8_t cmdIns) {
  // If update session ongoing or if skipUpdatesession true
  if (isUpdateSession) {
    if (!isCommandAllowed(cmdIns)) {
      return false;
    }
  }
  return true;
}

bool AppletConnection::connectToSEService() {
    if (!SignalHandler::getInstance()->isHandlerRegistered()) {
        LOG(INFO) << "register signal handler";
        SignalHandler::getInstance()->installHandler(this);
    }
    if (mSEClient != nullptr && mCallback->isClientConnected()) {
        LOG(INFO) <<"Already connected";
        return true;
    }

    uint8_t retry = 0;
    bool status = false;
    while (( mSEClient == nullptr ) && retry++ < MAX_GET_SERVICE_RETRY ){ // How long should we try before giving up !
      mSEClient = ISecureElement::tryGetService("eSE1");

      if(mSEClient == nullptr){
        LOG(ERROR) << "failed to get eSE HAL service : retry after 1 sec , retry cnt = " << android::hardware::toString(retry) ;
      }else {
        LOG(INFO) << " !!! SuccessFully got Handle to eSE HAL service" ;
        if (mCallback == nullptr) {
          mCallback = new SecureElementCallback();
        }
        mSEDeathRecipient = new SEDeathRecipient();
        mSEClient->init_1_1(mCallback);
        mSEClient->linkToDeath(mSEDeathRecipient, 0/*cookie*/);
        status = mCallback->isClientConnected();
        break;
      }
      usleep(ONE_SEC);
    }
    return status;
}

bool AppletConnection::selectSBApplet(std::vector<uint8_t>& resp, uint8_t p2) {
  bool stat = false;
  mSEClient->openLogicalChannel(
      kAppletAID, p2, [&](LogicalChannelResponse selectResponse, SecureElementStatus status) {
        if (status == SecureElementStatus::SUCCESS) {
          resp = selectResponse.selectResponse;
          mOpenChannel = selectResponse.channelNumber;
          stat = true;
          // Upgrade ongoing shall allow only specific commands
          if ((resp[resp.size() - UPGRADE_OFFSET_SW] & UPGRADE_MASK_BIT) == UPGRADE_MASK_VAL) {
            isUpdateSession = true;
          }
          LOG(INFO) << "openLogicalChannel:" << toString(status) << " channelNumber ="
                    << ::android::hardware::toString(selectResponse.channelNumber) << " "
                    << selectResponse.selectResponse;
        }
      });
  return stat;
}

bool AppletConnection::openChannelToApplet(std::vector<uint8_t>& resp) {
  bool ret = false;
  uint8_t retry = 0;
  if (mCallback == nullptr || !mCallback->isClientConnected()) {
    mSEClient = nullptr;
    mOpenChannel = -1;
    if (!connectToSEService()) {
      LOG(ERROR) << "Not connected to eSE Service";
      return ret;
    }
  }
  if (isChannelOpen()) {
    LOG(INFO) << "channel Already opened";
    return true;
  }
  do {
    if (selectSBApplet(resp, SELECT_P2_VALUE_0) || selectSBApplet(resp, SELECT_P2_VALUE_2)) {
      ret = true;
#ifdef HMAC_RETRIGGER_REQUIRED
      mSBUpdateconn.updateChannelState(true);
#endif
      break;
    }
    LOG(INFO) << " openChannelToApplet retry after 2 secs";
    usleep(2 * ONE_SEC);
  } while (retry++ < MAX_RETRY_COUNT);

  return ret;
}

bool AppletConnection::transmit(std::vector<uint8_t>& CommandApdu , std::vector<uint8_t>& output){
    hidl_vec<uint8_t> cmd = CommandApdu;
    cmd[0] |= mOpenChannel ;
    LOGD_OMAPI("Channel number " << ::android::hardware::toString(mOpenChannel));

    if (mSEClient == nullptr) return false;
    if (!isCommandAllowedForUpdateSession(CommandApdu[APDU_INS_OFFSET])) {
      LOG(ERROR) << "command Ins not allowed, " << CommandApdu[APDU_INS_OFFSET]
                 << " upgrade ongoing";
      output.clear();
      output.push_back(0xFF);
      output.push_back(0xFF);
      return false;
    }
#ifdef HMAC_RETRIGGER_REQUIRED
    else if (!mSBUpdateconn.checkAndTriggerHMACReshare(
                 CommandApdu, output, isUpdateSession,
                 isCommandAllowed(CommandApdu[APDU_INS_OFFSET]))) {
      return false;
    }
#endif
    // block any fatal signal delivery
    SignalHandler::getInstance()->blockSignals();

    mSEClient->transmit(cmd, [&](hidl_vec<uint8_t> result) {
        output = result;
        LOG(INFO) << "recieved response size = " << ::android::hardware::toString(result.size()) << " data = " << result;
    });

    // un-block signal delivery
    SignalHandler::getInstance()->unblockSignals();
    return true;
}

int AppletConnection::getSessionTimeout() {
  // session timer is while update ongoing
  return (isUpdateSession) ? UPGRADE_SESSION_TIMEOUT : SESSION_TIMEOUT;
}

bool AppletConnection::close() {
    LOGD_OMAPI("Enter");
    std::lock_guard<std::mutex> lock(channel_mutex_);
    isUpdateSession = false;
    if (mSEClient == nullptr) {
         LOG(ERROR) << "Channel couldn't be closed mSEClient handle is null";
         return false;
    }
#ifdef HMAC_RETRIGGER_REQUIRED
    mSBUpdateconn.updateChannelState(false);
#endif
    if(mOpenChannel < 0){
       LOG(INFO) << "Channel is already closed";
       return true;
    }
    SecureElementStatus status = mSEClient->closeChannel(mOpenChannel);
    if (status != SecureElementStatus::SUCCESS) {
        /*
         * reason could be SE reset or HAL deinit triggered from other client
         * which anyway closes all the opened channels
         * */
        LOG(ERROR) << "closeChannel failed";
        mOpenChannel = -1;
        return true;
    }
    LOG(INFO) << "Channel closed";
    mOpenChannel = -1;
    return true;
}

bool AppletConnection::isChannelOpen() {
    LOGD_OMAPI("Enter");
    std::lock_guard<std::mutex> lock(channel_mutex_);
    if(mCallback == nullptr || !mCallback->isClientConnected()) {
      return false;
    }
    return mOpenChannel >= 0;
}

}  // namespace se_transport
