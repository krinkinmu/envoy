#include <openssl/ec.h>
#include <ossl.h>


extern "C" void EC_POINT_free(EC_POINT *point) {
  return ossl.ossl_EC_POINT_free(point);
}
