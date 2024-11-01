#include <openssl/ec.h>
#include <ossl.h>


extern "C" int EC_KEY_set_private_key(EC_KEY *key, const BIGNUM *priv_key) {
  return ossl.ossl_EC_KEY_set_private_key(key, priv_key);
}
