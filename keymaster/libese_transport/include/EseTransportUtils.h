 /*
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
 */
#ifndef __ESE_TRANSPORT_CONFIG__
#define __ESE_TRANSPORT_CONFIG__
#include <vector>

namespace se_transport {

#define MAX_GET_SERVICE_RETRY 10
#define ONE_SEC  1000*1000*1
#define SESSION_TIMEOUT 30*1000 // 30 secs
// Helper method to dump vector contents
std::ostream& operator<<(std::ostream& os, const std::vector<uint8_t>& vec);

} // namespace se_transport
#endif /* __ESE_TRANSPORT_CONFIG__ */