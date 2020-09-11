#define LOG_TAG "OmapiTransport"

#include <android-base/logging.h>
#include <android-base/stringprintf.h>
#include <thread>
#include <Transport.h>
#include <android-base/properties.h>

#include <android/hardware/secure_element/1.0/ISecureElement.h>
#include <android/hardware/secure_element/1.0/ISecureElementHalCallback.h>
#include <android/hardware/secure_element/1.0/types.h>

#include "TestTransport.h"
//#include "NxpTestHelpers.h"

using ::android::hardware::secure_element::V1_0::ISecureElement;
using ::android::hardware::secure_element::V1_0::ISecureElementHalCallback;
using ::android::hardware::secure_element::V1_0::SecureElementStatus;
using ::android::hardware::secure_element::V1_0::LogicalChannelResponse;
using ::android::hardware::Return;
using ::android::hardware::Void;
using ::android::sp;
using android::hardware::hidl_vec;
using android::base::StringPrintf;

namespace nxp {
namespace se_transport {

#define APDU_DATA \
    {0x80,0xEE,0x00,0x00,0x06,0x01,0x02,0x03,0x04,0x05,0x06,0x06}

static std::unique_ptr<se_transport::TransportFactory> pTransportFactory = nullptr;

static inline std::unique_ptr<se_transport::TransportFactory>& getTransportFactoryInstance() {
  if(pTransportFactory == nullptr) {
     pTransportFactory = std::unique_ptr<se_transport::TransportFactory>(new se_transport::TransportFactory(
          android::base::GetBoolProperty("ro.kernel.qemu", false)));
      pTransportFactory->openConnection();
    }
    return pTransportFactory;
}
void eSEThread() {
  LOG(INFO) << "New Thread started ";
  std::vector<uint8_t> apdu = APDU_DATA;
  std::vector<uint8_t> response;
  if(!getTransportFactoryInstance()->sendData(apdu.data(), apdu.size(), response)) {
    LOG(INFO) << "Error in communicateToEse";
  }else {
    LOG(INFO) << "Sent data successfully to eSE";
  }
  LOG(INFO) << "Thread completed  , now waiting !!";
  while(true){
    sleep(5);
  }
}
void communicateToEse(){
  std::thread thr(eSEThread);
  thr.detach();
}

} // namespace se_transport
} // namespace nxp
