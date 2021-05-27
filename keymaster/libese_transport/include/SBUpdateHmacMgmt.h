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

#ifndef SBUPDATEHMACMGMT_H_
#define SBUPDATEHMACMGMT_H_
#include <IntervalTimer.h>
#include <vector>

#define INS_GET_HMAC_SHARING_PARAM (0x2D)  // command INS value for HMAC paramsharing
#define HMAC_RETRIGGER_POLL_TIMER (5000)  // 5 seconds poll to check and retrigger HMAC sharing

namespace se_transport {
class SBUpdateHmacMgmt {
 private:
  bool isHMACSharingrequired;  // to indicate HMAC re-sharing required or not

  IntervalTimer mTimer;  // timer for HMAC re-sharing with given timeout value

 public:
  bool isSessionOpen;  // flag to indicate whether channel session open/close

  /**
   * SB update management constructor initialization.
   * Initializes the class variabled to default values
   */
  SBUpdateHmacMgmt() : isHMACSharingrequired(false), isSessionOpen(false) {}
  /**
   * Below function performs following operations
   * 1) Identifies whether SB update session is in progress
   * 2) Is HMAC sharing requested while SB update in progress
   * 3) Set HMAC sharing required flag &
   *    start timer(ex:- with 5 secs time period) to trigger HMAC sharing
   * 4) Clear HMAC sharing flag when SB update completed & HMAC ins command
   *    received
   */
  bool checkAndTriggerHMACReshare(std::vector<uint8_t>& CommandApdu,
                                  std::vector<uint8_t>& output, bool isUpdateSession,
                                  bool isCommandAllow);
  /**
   * To update the current channel state open or closed
   * called by AppletConnection class to update channel state
   */
  void updateChannelState(bool isOpen);
  /**
   * Function is used to start/stop timer
   * based on the isStart input
   */
  void StartStopHMACTimer(bool isStart);
};
}  // namespace se_transport
#endif /* SBUPDATEHMACMGMT_H_ */
