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
#include <mutex>
#include <vector>

#include <AppletConnection.h>
#include <EseTransportUtils.h>

using ::android::hardware::secure_element::V1_0::SecureElementStatus;
using ::android::hardware::secure_element::V1_0::LogicalChannelResponse;
using android::base::StringPrintf;

se_transport::AppletConnection* g_AppClient = nullptr;

#define NUM_OF_SIGNALS 6
int handledSignals[] = {SIGTRAP, SIGFPE, SIGILL, SIGSEGV, SIGBUS, SIGTERM};
struct sigaction old_action[NUM_OF_SIGNALS] = {};

namespace se_transport {
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

/* Handle Fatal signals to close opened Logical channel before
 * the process dies
 */
void signalHandler(int sig, siginfo_t* info, void* ucontext) {
    LOG(WARNING) << "received signal " << sig;
    // close the opened channel ,if there is any
    g_AppClient->close();

    // default handling of the received signal
    for (int i = 0; i < NUM_OF_SIGNALS; i++) {
        if (handledSignals[i] == sig) {
            if (old_action[i].sa_sigaction != nullptr) {
                LOG(INFO) << "execute originally installed handler" << sig;
                (*(old_action[i].sa_sigaction))(sig, info, ucontext);
            } else if (old_action[i].sa_handler == SIG_DFL || old_action[i].sa_handler == SIG_IGN) {
                LOG(INFO) << "This signal had a default beahvior or should be ignored " << sig;
                signal(sig, old_action[i].sa_handler);  // reset to old handler
                raise(sig);
            }
            break;
        }
    }
}

void installHandler() {
    for (int i = 0; i < NUM_OF_SIGNALS; i++) {
        struct sigaction enable_act = {};
        enable_act.sa_sigaction = signalHandler;
        enable_act.sa_flags = SA_RESTART | SA_SIGINFO | SA_ONSTACK;
        if (sigaction(handledSignals[i], &enable_act, &(old_action[i])) != 0) {
            LOG(ERROR) << "Unable to set up signal handler for signal " << handledSignals[i]
                       << "errno " << errno;
        }
    }
}

AppletConnection::AppletConnection(const std::vector<uint8_t>& aid) : kAppletAID(aid) {
    g_AppClient = this;
    installHandler();
}

bool AppletConnection::connectToSEService() {
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

bool AppletConnection::openChannelToApplet(std::vector<uint8_t>& resp) {
    bool ret = true;
    if(mCallback == nullptr || !mCallback->isClientConnected()) {
      // SE HAL crashed in between re-connect
      mSEClient = nullptr;
      mOpenChannel = -1;
      if(!connectToSEService()) {
        LOG(ERROR) << "Not connected to eSE Service";
        return false;
      }
    }
    if (isChannelOpen()) {
        //close();
        LOG(INFO) << "channel Already opened";
        return true;
    }
      mSEClient->openLogicalChannel(kAppletAID, 00,
          [&](LogicalChannelResponse selectResponse, SecureElementStatus status) {
          if (status == SecureElementStatus::SUCCESS) {
          resp = selectResponse.selectResponse;
          mOpenChannel = selectResponse.channelNumber;
          }else {
          ret = false;
          }
          LOG(INFO) << "openLogicalChannel:" << toString(status) << " channelNumber =" <<                                                                            ::android::hardware::toString(selectResponse.channelNumber) << " " << selectResponse.selectResponse;
          });
    return ret;
}

bool AppletConnection::transmit(std::vector<uint8_t>& CommandApdu , std::vector<uint8_t>& output){
    hidl_vec<uint8_t> cmd = CommandApdu;
    cmd[0] |= mOpenChannel ;
    LOGD_OMAPI("Channel number " << ::android::hardware::toString(mOpenChannel));

    if (mSEClient == nullptr) return false;

    mSEClient->transmit(cmd, [&](hidl_vec<uint8_t> result) {
        output = result;
        LOG(INFO) << "recieved response size = " << ::android::hardware::toString(result.size()) << " data = " << result;
    });
    return true;
}

bool AppletConnection::close() {
    LOGD_OMAPI("Enter");
    std::lock_guard<std::mutex> lock(channel_mutex_);
    if (mSEClient == nullptr) {
         LOG(ERROR) << "Channel couldn't be closed mSEClient handle is null";
         return false;
    }
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
