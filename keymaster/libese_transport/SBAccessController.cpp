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
#define LOG_TAG "SBAccessController"

#include <android-base/logging.h>
#include <map>
#include <vector>

#include <EseTransportUtils.h>
#include <SBAccessController.h>

#define UPGRADE_OFFSET_SW 3    // upgrade offset from last in response
#define UPGRADE_MASK_BIT 0x02  // Update bit mask in upgrade byte
#define UPGRADE_MASK_VAL 0x02  // Update mask value in upgrade byte

namespace se_transport {

static bool mAccessAllowed = true;
// These should be in sync with JavacardKeymasterDevice41.cpp
// Whitelisted cmds
std::map<uint8_t, uint8_t> allowedCmdIns = {{0x09 /*INS_SET_VERSION_PATCHLEVEL*/, 0},
                                            {0x2A /*INS_COMPUTE_SHARED_HMAC*/, 0},
                                            {0x2D /*INS_GET_HMAC_SHARING_PARAM*/, 0},
                                            {0x2F /*INS_GET_HW_INFO*/, 0}};

static void AccessTimerFunc(union sigval arg) {
    (void)arg;  // unused
    mAccessAllowed = true;
}

void SBAccessController::StartAccessTimer(bool isStart) {
    mTimer.kill();
    if (!isStart) {
        mAccessAllowed = true;  // Full access
    } else {
        mAccessAllowed = false;  // No access or Limited access
        mTimer.set(SB_ACCESS_BLOCK_TIMER, this, AccessTimerFunc);
    }
}
void SBAccessController::parseResponse(std::vector<uint8_t>& responseApdu) {
    // check if StrongBox Applet update is underway
    if ((responseApdu[responseApdu.size() - UPGRADE_OFFSET_SW] & UPGRADE_MASK_BIT) ==
        UPGRADE_MASK_VAL) {
        mIsUpdateInProgress = true;
        LOG(INFO) << "StrongBox Applet update is in progress";
        StartAccessTimer(true);
    } else {
        mIsUpdateInProgress = false;
        StartAccessTimer(false);
    }
}
int SBAccessController::getSessionTimeout() {
    if (mIsUpdateInProgress) {
        return (mBootState == BOOTSTATE::SB_EARLY_BOOT_ENDED) ? SMALLEST_SESSION_TIMEOUT
                                                              : UPGRADE_SESSION_TIMEOUT;
    } else {
        return REGULAR_SESSION_TIMEOUT;
    }
}
bool SBAccessController::isSelectAllowed() {
    bool select_allowed = false;
    if (mAccessAllowed) {
        select_allowed = true;
    } else {
        switch (mBootState) {
            case BOOTSTATE::SB_EARLY_BOOT:
                select_allowed = true;
                break;
            case BOOTSTATE::SB_EARLY_BOOT_ENDED:
                break;
        }
    }
    if(!select_allowed)
        LOG(INFO) << "StrongBox Applet selection is not allowed";

    return select_allowed;
}
void SBAccessController::updateBootState() {
    // set the state to BOOT_ENDED once we have received
    // all whitelisted commands
    bool allCmdreceived = true;
    for (auto it = allowedCmdIns.begin(); it != allowedCmdIns.end(); it++) {
        if (it->second == 0) {
            allCmdreceived = false;
            break;
        }
    }
    if (allCmdreceived) {
        LOG(INFO) << "Early boot completed";
        mBootState = BOOTSTATE::SB_EARLY_BOOT_ENDED;
    }
}
bool SBAccessController::isOperationAllowed(uint8_t cmdIns) {
    bool op_allowed = false;
    if (mAccessAllowed) {
        op_allowed = true;
    } else {
        switch (mBootState) {
            case BOOTSTATE::SB_EARLY_BOOT: {
                auto it = allowedCmdIns.find(cmdIns);
                if (it != allowedCmdIns.end()) {
                    it->second = 1;  // cmd received
                    updateBootState();
                    op_allowed = true;
                }
            } break;
            case BOOTSTATE::SB_EARLY_BOOT_ENDED:
                break;
        }
    }
    if (cmdIns == INS_EARLY_BOOT_ENDED) {
        // allowed as this is sent by VOLD only during early boot
        op_allowed = true;
        mBootState = BOOTSTATE::SB_EARLY_BOOT_ENDED;
    }
    return op_allowed;
}
}  // namespace se_transport
