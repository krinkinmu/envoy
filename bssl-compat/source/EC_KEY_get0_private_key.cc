#include <openssl/ec.h>
#include <ossl.h>


extern "C" const BIGNUM *EC_KEY_get0_private_key(const EC_KEY *key) {
  return ossl.ossl_EC_KEY_get0_private_key(key);
}
