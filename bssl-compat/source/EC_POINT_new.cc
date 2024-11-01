#include <openssl/ec.h>
#include <ossl.h>


extern "C" EC_POINT *EC_POINT_new(const EC_GROUP *group) {
  return ossl.ossl_EC_POINT_new(group);
}
