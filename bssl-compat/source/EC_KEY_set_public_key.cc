#include <openssl/ec_key.h>
#include <ossl.h>


extern "C" int EC_KEY_set_public_key(EC_KEY *key, const EC_POINT *pub) {
  return ossl.ossl_EC_KEY_set_public_key(key, pub);
}
